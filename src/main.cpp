#include "io_adapter.hpp"
#include "market_router.hpp"

#include <boost/asio.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace asio = boost::asio;

static bool env_pretty_enabled() {
    const char* e = std::getenv("MATCHING_ENGINE_PRETTY");
    return e != nullptr && e[0] != '\0' && e[0] != '0';
}

static void run_sync_replay(me::MarketRouter& router, std::istream& in, bool pretty) {
    me::IoAdapter adapter(router);
    std::string line;
    while (std::getline(in, line)) {
        me::IoLineResult res = adapter.handle_line(std::move(line), pretty);
        std::cout << res.json << '\n';
        if (pretty && !res.display.empty()) {
            std::cout << res.display << '\n';
        }
    }
}

static void run_async_stdin(asio::io_context& ctx, me::MarketRouter& router, bool pretty) {
    auto adapter = std::make_shared<me::IoAdapter>(router);
    auto in = std::make_shared<asio::posix::stream_descriptor>(ctx, ::dup(STDIN_FILENO));
    auto buf = std::make_shared<asio::streambuf>();

    std::function<void()> read_more;
    read_more = [=]() {
        asio::async_read_until(*in, *buf, '\n',
            [=](const boost::system::error_code& ec, std::size_t) {
                if (ec) return;
                std::istream is(buf.get());
                std::string line;
                std::getline(is, line);
                me::IoLineResult res = adapter->handle_line(std::move(line), pretty);
                std::cout << res.json << '\n';
                if (pretty && !res.display.empty()) {
                    std::cout << res.display << '\n';
                }
                std::cout << std::flush;
                read_more();
            });
    };
    read_more();
}

static bool parse_pretty_flag(int argc, char* argv[], int& file_arg_index) {
    file_arg_index = -1;
    bool pretty = env_pretty_enabled();
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--pretty" || a == "-p") {
            pretty = true;
        } else if (a == "--json") {
            pretty = false;
        } else if (a == "--help" || a == "-h") {
            std::cerr << "Usage: matching_engine [--pretty|-p] [replay.txt]\n"
                      << "  --pretty / -p     tabulated book + fills (stdout)\n"
                      << "  --json            JSON lines only (default for tests)\n"
                      << "  MATCHING_ENGINE_PRETTY=1  enable pretty mode\n"
                      << "  Part 2: include \"symbol\" (BTC, CL, SILVER) for multi-market routing\n";
            std::exit(0);
        } else if (a[0] != '-') {
            file_arg_index = i;
        }
    }
    return pretty;
}

int main(int argc, char* argv[]) {
    me::MarketRouter router;
    asio::io_context ctx{1};

    int file_arg = -1;
    const bool pretty = parse_pretty_flag(argc, argv, file_arg);

    if (file_arg >= 0) {
        std::ifstream file(argv[file_arg]);
        if (!file) {
            std::cerr << "Cannot open " << argv[file_arg] << '\n';
            return 1;
        }
        run_sync_replay(router, file, pretty);
        return 0;
    }

    const char* input_file = std::getenv("INPUT_FILE");
    if (input_file != nullptr) {
        std::ifstream file(input_file);
        run_sync_replay(router, file, pretty);
        return 0;
    }

    std::cerr << "Matching Engine (JSON lines, multi-market). Use --pretty for tables. Ctrl+D to exit.\n";
    run_async_stdin(ctx, router, pretty);
    ctx.run();
    return 0;
}
