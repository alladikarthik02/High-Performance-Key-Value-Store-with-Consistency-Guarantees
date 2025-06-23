#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <mutex> 

/**
 * @brief Simple in‑process Prom‑style counter & gauge registry (M6+).
 */
class Metrics {
public:
    static Metrics& instance();

    void inc(const std::string& name, uint64_t by = 1);
    void set(const std::string& name, double value);

    struct Snapshot {
        std::unordered_map<std::string, uint64_t> counters;
        std::unordered_map<std::string, double>   gauges;
        std::chrono::system_clock::time_point     ts;
    };
    Snapshot snapshot() const;

private:
    Metrics()  = default;
    ~Metrics() = default;

    mutable std::mutex                                  mtx_;
    std::unordered_map<std::string, std::atomic_uint64_t> counters_;
    std::unordered_map<std::string, double>               gauges_;
};
