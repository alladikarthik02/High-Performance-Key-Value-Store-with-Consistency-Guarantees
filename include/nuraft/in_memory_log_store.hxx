#pragma once

// Bring in NuRaft core types: ptr<>, ulong, int32, log_entry, buffer, log_store, etc.
#include <libnuraft/nuraft.hxx>
#include <libnuraft/log_store.hxx>
#include <libnuraft/buffer.hxx>
#include <libnuraft/snapshot.hxx>

#include <map>
#include <mutex>
#include <vector>

namespace nuraft {

class inmem_log_store : public log_store {
public:
    inmem_log_store() : start_idx_(1) {}

    // Next write position (one past last)
    ulong next_slot() const override {
        std::lock_guard<std::mutex> l(m_);
        return start_idx_ + logs_.size();
    }

    // First valid index
    ulong start_index() const override {
        return start_idx_;
    }

    // Return last entry (NuRaft tags differ: make it non-const to satisfy older signatures)
    ptr<log_entry> last_entry() const override {
        std::lock_guard<std::mutex> l(m_);
        if (logs_.empty()) return nullptr;
        return logs_.rbegin()->second;
    }

    // Append and return index used
    ulong append(ptr<log_entry>& e) override {
        std::lock_guard<std::mutex> l(m_);
        ulong idx = start_idx_ + logs_.size();
        logs_[idx] = e;
        return idx;
    }

    // Overwrite at index (used on conflicts)
    void write_at(ulong index, ptr<log_entry>& e) override {
        std::lock_guard<std::mutex> l(m_);
        logs_[index] = e;
    }

    // Return entries in [start, end)
    ptr<std::vector<ptr<log_entry>>> log_entries(ulong start, ulong end) override {
        std::lock_guard<std::mutex> l(m_);
        auto v = cs_new<std::vector<ptr<log_entry>>>();
        v->reserve(end > start ? (size_t)(end - start) : 0);
        for (ulong i = start; i < end; ++i) {
            auto it = logs_.find(i);
            if (it == logs_.end()) continue;   // skip holes
            v->push_back(it->second);
        }
        return v;
    }

    // Single entry at index (nullptr if missing)
    ptr<log_entry> entry_at(ulong index) override {
        std::lock_guard<std::mutex> l(m_);
        auto it = logs_.find(index);
        return (it == logs_.end()) ? nullptr : it->second;
    }

    // Term at index (0 if missing)
    ulong term_at(ulong index) override {
        auto e = entry_at(index);
        return e ? e->get_term() : 0;
    }

    // Serialize up to `cnt` entries starting from `index`
    ptr<buffer> pack(ulong index, int32 cnt) override {
        std::vector<ptr<log_entry>> batch;
        batch.reserve(cnt > 0 ? (size_t)cnt : 0);

        for (int32 i = 0; i < cnt; ++i) {
            auto e = entry_at(index + (ulong)i);
            if (!e) break;
            batch.push_back(e);
        }

        size_t total = 0;
        for (auto& e : batch) {
            total += sizeof(ulong);              // term
            total += sizeof(char);               // type
            total += sizeof(int32);              // payload size
            total += e->get_buf().size();        // payload
        }

        auto buf = buffer::alloc(total);
        buf->pos(0);
        for (auto& e : batch) {
            buf->put((ulong)e->get_term());
            buf->put((char)e->get_val_type());
            auto b = e->get_buf_ptr();
            buf->put((int32)b->size());
            buf->put(*b);
        }
        return buf;
    }

    // Deserialize and write sequentially starting at `index`
    void apply_pack(ulong index, buffer& pack) override {
        pack.pos(0);
        while (pack.pos() < pack.size()) {
            ulong term = pack.get_ulong();
            log_val_type type = (log_val_type)pack.get_byte();
            int32 sz = pack.get_int();
            auto b = buffer::alloc(sz);
            pack.get(b);
            auto e = cs_new<log_entry>(term, b, type);
            write_at(index++, e);
        }
    }

    // Drop entries up to and including last_log_index
    bool compact(ulong last_log_index) override {
        std::lock_guard<std::mutex> l(m_);
        for (ulong i = start_idx_; i <= last_log_index; ++i) {
            logs_.erase(i);
        }
        start_idx_ = last_log_index + 1;
        return true;
    }

    bool flush() override { return true; }

private:
    std::map<ulong, ptr<log_entry>> logs_;
    mutable std::mutex m_;
    ulong start_idx_;
};

} // namespace nuraft

