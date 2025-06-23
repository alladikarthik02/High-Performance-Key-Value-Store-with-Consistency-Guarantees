#pragma once
#include <nlohmann/json.hpp>
#include <string>

/**
 * @brief Raft user log entry (key + value) encoded as JSON (M4).
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
