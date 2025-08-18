#include "raft/Log.hpp"
#include <libnuraft/buffer.hxx>
#include <cstring>

namespace kv {

std::shared_ptr<nuraft::buffer> encode(const LogEntry& entry) {
    std::string serialized = entry.serialize();
    auto buf = nuraft::buffer::alloc(serialized.size());
    
    // NuRaft v1.3.0 doesn't have put_bytes, use memcpy instead
    std::memcpy(buf->data_begin(), serialized.data(), serialized.size());
    buf->pos(0);  // Reset position to beginning
    
    return buf;
}

LogEntry decode(nuraft::buffer& buf) {
    std::string serialized(reinterpret_cast<const char*>(buf.data_begin()), buf.size());
    return LogEntry::deserialize(serialized);
}

} // namespace kv