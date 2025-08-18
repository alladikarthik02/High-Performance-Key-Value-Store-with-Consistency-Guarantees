#pragma once

#include <libnuraft/nuraft.hxx>

class RaftStateMachine : public nuraft::state_machine {
public:
    // Apply log before commit (optional)
    nuraft::ptr<nuraft::buffer> pre_commit(const nuraft::ulong log_idx,
                                           nuraft::buffer& data) override;

    // Apply committed log
    nuraft::ptr<nuraft::buffer> commit(const nuraft::ulong log_idx,
                                       nuraft::buffer& data) override;

    // Rollback uncommitted log
    void rollback(const nuraft::ulong log_idx,
                  nuraft::buffer& data) override;

    // Snapshot related
    void save_logical_snp_obj(nuraft::snapshot& s,
                              nuraft::ulong& obj_id,
                              nuraft::buffer& data,
                              bool is_first_obj,
                              bool is_last_obj) override;

    bool apply_snapshot(nuraft::snapshot& s) override;

    int read_logical_snp_obj(nuraft::snapshot& s,
                             void*& user_snp_ctx,
                             nuraft::ulong obj_id,
                             nuraft::ptr<nuraft::buffer>& data_out,
                             bool& is_last_obj) override;

    void free_user_snp_ctx(void*& user_snp_ctx) override;

    // These are NOT virtual in NuRaft v1.3.0; keep them non-override in your class
    nuraft::ulong last_snapshot_term();
    nuraft::ulong last_snapshot_index();
};
