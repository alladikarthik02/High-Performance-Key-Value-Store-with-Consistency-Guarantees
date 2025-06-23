#include "storage/Engine.hpp"
#include "storage/Filter.hpp"
#include <rocksdb/options.h>
#include <rocksdb/slice.h>

/* ---------------- ctor / dtor ------------------------------------------ */
Engine::Engine(const std::string& path) {
    rocksdb::Options opt;
    opt.create_if_missing = true;
    opt.table_factory     = makeLSMTableFactory();
    opt.IncreaseParallelism();
    opt.OptimizeLevelStyleCompaction();

    auto s = rocksdb::DB::Open(opt, path, &db_);
    if (!s.ok()) throw std::runtime_error(s.ToString());
}
Engine::~Engine() { delete db_; }

/* ---------------- point ops -------------------------------------------- */
bool Engine::put(const std::string& k, const std::string& v) {
    return db_->Put(rocksdb::WriteOptions(), k, v).ok();
}
bool Engine::get(const std::string& k, std::string& v) const {
    return db_->Get(rocksdb::ReadOptions(), k, &v).ok();
}
bool Engine::del(const std::string& k) {
    return db_->Delete(rocksdb::WriteOptions(), k).ok();
}

/* ---------------- scans ------------------------------------------------- */
std::vector<std::pair<std::string,std::string>>
Engine::scanRange(const std::string& start, const std::string& end) const
{
    rocksdb::ReadOptions ro;
    std::vector<std::pair<std::string,std::string>> out;
    std::unique_ptr<rocksdb::Iterator> it{ db_->NewIterator(ro) };

    for (it->Seek(start);
         it->Valid() && it->key().compare(end) < 0;
         it->Next())
        out.emplace_back(it->key().ToString(), it->value().ToString());

    return out;
}

std::vector<std::pair<std::string,std::string>>
Engine::scanPrefix(const std::string& pre) const
{
    rocksdb::ReadOptions ro;
    std::vector<std::pair<std::string,std::string>> out;
    std::unique_ptr<rocksdb::Iterator> it{ db_->NewIterator(ro) };

    for (it->Seek(pre);
         it->Valid() && it->key().starts_with(pre);
         it->Next())
        out.emplace_back(it->key().ToString(), it->value().ToString());

    return out;
}
