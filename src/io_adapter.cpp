#include "io_adapter.hpp"
#include <sstream>

namespace me {

namespace json = boost::json;

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

std::string IoAdapter::handle_line(std::string line) {
    json::object out;
    try {
        if (line.empty() || line[0] == '#') {
            out["ok"] = true;
            out["skipped"] = true;
            return json::serialize(out);
        }

        json::value jv = json::parse(line);
        const json::object& obj = jv.as_object();
        const std::string cmd = json::value_to<std::string>(obj.at("cmd"));

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
            HotResult r = engine_.place_order(std::move(req));
            out["ok"] = r.accepted && !r.rejected_self_trade && !r.rejected_post_only;
            if (r.rejected_self_trade) out["error"] = "self_trade";
            if (r.rejected_post_only) out["error"] = "post_only_would_cross";
            if (r.placed_order_id != 0) out["order_id"] = r.placed_order_id;
            else if (r.resting_order_id) out["order_id"] = *r.resting_order_id;
            append_fills(out, r);
            append_book(out, r);
        } else if (cmd == "cancel") {
            const uint64_t oid = static_cast<uint64_t>(obj.at("order_id").as_int64());
            HotResult r = engine_.cancel_order(oid);
            out["ok"] = r.accepted;
            append_book(out, r);
        } else if (cmd == "query") {
            const std::string type = json::value_to<std::string>(obj.at("type"));
            if (type == "top") {
                HotResult r = engine_.query_top();
                out["ok"] = true;
                append_book(out, r);
            } else if (type == "full") {
                out["ok"] = true;
                json::array bids, asks;
                for (const auto& lv : engine_.depth_snapshot(Side::Buy)) {
                    bids.push_back(json::object{
                        {"price", ticks_to_price(lv.price_ticks)},
                        {"size", lv.total_size},
                        {"orders", lv.order_count},
                    });
                }
                for (const auto& lv : engine_.depth_snapshot(Side::Sell)) {
                    asks.push_back(json::object{
                        {"price", ticks_to_price(lv.price_ticks)},
                        {"size", lv.total_size},
                        {"orders", lv.order_count},
                    });
                }
                out["bids"] = bids;
                out["asks"] = asks;
                HotResult r = engine_.query_top();
                append_book(out, r);
            } else if (type == "order") {
                const uint64_t oid = static_cast<uint64_t>(obj.at("order_id").as_int64());
                HotResult r = engine_.query_order(oid);
                out["ok"] = r.accepted;
                if (r.query_remaining) out["remaining"] = *r.query_remaining;
                if (r.query_price_ticks) out["price"] = ticks_to_price(*r.query_price_ticks);
                append_book(out, r);
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
    return json::serialize(out);
}

} // namespace me
