#pragma once

#include <map>
#include <mutex>
#include <string>
#include <cstdint>
#include <cstddef> // size_t

class HashRing {
public:
    HashRing() = default;

    // Add/remove a physical node with 'virtual_nodes' replicas.
    void addNode(const std::string& node_id, int virtual_nodes);
    void removeNode(const std::string& node_id);

    // Return the node responsible for 'key'. Empty string if ring is empty.
    std::string getNode(const std::string& key) const;

    // Number of virtual nodes currently in the ring (counting all virtual nodes).
    size_t size() const;

private:
    static uint32_t hash(const std::string& data);

    mutable std::mutex mtx_;
    std::map<uint32_t, std::string> ring_;  // ordered ring (hash -> node_id)
};
