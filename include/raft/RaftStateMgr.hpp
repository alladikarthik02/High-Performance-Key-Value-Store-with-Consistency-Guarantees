#pragma once

#include <libnuraft/nuraft.hxx>

namespace nuraft {
class inmem_log_store; // fwd (we define the class in our header, but avoid heavy includes here)
}

class RaftStateMgr : public nuraft::state_mgr {
public:
    RaftStateMgr() = default;

    // ---- state_mgr interface ----
    nuraft::ptr<nuraft::cluster_config> load_config() override;
    void save_config(const nuraft::cluster_config& config) override;

    nuraft::ptr<nuraft::srv_state> read_state() override;
    void save_state(const nuraft::srv_state& state) override;

    nuraft::ptr<nuraft::log_store> load_log_store() override;

    nuraft::int32 server_id() override;
    void system_exit(const int exit_code) override;

private:
    // minimal in-memory persistence
    nuraft::ptr<nuraft::cluster_config> config_;
    nuraft::ptr<nuraft::srv_state>      state_;
    nuraft::int32                       server_id_ {1};
};
