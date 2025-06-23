#pragma once
#include <rocksdb/table.h>

/**
 * @brief Factory function returning a 10‑bit Bloom filter (M2).
 */
inline std::unique_ptr<rocksdb::TableFactory> makeLSMTableFactory() {
    rocksdb::BlockBasedTableOptions tbo;
    tbo.filter_policy.reset(rocksdb::NewBloomFilterPolicy(10 /* bits/key */));
    return std::unique_ptr<rocksdb::TableFactory>(
        rocksdb::NewBlockBasedTableFactory(tbo));
}
