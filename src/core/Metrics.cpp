#include "core/Metrics.hpp"
#include <mutex>

Metrics& Metrics::instance() {
    static Metrics m;
    return m;
}

void Metrics::inc(const std::string& name, uint64_t by) {
    counters_[name].fetch_add(by, std::memory_order_relaxed);
}

void Metrics::set(const std::string& name, double v) {
    std::scoped_lock lk(mtx_);
    gauges_[name] = v;
}

Metrics::Snapshot Metrics::snapshot() const {
    Snapshot s;
    s.ts = std::chrono::system_clock::now();

    for (auto& [k, v] : counters_)
        s.counters[k] = v.load(std::memory_order_relaxed);

    {
        std::scoped_lock lk(mtx_);
        s.gauges = gauges_;
    }
    return s;
}
