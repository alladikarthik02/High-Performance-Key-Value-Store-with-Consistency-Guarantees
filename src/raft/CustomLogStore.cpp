#include "raft/CustomLogStore.hpp"
#include <spdlog/spdlog.h>

custom_log_store::custom_log_store() : start_idx_(1) {
    // Initialize with a dummy entry at index 0
    auto initial_entry = nuraft::cs_new<nuraft::log_entry>(
        0, nuraft::buffer::alloc(sizeof(int)), nuraft::log_val_type::app_log);
    logs_.push_back(initial_entry);
}

uint64_t custom_log_store::append(nuraft::ptr<nuraft::log_entry>& entry) {
    std::lock_guard<std::mutex> lock(logs_lock_);
    uint64_t idx = start_idx_ + logs_.size() - 1;
    logs_.push_back(entry);
    return idx;
}

void custom_log_store::write_at(uint64_t index, nuraft::ptr<nuraft::log_entry>& entry) {
    std::lock_guard<std::mutex> lock(logs_lock_);
    if (index < start_idx_) {
        return;
    }
    
    uint64_t relative_idx = index - start_idx_ + 1;
    if (relative_idx >= logs_.size()) {
        logs_.resize(relative_idx + 1);
    }
    logs_[relative_idx] = entry;
}

nuraft::ptr<std::vector<nuraft::ptr<nuraft::log_entry>>> custom_log_store::log_entries(
    uint64_t start, uint64_t end) {
    std::lock_guard<std::mutex> lock(logs_lock_);
    auto ret = nuraft::cs_new<std::vector<nuraft::ptr<nuraft::log_entry>>>();
    
    for (uint64_t i = start; i < end; ++i) {
        if (i < start_idx_ || i - start_idx_ + 1 >= logs_.size()) {
            continue;
        }
        ret->push_back(logs_[i - start_idx_ + 1]);
    }
    return ret;
}

nuraft::ptr<nuraft::log_entry> custom_log_store::entry_at(uint64_t index) {
    std::lock_guard<std::mutex> lock(logs_lock_);
    if (index < start_idx_ || index - start_idx_ + 1 >= logs_.size()) {
        return nullptr;
    }
    return logs_[index - start_idx_ + 1];
}

uint64_t custom_log_store::term_at(uint64_t index) {
    auto entry = entry_at(index);
    if (!entry) {
        return 0;
    }
    return entry->get_term();
}

nuraft::ptr<nuraft::buffer> custom_log_store::pack(uint64_t index, int32_t cnt) {
    std::lock_guard<std::mutex> lock(logs_lock_);
    
    // Suppress unused parameter warning
    (void)index;
    
    // Simple implementation: just return a dummy buffer
    auto buf = nuraft::buffer::alloc(sizeof(int32_t));
    buf->put(cnt);
    buf->pos(0);
    return buf;
}

void custom_log_store::apply_pack(uint64_t index, nuraft::buffer& pack) {
    // Basic implementation - in production you'd deserialize the pack
    (void)index;
    (void)pack;
}

bool custom_log_store::compact(uint64_t last_log_index) {
    std::lock_guard<std::mutex> lock(logs_lock_);
    
    if (last_log_index < start_idx_) {
        return true;
    }
    
    uint64_t compact_upto = last_log_index - start_idx_ + 1;
    if (compact_upto >= logs_.size()) {
        return true;
    }
    
    // Remove compacted entries
    logs_.erase(logs_.begin() + 1, logs_.begin() + compact_upto + 1);
    start_idx_ = last_log_index + 1;
    
    return true;
}

bool custom_log_store::flush() {
    // In-memory store, so always return true
    return true;
}

uint64_t custom_log_store::next_slot() const {
    std::lock_guard<std::mutex> lock(logs_lock_);
    return start_idx_ + logs_.size() - 1;
}

uint64_t custom_log_store::start_index() const {
    return start_idx_;
}

nuraft::ptr<nuraft::log_entry> custom_log_store::last_entry() const {
    std::lock_guard<std::mutex> lock(logs_lock_);
    if (logs_.empty()) {
        return nullptr;
    }
    return logs_.back();
}