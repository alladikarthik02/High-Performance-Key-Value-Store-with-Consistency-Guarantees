#pragma once
#include <map>
#include <shared_mutex>
#include <string>
#include <vector>

/**
 * @brief Consistent‑hash ring with virtual nodes (M1).
 *
 * Thread‑safe: readers use shared locks; writers use unique locks.
 */
class HashRing {
public:
    /// Add a physical node with @p virtual_nodes replicas on the ring
    void addNode(const std::string& node_id, int virtual_nodes = 100);

    /// Remove every replica belonging to @p node_id
    void removeNode(const std::string& node_id);

    /// Return the node responsible for @p key. Empty string if ring is empty.
    [[nodiscard]] std::string getNode(const std::string& key) const;

    /// Ring size (# of virtual entries) – handy for unit tests/metrics
    [[nodiscard]] size_t size() const;

private:
    uint32_t hash(const std::string& data) const;

    std::map<uint32_t, std::string> ring_;
    mutable std::shared_mutex       mtx_;
};
