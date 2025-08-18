#include "kvstore.grpc.pb.h"  // Fixed include path
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <vector>

static void usage() {
    std::cout <<
    "Usage:\n"
    "  kv_client <host:port> put <key> <value>\n"
    "  kv_client <host:port> del <key>\n"
    "  kv_client <host:port> get <key>\n"
    "  kv_client <host:port> get-lin  <key>\n"
    "  kv_client <host:port> get-stale <key>\n"
    "  kv_client <host:port> scan-range  <start> <end>\n"
    "  kv_client <host:port> scan-prefix <prefix>\n";
}

int main(int argc, char** argv)
{
    if (argc < 4) { usage(); return 1; }

    const std::string endpoint = argv[1];
    const std::string op       = argv[2];

    auto chan = grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
    auto stub = kv::KeyValue::NewStub(chan);
    grpc::ClientContext ctx;

    /* ---------------- point ops ---------------------------------------- */
    if (op == "put" && argc == 5) {
        kv::PutRequest req; req.set_key(argv[3]); req.set_value(argv[4]);
        kv::PutResponse resp;
        auto st = stub->Put(&ctx, req, &resp);
        std::cout << (st.ok() ? "OK\n" : st.error_message() + "\n");
    }
    else if (op == "del" && argc == 4) {
        kv::DeleteRequest req; req.set_key(argv[3]);
        kv::DeleteResponse resp;
        auto st = stub->Delete(&ctx, req, &resp);
        std::cout << (st.ok() ? "OK\n" : st.error_message() + "\n");
    }
    else if ((op == "get" || op == "get-lin" || op == "get-stale") && argc == 4) {
        kv::GetRequest req; req.set_key(argv[3]);
        kv::GetResponse resp;
        grpc::Status st;
        if      (op == "get")       st = stub->Get(&ctx, req, &resp);
        else if (op == "get-lin")   st = stub->GetLinearizable(&ctx, req, &resp);
        else                        st = stub->GetStale(&ctx, req, &resp);

        if (st.ok()) std::cout << resp.value() << '\n';
        else         std::cout << st.error_message() << '\n';
    }
    /* ---------------- scans ------------------------------------------- */
    else if (op == "scan-range" && argc == 5) {
        kv::ScanRangeRequest req; req.set_start_key(argv[3]); req.set_end_key(argv[4]);
        grpc::ClientContext ctx2;
        std::unique_ptr<grpc::ClientReader<kv::KVPair>> rdr(
            stub->ScanRange(&ctx2, req));

        kv::KVPair kvp;
        while (rdr->Read(&kvp))
            std::cout << kvp.key() << " -> " << kvp.value() << '\n';
    }
    else if (op == "scan-prefix" && argc == 4) {
        kv::ScanPrefixRequest req; req.set_prefix(argv[3]);
        grpc::ClientContext ctx2;
        auto rdr = stub->ScanPrefix(&ctx2, req);

        kv::KVPair kvp;
        while (rdr->Read(&kvp))
            std::cout << kvp.key() << " -> " << kvp.value() << '\n';
    }
    else {
        usage();
        return 1;
    }
}