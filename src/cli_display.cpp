#include "cli_display.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <tabulate/table.hpp>

namespace me {
namespace {

namespace json = boost::json;
using Table = tabulate::Table;

std::string fmt_price(int64_t ticks) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(2) << ticks_to_price(ticks);
    return os.str();
}

std::vector<MatchingEngine::DepthLevel> slice_levels(const std::vector<MatchingEngine::DepthLevel>& all,
                                                     std::size_t max_levels, bool show_all) {
    if (show_all || all.size() <= max_levels) return all;
    return std::vector<MatchingEngine::DepthLevel>(all.begin(), all.begin() + max_levels);
}

// BIDS | ASKS: bid price, bid qty, ask price, ask qty (resting liquidity per price level).
Table make_book_table(MatchingEngine& engine, std::size_t max_levels, bool show_all) {
    const auto bids = slice_levels(engine.depth_snapshot(Side::Buy), max_levels, show_all);
    const auto asks = slice_levels(engine.depth_snapshot(Side::Sell), max_levels, show_all);
    const std::size_t rows = std::max(bids.size(), asks.size());

    Table t;
    t.format().font_style({tabulate::FontStyle::bold});
    t.add_row({"Bid Price", "Bid Qty", "|", "Ask Price", "Ask Qty"});

    for (std::size_t i = 0; i < rows; ++i) {
        std::string bid_px, bid_sz, ask_px, ask_sz;
        if (i < bids.size()) {
            bid_px = fmt_price(bids[i].price_ticks);
            bid_sz = std::to_string(bids[i].total_size);
        } else {
            bid_px = bid_sz = "";
        }
        if (i < asks.size()) {
            ask_px = fmt_price(asks[i].price_ticks);
            ask_sz = std::to_string(asks[i].total_size);
        } else {
            ask_px = ask_sz = "";
        }
        t.add_row({bid_px, bid_sz, "|", ask_px, ask_sz});
    }
    if (rows == 0) {
        t.add_row({"—", "—", "|", "—", "—"});
    }
    return t;
}

Table make_fills_table(const HotResult& r) {
    Table t;
    t.format().font_style({tabulate::FontStyle::bold});
    t.add_row({"Maker ID", "Taker ID", "Size", "Price", "Timestamp"});
    for (std::size_t i = 0; i < r.fill_count; ++i) {
        const Fill& f = r.fills[i];
        std::ostringstream sz;
        sz << f.size;
        t.add_row({std::to_string(f.maker_order_id), std::to_string(f.taker_order_id), sz.str(),
                   fmt_price(f.price_ticks), std::to_string(f.timestamp)});
    }
    return t;
}

} // namespace

std::string format_cli_display(MatchingEngine& engine, const DisplayContext& ctx) {
    std::ostringstream out;
    const auto& r = *ctx.result;
    const auto& resp = *ctx.response;
    const bool show_all = ctx.show_all_levels;
    const bool is_query_top = ctx.command == "query" && ctx.query_type == "top";
    const bool is_query_full = ctx.command == "query" && ctx.query_type == "full";

    out << "\n";
    Table hdr;
    hdr.format().font_style({tabulate::FontStyle::bold});
    std::string status = resp.at("ok").as_bool() ? "OK" : "FAIL";
    if (resp.contains("error")) {
        status += " — " + json::value_to<std::string>(resp.at("error"));
    }
    hdr.add_row({"Command", ctx.command});
    hdr.add_row({"Status", status});
    if (resp.contains("order_id")) {
        hdr.add_row({"Order ID", std::to_string(resp.at("order_id").as_uint64())});
    }
    out << hdr << '\n';

    if (r.fill_count > 0) {
        out << "\n── Fills ──────────────────────────────────────\n";
        out << make_fills_table(r) << '\n';
    }

    if (ctx.command == "query" && ctx.query_type == "order") {
        out << "\n── Order Status ───────────────────────────────\n";
        Table ot;
        ot.add_row({"Field", "Value"});
        if (r.query_remaining) {
            ot.add_row({"Remaining", std::to_string(*r.query_remaining)});
            if (r.query_price_ticks) ot.add_row({"Price", fmt_price(*r.query_price_ticks)});
        } else {
            ot.add_row({"State", "not resting (filled, cancelled, or unknown)"});
        }
        out << ot << '\n';
    }

    // Top of book = best level only (one row).
    if (!is_query_full) {
        out << "\n── Top of Book (resting) ──────────────────────\n";
        out << make_book_table(engine, 1, false) << '\n';
    }

    // Book depth = all levels (or best N on normal commands).
    if (is_query_full) {
        out << "\n── Book Depth (resting) ───────────────────────\n";
        out << make_book_table(engine, ctx.ladder_depth, true) << '\n';
    } else if (!is_query_top) {
        out << "\n── Book Depth (resting, best " << ctx.ladder_depth << ") ─────────\n";
        out << make_book_table(engine, ctx.ladder_depth, false) << '\n';
    }

    return out.str();
}

} // namespace me
