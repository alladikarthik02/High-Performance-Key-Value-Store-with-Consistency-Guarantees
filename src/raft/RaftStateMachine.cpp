#include "raft/RaftStateMachine.hpp"
#include <iostream>

// Apply log before commit (optional)
nuraft::ptr<nuraft::buffer> RaftStateMachine::pre_commit(const nuraft::ulong log_idx,
                                                         nuraft::buffer& data) {
    // No-op for now
    (void)log_idx;
    (void)data;
    return nullptr;
}

// Apply committed log  
nuraft::ptr<nuraft::buffer> RaftStateMachine::commit(const nuraft::ulong log_idx,
                                                     nuraft::buffer& data) {
    // Basic implementation - decode and apply to storage
    (void)log_idx;
    (void)data;
    
    // In a real implementation, decode data and apply to storage engine
    return nullptr;
}

// Rollback uncommitted log
void RaftStateMachine::rollback(const nuraft::ulong log_idx,
                                nuraft::buffer& data) {
    (void)log_idx;
    (void)data;
    // No-op for now
}

// Snapshot related methods
void RaftStateMachine::save_logical_snp_obj(nuraft::snapshot& s,
                                             nuraft::ulong& obj_id,
                                             nuraft::buffer& data,
                                             bool is_first_obj,
                                             bool is_last_obj) {
    (void)s;
    (void)obj_id;
    (void)data;
    (void)is_first_obj;
    (void)is_last_obj;
}

bool RaftStateMachine::apply_snapshot(nuraft::snapshot& s) {
    (void)s;
    return true;
}

int RaftStateMachine::read_logical_snp_obj(nuraft::snapshot& s,
                                           void*& user_snp_ctx,
                                           nuraft::ulong obj_id,
                                           nuraft::ptr<nuraft::buffer>& data_out,
                                           bool& is_last_obj) {
    (void)s;
    (void)user_snp_ctx;
    (void)obj_id;
    (void)data_out;
    (void)is_last_obj;
    return 0;
}

void RaftStateMachine::free_user_snp_ctx(void*& user_snp_ctx) {
    (void)user_snp_ctx;
}

// Non-virtual methods (not override)
nuraft::ulong RaftStateMachine::last_snapshot_term() {
    return 0;
}

nuraft::ulong RaftStateMachine::last_snapshot_index() {
    return 0;
}