// src/server/main.cpp
#include "server/KVServer.hpp"
#include "raft/RaftNode.hpp"
#include "storage/Engine.hpp"

#include <asio/signal_set.hpp>
#include <asio/io_context.hpp>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <sstream>

/* ───── Tiny CLI parser ──────────────────────────────────────────────── */
struct Options {
    int id = 1;
    std::string listen = "0.0.0.0:50051";
    std::vector<std::string> peers;
    std::string data = "wal";
};

static std::vector<std::string> split_csv(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

static Options parse(int argc, char* argv[]) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* what)->std::string{
            if (i + 1 >= argc) throw std::runtime_error(std::string("missing value for ")+what);
            return argv[++i];
        };
        if (a == "--id")        { opt.id = std::stoi(next("--id")); }
        else if (a == "--listen"){ opt.listen = next("--listen"); }
        else if (a == "--peers") { opt.peers = split_csv(next("--peers")); }
        else if (a == "--data")  { opt.data = next("--data"); }
        else {
            throw std::runtime_error("unknown arg: " + a);
        }
    }
    return opt;
}

int main(int argc, char* argv[]) try {
    auto opt = parse(argc, argv);
    std::filesystem::create_directories(opt.data);

    // Storage + Raft
    auto kv   = std::make_shared<Engine>(opt.data);
    auto raft = std::make_shared<RaftNode>(opt.id, opt.peers, opt.data, *kv);
    raft->start();

    auto service = std::make_unique<KVServer>(raft, kv);

    // Enable gRPC Health + Reflection
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    // Build server
    grpc::ServerBuilder builder;
    builder.AddListeningPort(opt.listen, grpc::InsecureServerCredentials());
    builder.RegisterService(service.get());

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server) throw std::runtime_error("failed to start gRPC server");

    spdlog::info("Node {} listening on {}", opt.id, opt.listen);

    // Graceful shutdown via signals
    asio::io_context io;
    asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto){
        spdlog::info("shutdown requested");
        server->Shutdown();
    });
    io.run();

    return 0;
}
catch (const std::exception& e) {
    spdlog::error("fatal: {}", e.what());
    return 1;
}
