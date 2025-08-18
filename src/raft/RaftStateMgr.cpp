#include "raft/RaftStateMgr.hpp"
#include "nuraft/in_memory_log_store.hxx"

using namespace nuraft;

ptr<cluster_config> RaftStateMgr::load_config() {
    if (!config_) {
        // default single-node config with this server id
        config_ = cs_new<cluster_config>();
        config_->get_servers().push_back(cs_new<srv_config>(server_id_, std::string("localhost")));
    }
    return config_;
}

void RaftStateMgr::save_config(const cluster_config& config) {
    // NuRaft v1.3.0: deserialize expects buffer_serializer&
    auto buf = config.serialize();
    buffer_serializer bs(buf);
    config_ = cluster_config::deserialize(bs);
}

ptr<srv_state> RaftStateMgr::read_state() {
    if (!state_) {
        state_ = cs_new<srv_state>();
        state_->set_term(1);
        state_->set_voted_for(-1);
    }
    return state_;
}

void RaftStateMgr::save_state(const srv_state& /*state*/) {
    // If you want a real copy, serialize/deserialize;
    // here we keep a shallow default (the code path using this was warning-only).
    if (!state_) state_ = cs_new<srv_state>();
}

ptr<log_store> RaftStateMgr::load_log_store() {
    // Use our vendored in-memory log store
    return cs_new<inmem_log_store>();
}

int32 RaftStateMgr::server_id() {
    return server_id_;
}

void RaftStateMgr::system_exit(const int exit_code) {
    (void)exit_code;
    // No-op in this toy persistence layer. In production, clean up and exit.
}
