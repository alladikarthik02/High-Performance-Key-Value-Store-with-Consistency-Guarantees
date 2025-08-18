#include "core/HashRing.hpp"
#include <openssl/sha.h>
#include <mutex>
#include <string>

namespace {
static inline uint32_t to_u32(const unsigned char* d) {
    return (static_cast<uint32_t>(d[0]) << 24) |
           (static_cast<uint32_t>(d[1]) << 16) |
           (static_cast<uint32_t>(d[2]) << 8)  |
            static_cast<uint32_t>(d[3]);
}
} // namespace

uint32_t HashRing::hash(const std::string& data) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest);
    uint32_t h = 0;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i += 4) h ^= to_u32(&digest[i]);
    return h;
}

void HashRing::addNode(const std::string& node_id, int virtual_nodes) {
    std::unique_lock<std::mutex> lk(mtx_);
    for (int i = 0; i < virtual_nodes; ++i) {
        const std::string key = node_id + "#" + std::to_string(i);
        ring_.emplace(hash(key), node_id);
    }
}

void HashRing::removeNode(const std::string& node_id) {
    std::unique_lock<std::mutex> lk(mtx_);
    for (auto it = ring_.begin(); it != ring_.end(); ) {
        if (it->second == node_id) it = ring_.erase(it);
        else ++it;
    }
}

std::string HashRing::getNode(const std::string& key) const {
    std::unique_lock<std::mutex> lk(mtx_);
    if (ring_.empty()) return {};
    const uint32_t h = hash(key);
    auto it = ring_.lower_bound(h);
    if (it == ring_.end()) it = ring_.begin();
    return it->second;
}

size_t HashRing::size() const {
    std::unique_lock<std::mutex> lk(mtx_);
    return ring_.size();
}
