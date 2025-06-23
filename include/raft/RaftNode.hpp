#pragma once
#include <string>
#include <thread>
#include <vector>
#include <memory>

class Engine;
namespace nuraft { class raft_server; }

class RaftNode {
public:
    RaftNode(int id,
             const std::vector<std::string>& peers,
             const std::string& wal_dir,
             Engine& kv);

    void start();                                   // run NuRaft in its own thread
    void applyCommand(const std::string& key,
                      const std::string& value);    // replicate Put/Del

    bool linearizableRead();                        // Raft read-index helper
    bool isLeader() const;
    int  leaderId() const;

private:
    int                                id_;
    std::thread                        svc_thread_;
    std::shared_ptr<nuraft::raft_server> srv_;      // NuRaft server
};
