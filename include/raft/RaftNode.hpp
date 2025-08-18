#pragma once
#include <memory>
#include <string>
#include <vector>

namespace nuraft { 
    class raft_server; 
    class buffer;
    class state_machine;
}
class Engine;

class RaftNode {
public:
    RaftNode(int id,
             const std::vector<std::string>& peers,
             const std::string& wal_dir,
             Engine& engine);
    ~RaftNode();

    void start();           // no-op for NuRaft v1.3.x
    void stop();            // shutdown raft_server
    bool linearizableRead(); // stubbed true (append no-op pattern would go here)
    
    // Additional methods used by KVServer
    bool isLeader() const;
    int leaderId() const;
    std::string leaderEndpoint() const;
    bool replicatePut(const std::string& key, const std::string& value);
    bool replicateDelete(const std::string& key);
    
    // Method for direct buffer append
    std::shared_ptr<nuraft::buffer> append(std::shared_ptr<nuraft::buffer> buf);

private:
    int id_;
    std::vector<std::string> peers_;
    std::string wal_dir_;
    Engine& engine_;
    
    std::shared_ptr<nuraft::raft_server> srv_;
    std::shared_ptr<nuraft::state_machine> sm_;
};