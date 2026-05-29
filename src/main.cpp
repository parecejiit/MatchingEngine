#include "io_adapter.hpp"
#include "matching_engine.hpp"

#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include <string>

namespace asio = boost::asio;

static void run_sync_replay(me::MatchingEngine& engine, std::istream& in) {
    me::IoAdapter adapter(engine);
    std::string line;
    while (std::getline(in, line)) {
        std::cout << adapter.handle_line(std::move(line)) << '\n';
    }
}

static void run_async_stdin(asio::io_context& ctx, me::MatchingEngine& engine) {
    auto adapter = std::make_shared<me::IoAdapter>(engine);
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
                std::cout << adapter->handle_line(std::move(line)) << '\n' << std::flush;
                read_more();
            });
    };
    read_more();
}

int main(int argc, char* argv[]) {
    me::MatchingEngine engine;
    asio::io_context ctx{1};

    if (argc > 1) {
        std::ifstream file(argv[1]);
        if (!file) {
            std::cerr << "Cannot open " << argv[1] << '\n';
            return 1;
        }
        run_sync_replay(engine, file);
        return 0;
    }

    const char* input_file = std::getenv("INPUT_FILE");
    if (input_file != nullptr) {
        std::ifstream file(input_file);
        run_sync_replay(engine, file);
        return 0;
    }

    std::cerr << "Matching Engine (JSON lines). Single-threaded Boost.Asio stdin.\n";
    run_async_stdin(ctx, engine);
    ctx.run();
    return 0;
}
