#include "io_adapter.hpp"
#include "cli_display.hpp"

namespace me {

namespace json = boost::json;

static json::array depth_levels_json(MatchingEngine& engine, Side side) {
    json::array arr;
    for (const auto& lv : engine.depth_snapshot(side)) {
        arr.push_back(json::object{
            {"level", lv.level_rank},
            {"price", ticks_to_price(lv.price_ticks)},
            {"price_ticks", lv.price_ticks},
            {"size", lv.total_size},
            {"orders", lv.order_count},
        });
    }
    return arr;
}

static void append_depth_levels(json::object& o, MatchingEngine& engine) {
    o["bid_levels"] = depth_levels_json(engine, Side::Buy);
    o["ask_levels"] = depth_levels_json(engine, Side::Sell);
}

static void append_fills(json::object& o, const HotResult& r) {
    json::array fills;
    for (std::size_t i = 0; i < r.fill_count; ++i) {
        const Fill& f = r.fills[i];
        fills.push_back(json::object{
            {"maker_order_id", f.maker_order_id},
            {"taker_order_id", f.taker_order_id},
            {"size", f.size},
            {"price", ticks_to_price(f.price_ticks)},
            {"timestamp", f.timestamp},
        });
    }
    o["fills"] = fills;
}

static void append_book(json::object& o, const HotResult& r) {
    json::object book;
    if (r.has_bid) book["bid"] = ticks_to_price(r.best_bid_ticks);
    if (r.has_ask) book["ask"] = ticks_to_price(r.best_ask_ticks);
    o["book"] = book;
}

static void append_router_meta(json::object& o, const RoutedResult& rr) {
    if (!rr.named_market || rr.shard == nullptr) return;
    o["symbol"] = rr.shard->symbol();
    o["event_seq"] = rr.shard->last_event_seq();
}

static const char* event_kind_str(MarketEventKind k) {
    switch (k) {
    case MarketEventKind::Place: return "place";
    case MarketEventKind::Cancel: return "cancel";
    case MarketEventKind::Query: return "query";
    case MarketEventKind::Fill: return "fill";
    }
    return "unknown";
}

static json::array events_to_json(const std::vector<MarketEvent>& events) {
    json::array arr;
    for (const MarketEvent& e : events) {
        json::object o{
            {"seq", e.seq},
            {"kind", event_kind_str(e.kind)},
            {"ok", e.ok},
        };
        if (e.order_id != 0) o["order_id"] = e.order_id;
        if (e.maker_order_id != 0) o["maker_order_id"] = e.maker_order_id;
        if (e.taker_order_id != 0) o["taker_order_id"] = e.taker_order_id;
        if (e.size != 0) o["size"] = e.size;
        if (e.price_ticks != 0) o["price_ticks"] = e.price_ticks;
        arr.push_back(o);
    }
    return arr;
}

static std::string parse_symbol(const json::object& obj, bool& named) {
    if (obj.contains("symbol")) {
        named = true;
        return json::value_to<std::string>(obj.at("symbol"));
    }
    named = false;
    return {};
}

IoLineResult IoAdapter::handle_line(std::string line, bool pretty) {
    IoLineResult out_line;
    json::object out;
    HotResult hot{};
    RoutedResult routed{};
    std::string cmd;
    std::string query_type;
    DisplayContext disp_ctx{};
    disp_ctx.ladder_depth = 10;
    bool named_market = false;

    try {
        if (line.empty() || line[0] == '#') {
            out["ok"] = true;
            out["skipped"] = true;
            out_line.json = json::serialize(out);
            return out_line;
        }

        json::value jv = json::parse(line);
        const json::object& obj = jv.as_object();
        cmd = json::value_to<std::string>(obj.at("cmd"));
        disp_ctx.command = cmd;

        const std::string symbol = parse_symbol(obj, named_market);
        if (named_market) {
            disp_ctx.symbol = symbol;
        }

        if (cmd == "order") {
            PlaceRequest req{};
            req.account_id = static_cast<uint64_t>(obj.at("account_id").as_int64());
            const std::string side_s = json::value_to<std::string>(obj.at("side"));
            req.side = (side_s == "buy") ? Side::Buy : Side::Sell;
            req.size = static_cast<uint64_t>(obj.at("size").as_int64());
            const std::string tif_s = json::value_to<std::string>(obj.at("tif"));
            req.tif = (tif_s == "GTC") ? TimeInForce::GTC : TimeInForce::IOC;
            if (obj.contains("price") && !obj.at("price").is_null()) {
                req.limit_ticks = price_to_ticks(obj.at("price").as_double());
            }
            if (obj.contains("post_only") && obj.at("post_only").is_bool()) {
                req.post_only = obj.at("post_only").as_bool();
            }
            routed = router_.place_order(symbol, std::move(req));
            hot = routed.hot;
            out["ok"] = hot.accepted && !hot.rejected_self_trade && !hot.rejected_post_only;
            if (hot.rejected_self_trade) out["error"] = "self_trade";
            if (hot.rejected_post_only) out["error"] = "post_only_would_cross";
            if (hot.placed_order_id != 0) out["order_id"] = hot.placed_order_id;
            else if (hot.resting_order_id) out["order_id"] = *hot.resting_order_id;
            append_fills(out, hot);
            append_book(out, hot);
            append_depth_levels(out, routed.shard->engine());
            append_router_meta(out, routed);
        } else if (cmd == "cancel") {
            const uint64_t oid = static_cast<uint64_t>(obj.at("order_id").as_int64());
            routed = router_.cancel_order(symbol, oid);
            hot = routed.hot;
            out["ok"] = hot.accepted;
            append_book(out, hot);
            append_depth_levels(out, routed.shard->engine());
            append_router_meta(out, routed);
        } else if (cmd == "query") {
            query_type = json::value_to<std::string>(obj.at("type"));
            disp_ctx.query_type = query_type;
            if (query_type == "top") {
                routed = router_.query_top(symbol);
                hot = routed.hot;
                out["ok"] = true;
                append_book(out, hot);
                append_depth_levels(out, routed.shard->engine());
                append_router_meta(out, routed);
            } else if (query_type == "full") {
                disp_ctx.show_all_levels = true;
                routed = router_.query_top(symbol);
                hot = routed.hot;
                out["ok"] = true;
                out["bids"] = depth_levels_json(routed.shard->engine(), Side::Buy);
                out["asks"] = depth_levels_json(routed.shard->engine(), Side::Sell);
                append_book(out, hot);
                append_router_meta(out, routed);
            } else if (query_type == "order") {
                const uint64_t oid = static_cast<uint64_t>(obj.at("order_id").as_int64());
                routed = router_.query_order(symbol, oid);
                hot = routed.hot;
                out["ok"] = hot.accepted;
                if (hot.query_remaining) out["remaining"] = *hot.query_remaining;
                if (hot.query_price_ticks) out["price"] = ticks_to_price(*hot.query_price_ticks);
                append_book(out, hot);
                append_depth_levels(out, routed.shard->engine());
                for (const auto& row : routed.shard->engine().book_orders()) {
                    if (row.order_id == oid) {
                        out["price_level"] = row.price_level;
                        out["price_level_price"] = ticks_to_price(row.price_ticks);
                        out["side"] = (row.side == Side::Buy) ? "buy" : "sell";
                        break;
                    }
                }
                append_router_meta(out, routed);
            } else if (query_type == "events") {
                disp_ctx.show_book_tables = false;
                if (!named_market) {
                    out["ok"] = false;
                    out["error"] = "symbol_required";
                } else {
                    uint64_t from_seq = 1;
                    std::size_t limit = 100;
                    if (obj.contains("from_seq")) {
                        from_seq = static_cast<uint64_t>(obj.at("from_seq").as_int64());
                    }
                    if (obj.contains("limit")) {
                        limit = static_cast<std::size_t>(obj.at("limit").as_int64());
                    }
                    MarketShard& ms = router_.shard(symbol);
                    const auto evs = ms.events_since(from_seq, limit);
                    out["ok"] = true;
                    out["symbol"] = ms.symbol();
                    out["events"] = events_to_json(evs);
                    out["event_seq"] = ms.last_event_seq();
                    routed.shard = &ms;
                    routed.named_market = true;
                    hot.accepted = true;
                }
            } else {
                out["ok"] = false;
                out["error"] = "unknown_query";
            }
        } else {
            out["ok"] = false;
            out["error"] = "unknown_cmd";
        }
    } catch (const std::exception& e) {
        out["ok"] = false;
        out["error"] = e.what();
    }

    out_line.json = json::serialize(out);

    if (pretty && !out.contains("skipped") && !cmd.empty() && routed.shard != nullptr) {
        disp_ctx.result = &hot;
        disp_ctx.response = &out;
        out_line.display = format_cli_display(routed.shard->engine(), disp_ctx);
    }

    return out_line;
}

} // namespace me
