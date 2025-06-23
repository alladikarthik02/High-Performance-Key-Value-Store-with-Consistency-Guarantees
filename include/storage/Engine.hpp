#pragma once
#include <rocksdb/db.h>
#include <optional>
#include <string>
#include <vector>

class Engine {
public:
    explicit Engine(const std::string& path);
    ~Engine();

    // point operations
    bool put(const std::string& k, const std::string& v);
    bool get(const std::string& k, std::string& v) const;
    std::optional<std::string> get(const std::string& k) const {
        std::string tmp; return get(k, tmp) ? std::optional(tmp) : std::nullopt;
    }
    bool del(const std::string& k);

    // range / prefix scans
    std::vector<std::pair<std::string,std::string>>
    scanRange(const std::string& start, const std::string& end) const;

    std::vector<std::pair<std::string,std::string>>
    scanPrefix(const std::string& prefix) const;

private:
    rocksdb::DB* db_{nullptr};
};
