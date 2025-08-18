#pragma once
#include <memory>
#include <rocksdb/table.h>
#include <rocksdb/filter_policy.h>  // NewBloomFilterPolicy

inline std::unique_ptr<rocksdb::TableFactory> makeLSMTableFactory() {
    rocksdb::BlockBasedTableOptions tbo;
    tbo.filter_policy.reset(rocksdb::NewBloomFilterPolicy(10)); // 10 bits/key
    return std::unique_ptr<rocksdb::TableFactory>(
        rocksdb::NewBlockBasedTableFactory(tbo)
    );
}
