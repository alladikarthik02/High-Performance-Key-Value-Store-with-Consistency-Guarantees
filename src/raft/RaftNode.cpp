#include "raft/RaftNode.hpp"
#include "raft/Log.hpp"           // kv::encode / kv::decode
#include "storage/Engine.hpp"     // Engine::put / Engine::del

#include <libnuraft/nuraft.hxx>
#include <iostream>

#include <atomic>
#include <cassert>
#include <chrono>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// We vendor NuRaft's example in-memory log store.
#include "nuraft/in_memory_log_store.hxx"

using namespace nuraft;
using namespace std::chrono_literals;

// ----------------------
// KV state machine
// ----------------------
namespace {

class KVStateMachine : public state_machine {
public:
    explicit KVStateMachine(Engine& eng)
        : engine_(eng), last_commit_idx_(0) {}

    // Apply a committed log entry: decode -> put/del in the storage engine.
    ptr<buffer> commit(ulong idx, buffer& data) override {
        // NuRaft delivers commits in order; track our last index.
        last_commit_idx_.store(idx, std::memory_order_release);

        // Log entry payload is our LogEntry {key,value}. Empty value => delete.
        kv::LogEntry e = kv::decode(data);
        if (e.value.empty()) {
            engine_.del(e.key);
        } else {
            engine_.put(e.key, e.value);
        }
        // We don't return any user payload from commit.
        return ptr<buffer>();
    }

    // Config changes not used yet; keep as a no-op (no override to dodge
    // minor signature drifts across NuRaft versions).
    void commit_config(const cluster_config&) { /* no-op */ }

    // Snapshotting is stubbed for now (RocksDB checkpoint can be wired later).
    bool apply_snapshot(snapshot&) override { return true; }
    ptr<snapshot> last_snapshot() override { return ptr<snapshot>(); }

    ulong last_commit_index() override {
        return last_commit_idx_.load(std::memory_order_acquire);
    }

    // Tell NuRaft "snapshot succeeded" immediately.
    void create_snapshot(snapshot&,
                         async_result<bool>::handler_type& when_done) override {
        bool ok = true;
        std::shared_ptr<std::exception> err;
        when_done(ok, err);
    }

private:
    Engine& engine_;
    std::atomic<ulong> last_commit_idx_;
};

// Utility: build a log buffer from (key, value).
static ptr<buffer> make_kv_buf(const std::string& key, const std::string& value) {
    kv::LogEntry e{ key, value };
    return kv::encode(e);
}

// Utility: make a "barrier" NO-OP entry that we can replicate to fence reads.
// We write a key in a reserved namespace and immediately delete it (empty value).
static ptr<buffer> make_barrier_buf() {
    static thread_local uint64_t c = 0;
    std::string key = "__kvstore::barrier__/" + std::to_string(++c);
    std::string value; // empty => delete in our state machine
    return make_kv_buf(key, value);
}

} // anonymous namespace

// ----------------------
// Minimal State Manager
// Loads cluster config from `peers_` and uses in-memory log store/state.
// ----------------------
class PeersStateMgr : public state_mgr {
public:
    PeersStateMgr(int my_id, const std::vector<std::string>& peers)
        : my_id_(my_id), peers_(peers) {
        assert(my_id_ >= 1 && static_cast<size_t>(my_id_) <= peers_.size());
        // FIXED: srv_state constructor in v1.3.0 requires 3 arguments (term, voted_for, et_allowed)
        state_ = cs_new<srv_state>(0, 0, true);
    }

    // --- cluster configuration ---
    ptr<cluster_config> load_config() override {
        auto conf = cs_new<cluster_config>();
        for (size_t i = 0; i < peers_.size(); ++i) {
            int id = static_cast<int>(i + 1);
            conf->get_servers().push_back(cs_new<srv_config>(id, peers_[i]));
        }
        return conf;
    }

    void save_config(const cluster_config&) override {
        // Keep config in memory only for this project.
    }

    // --- persistent server state (term, voted_for) ---
    void save_state(const srv_state& s) override {
        // FIXED: srv_state constructor requires 3 arguments
        state_ = cs_new<srv_state>(s.get_term(), s.get_voted_for(), true);
    }

    // NOTE: NuRaft v1.3.0 requires returning a ptr<srv_state>.
    ptr<srv_state> read_state() override {
        return state_;
    }

    // --- log store ---
    ptr<log_store> load_log_store() override {
        return cs_new<inmem_log_store>();
    }

    int  server_id() override { return my_id_; }
    void system_exit(int) override { /* not used */ }

private:
    int my_id_;
    std::vector<std::string> peers_;
    ptr<srv_state> state_;
};

// ----------------------
// Simple Console Logger
// ----------------------
class SimpleLogger : public logger {
public:
    SimpleLogger() = default;

    void put_details(int level,
                    const char* source_file,
                    const char* func_name,
                    size_t line_number,
                    const std::string& msg) override {
        // Simple console logging - you can make this more sophisticated
        std::cout << "[" << level << "] " << source_file << ":" << line_number 
                  << " " << func_name << "() - " << msg << std::endl;
    }

    void flush() {
        std::cout.flush();
    }
};

// ----------------------
// RaftNode implementation
// ----------------------

RaftNode::RaftNode(int id,
                   const std::vector<std::string>& peers,
                   const std::string& wal_dir,
                   Engine& engine)
    : id_(id), peers_(peers), wal_dir_(wal_dir), engine_(engine) {

    // Create state manager (using the existing PeersStateMgr from your code)
    ptr<state_mgr> smgr = cs_new<PeersStateMgr>(id_, peers_);

    // Create state machine (using the existing KVStateMachine from your code)
    sm_ = cs_new<KVStateMachine>(engine_);

    // Create logger
    ptr<logger> lg = cs_new<SimpleLogger>();

    // ASIO options - this should be a struct, not a string
    asio_service::options iopts;
    iopts.thread_pool_size_ = 4;

    // Raft parameters
    raft_params params;
    params.with_election_timeout_lower(150)
          .with_election_timeout_upper(300)
          .with_hb_interval(50);  // Use with_snapshot_distance, not snapshot_distance_
    params.snapshot_distance_ = 0;
    // Initialize raft launcher
    raft_launcher launcher;
    
    // The init method signature for NuRaft v1.3.0:
    // init(state_machine, state_mgr, logger, port, asio_options, raft_params)
    int port = 50051 + id_;
    srv_ = launcher.init(sm_, smgr, lg, port, iopts, params);

    if (!srv_) {
        throw std::runtime_error("Failed to initialize Raft server");
    }
}

RaftNode::~RaftNode() { 
    stop(); 
}

void RaftNode::start() {
    // NuRaft manages its own worker threads; nothing to do here.
}

void RaftNode::stop() {
    if (srv_) {
        try { 
            srv_->shutdown(); 
        } catch (...) {
            // Ignore shutdown exceptions
        }
        srv_.reset();
    }
}

bool RaftNode::isLeader() const {
    return srv_ && srv_->is_leader();
}

int RaftNode::leaderId() const {
    if (!srv_) return -1;
    // FIXED: get_leader() no longer returns a pointer in v1.3.0
    auto ldr_id = srv_->get_leader();
    return static_cast<int>(ldr_id);
}

std::string RaftNode::leaderEndpoint() const {
    int lid = leaderId();
    if (lid > 0 && lid <= static_cast<int>(peers_.size())) {
        return peers_[lid - 1];
    }
    return "";
}

// Linearizable read: replicate a no-op barrier and wait for it to commit.
// Caller should redirect to leader if this returns false and we're not leader.
bool RaftNode::linearizableRead() {
    if (!srv_ || !srv_->is_leader()) return false;

    std::vector<ptr<buffer>> batch{ make_barrier_buf() };

    // append_entries returns a future-like cmd_result<T>.
    auto res = srv_->append_entries(batch);
    if (!res) return false;

    // Check submission status on the cmd_result object (NOT on .get()).
    if (!res->get_accepted() || res->get_result_code() != cmd_result_code::OK) {
        return false;
    }

    // Block until the entry is committed on majority.
    try {
        (void)res->get(); // we don't use the returned buffer payload
        return true;
    } catch (...) {
        return false;
    }
}

bool RaftNode::replicatePut(const std::string& key, const std::string& value) {
    if (!srv_ || !srv_->is_leader()) return false;

    std::vector<ptr<buffer>> batch{ make_kv_buf(key, value) };
    auto res = srv_->append_entries(batch);
    if (!res) return false;

    if (!res->get_accepted() || res->get_result_code() != cmd_result_code::OK) {
        return false;
    }

    try {
        (void)res->get();
        return true;
    } catch (...) {
        return false;
    }
}

bool RaftNode::replicateDelete(const std::string& key) {
    // Empty value triggers delete in KVStateMachine.
    return replicatePut(key, std::string{});
}

std::shared_ptr<nuraft::buffer> RaftNode::append(std::shared_ptr<nuraft::buffer> buf) {
    if (!srv_ || !srv_->is_leader()) return nullptr;
    
    std::vector<ptr<buffer>> batch{ buf };
    auto res = srv_->append_entries(batch);
    if (!res) return nullptr;
    
    if (!res->get_accepted() || res->get_result_code() != cmd_result_code::OK) {
        return nullptr;
    }
    
    try {
        return res->get();
    } catch (...) {
        return nullptr;
    }
}