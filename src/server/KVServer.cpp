#include "server/KVServer.hpp"
#include "core/Metrics.hpp"
#include <spdlog/spdlog.h>

KVServer::KVServer(std::shared_ptr<RaftNode> raft,
                   std::shared_ptr<Engine> kv)
    : raft_(std::move(raft)), kv_(std::move(kv)) {}

grpc::Status KVServer::Put(grpc::ServerContext*,
                          const kvproto::PutRequest* req,
                          kvproto::PutResponse* resp) {
    Metrics::instance().inc("put_requests");
    
    if (!raft_->isLeader()) {
        std::string leader = raft_->leaderEndpoint();
        if (!leader.empty()) {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                              "Not leader, try: " + leader);
        }
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "No leader");
    }

    bool ok = raft_->replicatePut(req->key(), req->value());
    if (!ok) {
        Metrics::instance().inc("put_failures");
        return grpc::Status(grpc::StatusCode::INTERNAL, "Replication failed");
    }

    Metrics::instance().inc("put_success");
    return grpc::Status::OK;
}

grpc::Status KVServer::Get(grpc::ServerContext*,
                          const kvproto::GetRequest* req,
                          kvproto::GetResponse* resp) {
    Metrics::instance().inc("get_requests");
    
    auto val = kv_->get(req->key());
    if (!val) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Key not found");
    }
    
    resp->set_value(*val);
    Metrics::instance().inc("get_success");
    return grpc::Status::OK;
}

grpc::Status KVServer::GetLinearizable(grpc::ServerContext*,
                                      const kvproto::GetRequest* req,
                                      kvproto::GetResponse* resp) {
    Metrics::instance().inc("get_linearizable_requests");
    
    if (!raft_->isLeader()) {
        std::string leader = raft_->leaderEndpoint();
        if (!leader.empty()) {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                              "Not leader, try: " + leader);
        }
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "No leader");
    }

    // Perform linearizable read barrier
    if (!raft_->linearizableRead()) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "Linearizable read failed");
    }

    auto val = kv_->get(req->key());
    if (!val) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Key not found");
    }
    
    resp->set_value(*val);
    Metrics::instance().inc("get_linearizable_success");
    return grpc::Status::OK;
}

grpc::Status KVServer::GetStale(grpc::ServerContext*,
                               const kvproto::GetRequest* req,
                               kvproto::GetResponse* resp) {
    Metrics::instance().inc("get_stale_requests");
    
    // Stale read - no leader check, just read from local storage
    auto val = kv_->get(req->key());
    if (!val) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Key not found");
    }
    
    resp->set_value(*val);
    Metrics::instance().inc("get_stale_success");
    return grpc::Status::OK;
}

grpc::Status KVServer::Delete(grpc::ServerContext*,
                             const kvproto::DeleteRequest* req,
                             kvproto::DeleteResponse* resp) {
    if (!raft_->isLeader()) {
        std::string leader = raft_->leaderEndpoint();
        if (!leader.empty()) {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                              "Not leader, try: " + leader);
        }
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "No leader");
    }

    bool ok = raft_->replicateDelete(req->key());
    if (!ok) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "Replication failed");
    }

    return grpc::Status::OK;
}

grpc::Status KVServer::ScanRange(grpc::ServerContext*,
                                const kvproto::ScanRangeRequest* req,
                                grpc::ServerWriter<kvproto::KVPair>* writer) {
    auto results = kv_->scanRange(req->start_key(), req->end_key());
    for (const auto& [key, value] : results) {
        kvproto::KVPair pair;
        pair.set_key(key);
        pair.set_value(value);
        if (!writer->Write(pair)) {
            break; // Client disconnected
        }
    }
    return grpc::Status::OK;
}

grpc::Status KVServer::ScanPrefix(grpc::ServerContext*,
                                 const kvproto::ScanPrefixRequest* req,
                                 grpc::ServerWriter<kvproto::KVPair>* writer) {
    auto results = kv_->scanPrefix(req->prefix());
    for (const auto& [key, value] : results) {
        kvproto::KVPair pair;
        pair.set_key(key);
        pair.set_value(value);
        if (!writer->Write(pair)) {
            break; // Client disconnected
        }
    }
    return grpc::Status::OK;
}