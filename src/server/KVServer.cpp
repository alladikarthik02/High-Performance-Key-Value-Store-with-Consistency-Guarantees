#include "server/KVServer.hpp"          // header that declares this class
#include "raft/RaftNode.hpp"            // your thin NuRaft wrapper
#include "storage/Engine.hpp"           // RocksDB wrapper

#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>

using grpc::Status;
using grpc::StatusCode;

/**********************************************************************
* ctor
**********************************************************************/
KVServer::KVServer(std::shared_ptr<RaftNode>          raft,
                   std::shared_ptr<storage::Engine>   kv)
    : raft_(std::move(raft)), kv_(std::move(kv)) {}

/**********************************************************************
* PUT  (linearizable by default – waits for commit on majority)
**********************************************************************/
Status KVServer::Put(grpc::ServerContext*,
                     const kvproto::PutRequest*  req,
                     kvproto::PutResponse*       resp)
{
    // --- serialize entry: 'P' | key_len | key | value ----------------
    auto buf = nuraft::buffer::alloc(1 + sizeof(uint32_t)
                                     + req->key().size()
                                     + req->value().size());
    buf->put_u8('P');
    buf->put_u32(static_cast<uint32_t>(req->key().size()));
    buf->put_bytes(req->key().data(),  req->key().size());
    buf->put_bytes(req->value().data(), req->value().size());

    // append & wait for commit
    auto fut = raft_->append(std::move(buf));
    if (!fut->get()) {                               // false -> not committed
        return Status(StatusCode::ABORTED, "append_failed");
    }
    resp->set_ok(true);
    return Status::OK;
}

/**********************************************************************
* GET  (leader-only, **now fully linearizable**)
**********************************************************************/
Status KVServer::Get(grpc::ServerContext*,
                     const kvproto::GetRequest*  req,
                     kvproto::GetResponse*       resp)
{
    if (!raft_->isLeader()) {
        // hint client with current leader’s address if known
        auto ldr = raft_->leaderEndpoint();
        return Status(StatusCode::FAILED_PRECONDITION,
                      ldr.empty() ? "not_leader" : ("redirect " + ldr));
    }

    // ---- NEW line: ensure our state machine is up-to-date ------------
    if (!raft_->linearizableRead()) {
        return Status(StatusCode::ABORTED, "read_index_failed");
    }
    // ------------------------------------------------------------------

    std::string val;
    if (kv_->get(req->key(), val)) {
        resp->set_found(true);
        resp->set_value(std::move(val));
    } else {
        resp->set_found(false);
    }
    return Status::OK;
}

/**********************************************************************
* GET_LINEARIZABLE  (any replica – uses read-index internally)
**********************************************************************/
Status KVServer::GetLinearizable(grpc::ServerContext*,
                                 const kvproto::GetRequest*  req,
                                 kvproto::GetResponse*       resp)
{
    if (!raft_->linearizableRead()) {
        return Status(StatusCode::ABORTED, "read_index_failed");
    }
    std::string val;
    if (kv_->get(req->key(), val)) {
        resp->set_found(true);
        resp->set_value(std::move(val));
    } else {
        resp->set_found(false);
    }
    return Status::OK;
}

/**********************************************************************
* GET_STALE  (fast follower read – may lag)
**********************************************************************/
Status KVServer::GetStale(grpc::ServerContext*,
                          const kvproto::GetRequest*  req,
                          kvproto::GetResponse*       resp)
{
    std::string val;
    if (kv_->get(req->key(), val)) {
        resp->set_found(true);
        resp->set_value(std::move(val));
    } else {
        resp->set_found(false);
    }
    return Status::OK;
}
