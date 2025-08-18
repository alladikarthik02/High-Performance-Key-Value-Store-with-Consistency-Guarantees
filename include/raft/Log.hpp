#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <memory>

// Forward declaration
namespace nuraft { class buffer; }

namespace kv {

/**
 * @brief Raft user log entry (key + value) encoded as JSON (M4).
 */
struct LogEntry {
    std::string key;
    std::string value;

    inline std::string serialize() const {
        return nlohmann::json{{"k", key}, {"v", value}}.dump();
    }
    
    inline static LogEntry deserialize(const std::string& s) {
        auto j = nlohmann::json::parse(s);
        return {j["k"], j["v"]};
    }
};

// Functions needed by RaftNode.cpp
std::shared_ptr<nuraft::buffer> encode(const LogEntry& entry);
LogEntry decode(nuraft::buffer& buf);

} // namespace kv