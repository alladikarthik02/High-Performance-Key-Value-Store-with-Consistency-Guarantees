#include "raft/RaftNode.hpp"
#include "raft/Log.hpp"
#include "storage/Engine.hpp"
#include <libnuraft/nuraft.hxx>
#include <spdlog/spdlog.h>
#include <cstring>

namespace {

/* ---------------- NuRaft state-machine ----------------------------------- */
class KVStateMachine : public nuraft::state_machine {
public:
    explicit KVStateMachine(Engine& kv) : kv_(kv) {}
    ~KVStateMachine() override = default;

    nuraft::ptr<nuraft::buffer> commit(uint64_t /*idx*/,
                                       nuraft::ptr<nuraft::buffer> data) override {
        auto s = std::string{reinterpret_cast<char*>(data->data_begin()), data->size()};
        auto e = LogEntry::deserialize(s);
        kv_.put(e.key, e.value);
        return nullptr;
    }

    /* snapshot stubs (TODO: real snapshots) */
    void create_snapshot(nuraft::snapshot&,
                         nuraft::async_result<nuraft::ptr<nuraft::buffer>>::handler_type&) override {}
    int  read_snapshot(nuraft::ptr<nuraft::snapshot>&,
                       const std::vector<nuraft::ptr<nuraft::buffer>>&) override { return 0; }

private:
    Engine& kv_;
};

} // namespace

/* ------------------- RaftNode impl -------------------------------------- */
RaftNode::RaftNode(int id,
                   const std::vector<std::string>& peers,
                   const std::string& wal_dir,
                   Engine& engine)
    : id_(id)
{
    nuraft::raft_params params;
    params.heart_beat_interval_          = 100;   // ms
    params.election_timeout_lower_bound_ = 300;
    params.election_timeout_upper_bound_ = 600;

    auto sm = nuraft::cs_new<KVStateMachine>(engine);

    nuraft::launcher launcher;
    launcher.init(id_, wal_dir, peers, sm, params);

    srv_ = launcher.get_raft_server();
}

void RaftNode::start() { svc_thread_ = std::thread([this] { srv_->run(); }); }

void RaftNode::applyCommand(const std::string& key, const std::string& value)
{
    LogEntry e{key, value};
    auto buf = nuraft::buffer::alloc(e.serialize().size());
    std::memcpy(buf->data_begin(), e.serialize().data(), e.serialize().size());
    srv_->append_entries({buf});
}

bool RaftNode::linearizableRead()
{
    auto buf = nuraft::buffer::alloc(1);
    buf->pos(0);
    buf->data_begin()[0] = 0;

    auto res = srv_->read(buf);                 // blocks until quorum
    return res && res->get_accepted();
}

bool RaftNode::isLeader() const { return srv_->is_leader(); }
int  RaftNode::leaderId() const { return srv_->get_leader(); }
