#include "proto/kv.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: benchmark <addr> <seconds>\n";
        return 1;
    }
    std::string addr = argv[1];
    int seconds      = std::stoi(argv[2]);

    auto chan = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    auto stub = kv::KeyValue::NewStub(chan);

    std::atomic<uint64_t> ops{0};
    auto worker = [&]() {
        grpc::ClientContext ctx;
        kv::PutRequest req; kv::PutResponse res;
        req.set_key("k"); req.set_value("v");
        while (true) {
            if (!stub->Put(&ctx, req, &res).ok()) break;
            ++ops;
        }
    };

    std::vector<std::thread> threads;
    for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i)
        threads.emplace_back(worker);

    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    for (auto& t : threads) t.detach();   // stop via ctx cancellation

    std::cout << "Ops: " << ops.load() / seconds << " / s\n";
}
