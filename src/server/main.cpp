#include "server/KVServer.hpp"
#include <asio/signal_set.hpp>
#include <asio/io_context.hpp>
#include <spdlog/spdlog.h>
#include <filesystem>

/* ───── Tiny CLI parser ──────────────────────────────────────────────── */
struct Options {
    int                 id   = 1;
    std::string         listen;
    std::vector<std::string> peers;
    std::string         data = "wal";
};

static Options parse(int argc, char* argv[])
{
    Options o;
    for (int i = 1; i < argc; i += 2) {
        std::string flag = argv[i];
        if (i + 1 == argc) throw std::runtime_error("missing value for " + flag);
        std::string val  = argv[i + 1];

        if      (flag == "--id")     o.id     = std::stoi(val);
        else if (flag == "--listen") o.listen = val;
        else if (flag == "--peers") {
            std::stringstream ss(val); std::string tok;
            while (std::getline(ss, tok, ',')) o.peers.push_back(tok);
        }
        else if (flag == "--data")   o.data   = val;
        else throw std::runtime_error("unknown flag " + flag);
    }
    if (o.listen.empty()) throw std::runtime_error("--listen is required");
    return o;
}

int main(int argc, char* argv[])
try {
    Options opt = parse(argc, argv);
    std::filesystem::create_directories(opt.data);

    /* storage + raft */
    auto kv   = std::make_shared<storage::Engine>(opt.data);
    auto raft = std::make_shared<RaftNode>(opt.id, opt.peers, opt.data, *kv);
    raft->start();

    /* gRPC service */
    KVServer service{raft, kv};
    grpc::ServerBuilder b;
    b.AddListeningPort(opt.listen, grpc::InsecureServerCredentials());
    b.RegisterService(&service);
    std::unique_ptr<grpc::Server> server = b.BuildAndStart();

    spdlog::info("Node {} listening on {}", opt.id, opt.listen);

    /* nice Ctrl-C handling inside the container                       */
    asio::io_context io;
    asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto){ server->Shutdown(); });
    io.run();
    return 0;
}
catch (const std::exception& e) {
    spdlog::error("fatal: {}", e.what());
    return 1;
}
