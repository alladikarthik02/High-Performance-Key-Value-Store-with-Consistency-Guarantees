#pragma once
#include <libnuraft/nuraft.hxx>
#include <vector>
#include <mutex>

// Custom in-memory log store that properly implements the v1.3.0 interface
class custom_log_store : public nuraft::log_store {
public:
    custom_log_store();
    ~custom_log_store() override = default;

    // Required methods from log_store interface
    uint64_t next_slot() const override;
    uint64_t start_index() const override;
    nuraft::ptr<nuraft::log_entry> last_entry() const override;
    uint64_t append(nuraft::ptr<nuraft::log_entry>& entry) override;
    void write_at(uint64_t index, nuraft::ptr<nuraft::log_entry>& entry) override;
    nuraft::ptr<std::vector<nuraft::ptr<nuraft::log_entry>>> log_entries(
        uint64_t start, uint64_t end) override;
    nuraft::ptr<nuraft::log_entry> entry_at(uint64_t index) override;
    uint64_t term_at(uint64_t index) override;
    nuraft::ptr<nuraft::buffer> pack(uint64_t index, int32_t cnt) override;
    void apply_pack(uint64_t index, nuraft::buffer& pack) override;
    bool compact(uint64_t last_log_index) override;
    bool flush() override;

private:
    mutable std::mutex logs_lock_;
    std::vector<nuraft::ptr<nuraft::log_entry>> logs_;
    uint64_t start_idx_;
};