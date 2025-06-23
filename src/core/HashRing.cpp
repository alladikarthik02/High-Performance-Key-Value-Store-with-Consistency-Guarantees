#include "core/HashRing.hpp"
#include <openssl/sha.h>

namespace {

uint32_t to_u32(const unsigned char* d) {
    return (static_cast<uint32_t>(d[0]) << 24) |
           (static_cast<uint32_t>(d[1]) << 16) |
           (static_cast<uint32_t>(d[2]) << 8)  |
            static_cast<uint32_t>(d[3]);
}

}

uint32_t HashRing::hash(const std::string& data) const {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()),
           data.size(), digest);
    return to_u32(digest);  // first 4 bytes
}

void HashRing::addNode(const std::string& node, int vnodes) {
    std::unique_lock lk(mtx_);
    for (int i = 0; i < vnodes; ++i)
        ring_[hash(node + "#" + std::to_string(i))] = node;
}

void HashRing::removeNode(const std::string& node) {
    std::unique_lock lk(mtx_);
    for (auto it = ring_.begin(); it != ring_.end(); )
        if (it->second == node) it = ring_.erase(it);
        else ++it;
}

std::string HashRing::getNode(const std::string& key) const {
    std::shared_lock lk(mtx_);
    if (ring_.empty()) return {};
    uint32_t h = hash(key);
    auto it = ring_.lower_bound(h);
    if (it == ring_.end()) it = ring_.begin();
    return it->second;
}

size_t HashRing::size() const {
    std::shared_lock lk(mtx_);
    return ring_.size();
}
