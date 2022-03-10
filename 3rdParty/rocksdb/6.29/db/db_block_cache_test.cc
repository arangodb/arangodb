//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
#include <cstdlib>
#include <functional>
#include <memory>

#include "cache/cache_entry_roles.h"
#include "cache/lru_cache.h"
#include "db/column_family.h"
#include "db/db_test_util.h"
#include "port/stack_trace.h"
#include "rocksdb/persistent_cache.h"
#include "rocksdb/statistics.h"
#include "rocksdb/table.h"
#include "util/compression.h"
#include "util/defer.h"
#include "util/random.h"
#include "utilities/fault_injection_fs.h"

namespace ROCKSDB_NAMESPACE {

class DBBlockCacheTest : public DBTestBase {
 private:
  size_t miss_count_ = 0;
  size_t hit_count_ = 0;
  size_t insert_count_ = 0;
  size_t failure_count_ = 0;
  size_t compression_dict_miss_count_ = 0;
  size_t compression_dict_hit_count_ = 0;
  size_t compression_dict_insert_count_ = 0;
  size_t compressed_miss_count_ = 0;
  size_t compressed_hit_count_ = 0;
  size_t compressed_insert_count_ = 0;
  size_t compressed_failure_count_ = 0;

 public:
  const size_t kNumBlocks = 10;
  const size_t kValueSize = 100;

  DBBlockCacheTest()
      : DBTestBase("db_block_cache_test", /*env_do_fsync=*/true) {}

  BlockBasedTableOptions GetTableOptions() {
    BlockBasedTableOptions table_options;
    // Set a small enough block size so that each key-value get its own block.
    table_options.block_size = 1;
    return table_options;
  }

  Options GetOptions(const BlockBasedTableOptions& table_options) {
    Options options = CurrentOptions();
    options.create_if_missing = true;
    options.avoid_flush_during_recovery = false;
    // options.compression = kNoCompression;
    options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();
    options.table_factory.reset(NewBlockBasedTableFactory(table_options));
    return options;
  }

  void InitTable(const Options& /*options*/) {
    std::string value(kValueSize, 'a');
    for (size_t i = 0; i < kNumBlocks; i++) {
      ASSERT_OK(Put(ToString(i), value.c_str()));
    }
  }

  void RecordCacheCounters(const Options& options) {
    miss_count_ = TestGetTickerCount(options, BLOCK_CACHE_MISS);
    hit_count_ = TestGetTickerCount(options, BLOCK_CACHE_HIT);
    insert_count_ = TestGetTickerCount(options, BLOCK_CACHE_ADD);
    failure_count_ = TestGetTickerCount(options, BLOCK_CACHE_ADD_FAILURES);
    compressed_miss_count_ =
        TestGetTickerCount(options, BLOCK_CACHE_COMPRESSED_MISS);
    compressed_hit_count_ =
        TestGetTickerCount(options, BLOCK_CACHE_COMPRESSED_HIT);
    compressed_insert_count_ =
        TestGetTickerCount(options, BLOCK_CACHE_COMPRESSED_ADD);
    compressed_failure_count_ =
        TestGetTickerCount(options, BLOCK_CACHE_COMPRESSED_ADD_FAILURES);
  }

  void RecordCacheCountersForCompressionDict(const Options& options) {
    compression_dict_miss_count_ =
        TestGetTickerCount(options, BLOCK_CACHE_COMPRESSION_DICT_MISS);
    compression_dict_hit_count_ =
        TestGetTickerCount(options, BLOCK_CACHE_COMPRESSION_DICT_HIT);
    compression_dict_insert_count_ =
        TestGetTickerCount(options, BLOCK_CACHE_COMPRESSION_DICT_ADD);
  }

  void CheckCacheCounters(const Options& options, size_t expected_misses,
                          size_t expected_hits, size_t expected_inserts,
                          size_t expected_failures) {
    size_t new_miss_count = TestGetTickerCount(options, BLOCK_CACHE_MISS);
    size_t new_hit_count = TestGetTickerCount(options, BLOCK_CACHE_HIT);
    size_t new_insert_count = TestGetTickerCount(options, BLOCK_CACHE_ADD);
    size_t new_failure_count =
        TestGetTickerCount(options, BLOCK_CACHE_ADD_FAILURES);
    ASSERT_EQ(miss_count_ + expected_misses, new_miss_count);
    ASSERT_EQ(hit_count_ + expected_hits, new_hit_count);
    ASSERT_EQ(insert_count_ + expected_inserts, new_insert_count);
    ASSERT_EQ(failure_count_ + expected_failures, new_failure_count);
    miss_count_ = new_miss_count;
    hit_count_ = new_hit_count;
    insert_count_ = new_insert_count;
    failure_count_ = new_failure_count;
  }

  void CheckCacheCountersForCompressionDict(
      const Options& options, size_t expected_compression_dict_misses,
      size_t expected_compression_dict_hits,
      size_t expected_compression_dict_inserts) {
    size_t new_compression_dict_miss_count =
        TestGetTickerCount(options, BLOCK_CACHE_COMPRESSION_DICT_MISS);
    size_t new_compression_dict_hit_count =
        TestGetTickerCount(options, BLOCK_CACHE_COMPRESSION_DICT_HIT);
    size_t new_compression_dict_insert_count =
        TestGetTickerCount(options, BLOCK_CACHE_COMPRESSION_DICT_ADD);
    ASSERT_EQ(compression_dict_miss_count_ + expected_compression_dict_misses,
              new_compression_dict_miss_count);
    ASSERT_EQ(compression_dict_hit_count_ + expected_compression_dict_hits,
              new_compression_dict_hit_count);
    ASSERT_EQ(
        compression_dict_insert_count_ + expected_compression_dict_inserts,
        new_compression_dict_insert_count);
    compression_dict_miss_count_ = new_compression_dict_miss_count;
    compression_dict_hit_count_ = new_compression_dict_hit_count;
    compression_dict_insert_count_ = new_compression_dict_insert_count;
  }

  void CheckCompressedCacheCounters(const Options& options,
                                    size_t expected_misses,
                                    size_t expected_hits,
                                    size_t expected_inserts,
                                    size_t expected_failures) {
    size_t new_miss_count =
        TestGetTickerCount(options, BLOCK_CACHE_COMPRESSED_MISS);
    size_t new_hit_count =
        TestGetTickerCount(options, BLOCK_CACHE_COMPRESSED_HIT);
    size_t new_insert_count =
        TestGetTickerCount(options, BLOCK_CACHE_COMPRESSED_ADD);
    size_t new_failure_count =
        TestGetTickerCount(options, BLOCK_CACHE_COMPRESSED_ADD_FAILURES);
    ASSERT_EQ(compressed_miss_count_ + expected_misses, new_miss_count);
    ASSERT_EQ(compressed_hit_count_ + expected_hits, new_hit_count);
    ASSERT_EQ(compressed_insert_count_ + expected_inserts, new_insert_count);
    ASSERT_EQ(compressed_failure_count_ + expected_failures, new_failure_count);
    compressed_miss_count_ = new_miss_count;
    compressed_hit_count_ = new_hit_count;
    compressed_insert_count_ = new_insert_count;
    compressed_failure_count_ = new_failure_count;
  }

#ifndef ROCKSDB_LITE
  const std::array<size_t, kNumCacheEntryRoles> GetCacheEntryRoleCountsBg() {
    // Verify in cache entry role stats
    ColumnFamilyHandleImpl* cfh =
        static_cast<ColumnFamilyHandleImpl*>(dbfull()->DefaultColumnFamily());
    InternalStats* internal_stats_ptr = cfh->cfd()->internal_stats();
    InternalStats::CacheEntryRoleStats stats;
    internal_stats_ptr->TEST_GetCacheEntryRoleStats(&stats,
                                                    /*foreground=*/false);
    return stats.entry_counts;
  }
#endif  // ROCKSDB_LITE
};

TEST_F(DBBlockCacheTest, IteratorBlockCacheUsage) {
  ReadOptions read_options;
  read_options.fill_cache = false;
  auto table_options = GetTableOptions();
  auto options = GetOptions(table_options);
  InitTable(options);

  LRUCacheOptions co;
  co.capacity = 0;
  co.num_shard_bits = 0;
  co.strict_capacity_limit = false;
  // Needed not to count entry stats collector
  co.metadata_charge_policy = kDontChargeCacheMetadata;
  std::shared_ptr<Cache> cache = NewLRUCache(co);
  table_options.block_cache = cache;
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  Reopen(options);
  RecordCacheCounters(options);

  std::vector<std::unique_ptr<Iterator>> iterators(kNumBlocks - 1);
  Iterator* iter = nullptr;

  ASSERT_EQ(0, cache->GetUsage());
  iter = db_->NewIterator(read_options);
  iter->Seek(ToString(0));
  ASSERT_LT(0, cache->GetUsage());
  delete iter;
  iter = nullptr;
  ASSERT_EQ(0, cache->GetUsage());
}

TEST_F(DBBlockCacheTest, TestWithoutCompressedBlockCache) {
  ReadOptions read_options;
  auto table_options = GetTableOptions();
  auto options = GetOptions(table_options);
  InitTable(options);

  LRUCacheOptions co;
  co.capacity = 0;
  co.num_shard_bits = 0;
  co.strict_capacity_limit = false;
  // Needed not to count entry stats collector
  co.metadata_charge_policy = kDontChargeCacheMetadata;
  std::shared_ptr<Cache> cache = NewLRUCache(co);
  table_options.block_cache = cache;
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  Reopen(options);
  RecordCacheCounters(options);

  std::vector<std::unique_ptr<Iterator>> iterators(kNumBlocks - 1);
  Iterator* iter = nullptr;

  // Load blocks into cache.
  for (size_t i = 0; i + 1 < kNumBlocks; i++) {
    iter = db_->NewIterator(read_options);
    iter->Seek(ToString(i));
    ASSERT_OK(iter->status());
    CheckCacheCounters(options, 1, 0, 1, 0);
    iterators[i].reset(iter);
  }
  size_t usage = cache->GetUsage();
  ASSERT_LT(0, usage);
  cache->SetCapacity(usage);
  ASSERT_EQ(usage, cache->GetPinnedUsage());

  // Test with strict capacity limit.
  cache->SetStrictCapacityLimit(true);
  iter = db_->NewIterator(read_options);
  iter->Seek(ToString(kNumBlocks - 1));
  ASSERT_TRUE(iter->status().IsIncomplete());
  CheckCacheCounters(options, 1, 0, 0, 1);
  delete iter;
  iter = nullptr;

  // Release iterators and access cache again.
  for (size_t i = 0; i + 1 < kNumBlocks; i++) {
    iterators[i].reset();
    CheckCacheCounters(options, 0, 0, 0, 0);
  }
  ASSERT_EQ(0, cache->GetPinnedUsage());
  for (size_t i = 0; i + 1 < kNumBlocks; i++) {
    iter = db_->NewIterator(read_options);
    iter->Seek(ToString(i));
    ASSERT_OK(iter->status());
    CheckCacheCounters(options, 0, 1, 0, 0);
    iterators[i].reset(iter);
  }
}

#ifdef SNAPPY
TEST_F(DBBlockCacheTest, TestWithCompressedBlockCache) {
  Options options = CurrentOptions();
  options.create_if_missing = true;
  options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();

  BlockBasedTableOptions table_options;
  table_options.no_block_cache = true;
  table_options.block_cache_compressed = nullptr;
  table_options.block_size = 1;
  table_options.filter_policy.reset(NewBloomFilterPolicy(20));
  table_options.cache_index_and_filter_blocks = false;
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  options.compression = CompressionType::kSnappyCompression;

  DestroyAndReopen(options);

  std::string value(kValueSize, 'a');
  for (size_t i = 0; i < kNumBlocks; i++) {
    ASSERT_OK(Put(ToString(i), value));
    ASSERT_OK(Flush());
  }

  ReadOptions read_options;
  std::shared_ptr<Cache> compressed_cache = NewLRUCache(1 << 25, 0, false);
  LRUCacheOptions co;
  co.capacity = 0;
  co.num_shard_bits = 0;
  co.strict_capacity_limit = false;
  // Needed not to count entry stats collector
  co.metadata_charge_policy = kDontChargeCacheMetadata;
  std::shared_ptr<Cache> cache = NewLRUCache(co);
  table_options.block_cache = cache;
  table_options.no_block_cache = false;
  table_options.block_cache_compressed = compressed_cache;
  table_options.max_auto_readahead_size = 0;
  table_options.cache_index_and_filter_blocks = false;
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  Reopen(options);
  RecordCacheCounters(options);

  // Load blocks into cache.
  for (size_t i = 0; i < kNumBlocks - 1; i++) {
    ASSERT_EQ(value, Get(ToString(i)));
    CheckCacheCounters(options, 1, 0, 1, 0);
    CheckCompressedCacheCounters(options, 1, 0, 1, 0);
  }

  size_t usage = cache->GetUsage();
  ASSERT_EQ(0, usage);
  ASSERT_EQ(usage, cache->GetPinnedUsage());
  size_t compressed_usage = compressed_cache->GetUsage();
  ASSERT_LT(0, compressed_usage);
  // Compressed block cache cannot be pinned.
  ASSERT_EQ(0, compressed_cache->GetPinnedUsage());

  // Set strict capacity limit flag. Now block will only load into compressed
  // block cache.
  cache->SetCapacity(usage);
  cache->SetStrictCapacityLimit(true);
  ASSERT_EQ(usage, cache->GetPinnedUsage());

  // Load last key block.
  ASSERT_EQ("Result incomplete: Insert failed due to LRU cache being full.",
            Get(ToString(kNumBlocks - 1)));
  // Failure will also record the miss counter.
  CheckCacheCounters(options, 1, 0, 0, 1);
  CheckCompressedCacheCounters(options, 1, 0, 1, 0);

  // Clear strict capacity limit flag. This time we shall hit compressed block
  // cache and load into block cache.
  cache->SetStrictCapacityLimit(false);
  // Load last key block.
  ASSERT_EQ(value, Get(ToString(kNumBlocks - 1)));
  CheckCacheCounters(options, 1, 0, 1, 0);
  CheckCompressedCacheCounters(options, 0, 1, 0, 0);
}

namespace {
class PersistentCacheFromCache : public PersistentCache {
 public:
  PersistentCacheFromCache(std::shared_ptr<Cache> cache, bool read_only)
      : cache_(cache), read_only_(read_only) {}

  Status Insert(const Slice& key, const char* data,
                const size_t size) override {
    if (read_only_) {
      return Status::NotSupported();
    }
    std::unique_ptr<char[]> copy{new char[size]};
    std::copy_n(data, size, copy.get());
    Status s = cache_->Insert(
        key, copy.get(), size,
        GetCacheEntryDeleterForRole<char[], CacheEntryRole::kMisc>());
    if (s.ok()) {
      copy.release();
    }
    return s;
  }

  Status Lookup(const Slice& key, std::unique_ptr<char[]>* data,
                size_t* size) override {
    auto handle = cache_->Lookup(key);
    if (handle) {
      char* ptr = static_cast<char*>(cache_->Value(handle));
      *size = cache_->GetCharge(handle);
      data->reset(new char[*size]);
      std::copy_n(ptr, *size, data->get());
      cache_->Release(handle);
      return Status::OK();
    } else {
      return Status::NotFound();
    }
  }

  bool IsCompressed() override { return false; }

  StatsType Stats() override { return StatsType(); }

  std::string GetPrintableOptions() const override { return ""; }

  uint64_t NewId() override { return cache_->NewId(); }

 private:
  std::shared_ptr<Cache> cache_;
  bool read_only_;
};

class ReadOnlyCacheWrapper : public CacheWrapper {
  using CacheWrapper::CacheWrapper;

  using Cache::Insert;
  Status Insert(const Slice& /*key*/, void* /*value*/, size_t /*charge*/,
                void (*)(const Slice& key, void* value) /*deleter*/,
                Handle** /*handle*/, Priority /*priority*/) override {
    return Status::NotSupported();
  }
};

}  // namespace

TEST_F(DBBlockCacheTest, TestWithSameCompressed) {
  auto table_options = GetTableOptions();
  auto options = GetOptions(table_options);
  InitTable(options);

  std::shared_ptr<Cache> rw_cache{NewLRUCache(1000000)};
  std::shared_ptr<PersistentCacheFromCache> rw_pcache{
      new PersistentCacheFromCache(rw_cache, /*read_only*/ false)};
  // Exercise some obscure behavior with read-only wrappers
  std::shared_ptr<Cache> ro_cache{new ReadOnlyCacheWrapper(rw_cache)};
  std::shared_ptr<PersistentCacheFromCache> ro_pcache{
      new PersistentCacheFromCache(rw_cache, /*read_only*/ true)};

  // Simple same pointer
  table_options.block_cache = rw_cache;
  table_options.block_cache_compressed = rw_cache;
  table_options.persistent_cache.reset();
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  ASSERT_EQ(TryReopen(options).ToString(),
            "Invalid argument: block_cache same as block_cache_compressed not "
            "currently supported, and would be bad for performance anyway");

  // Other cases
  table_options.block_cache = ro_cache;
  table_options.block_cache_compressed = rw_cache;
  table_options.persistent_cache.reset();
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  ASSERT_EQ(TryReopen(options).ToString(),
            "Invalid argument: block_cache and block_cache_compressed share "
            "the same key space, which is not supported");

  table_options.block_cache = rw_cache;
  table_options.block_cache_compressed = ro_cache;
  table_options.persistent_cache.reset();
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  ASSERT_EQ(TryReopen(options).ToString(),
            "Invalid argument: block_cache_compressed and block_cache share "
            "the same key space, which is not supported");

  table_options.block_cache = ro_cache;
  table_options.block_cache_compressed.reset();
  table_options.persistent_cache = rw_pcache;
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  ASSERT_EQ(TryReopen(options).ToString(),
            "Invalid argument: block_cache and persistent_cache share the same "
            "key space, which is not supported");

  table_options.block_cache = rw_cache;
  table_options.block_cache_compressed.reset();
  table_options.persistent_cache = ro_pcache;
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  ASSERT_EQ(TryReopen(options).ToString(),
            "Invalid argument: persistent_cache and block_cache share the same "
            "key space, which is not supported");

  table_options.block_cache.reset();
  table_options.no_block_cache = true;
  table_options.block_cache_compressed = ro_cache;
  table_options.persistent_cache = rw_pcache;
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  ASSERT_EQ(TryReopen(options).ToString(),
            "Invalid argument: block_cache_compressed and persistent_cache "
            "share the same key space, which is not supported");

  table_options.block_cache.reset();
  table_options.no_block_cache = true;
  table_options.block_cache_compressed = rw_cache;
  table_options.persistent_cache = ro_pcache;
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  ASSERT_EQ(TryReopen(options).ToString(),
            "Invalid argument: persistent_cache and block_cache_compressed "
            "share the same key space, which is not supported");
}
#endif  // SNAPPY

#ifndef ROCKSDB_LITE

// Make sure that when options.block_cache is set, after a new table is
// created its index/filter blocks are added to block cache.
TEST_F(DBBlockCacheTest, IndexAndFilterBlocksOfNewTableAddedToCache) {
  Options options = CurrentOptions();
  options.create_if_missing = true;
  options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();
  BlockBasedTableOptions table_options;
  table_options.cache_index_and_filter_blocks = true;
  table_options.filter_policy.reset(NewBloomFilterPolicy(20));
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  CreateAndReopenWithCF({"pikachu"}, options);

  ASSERT_OK(Put(1, "key", "val"));
  // Create a new table.
  ASSERT_OK(Flush(1));

  // index/filter blocks added to block cache right after table creation.
  ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_INDEX_MISS));
  ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_FILTER_MISS));
  ASSERT_EQ(2, /* only index/filter were added */
            TestGetTickerCount(options, BLOCK_CACHE_ADD));
  ASSERT_EQ(0, TestGetTickerCount(options, BLOCK_CACHE_DATA_MISS));
  uint64_t int_num;
  ASSERT_TRUE(
      dbfull()->GetIntProperty("rocksdb.estimate-table-readers-mem", &int_num));
  ASSERT_EQ(int_num, 0U);

  // Make sure filter block is in cache.
  std::string value;
  ReadOptions ropt;
  db_->KeyMayExist(ReadOptions(), handles_[1], "key", &value);

  // Miss count should remain the same.
  ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_FILTER_MISS));
  ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_FILTER_HIT));

  db_->KeyMayExist(ReadOptions(), handles_[1], "key", &value);
  ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_FILTER_MISS));
  ASSERT_EQ(2, TestGetTickerCount(options, BLOCK_CACHE_FILTER_HIT));

  // Make sure index block is in cache.
  auto index_block_hit = TestGetTickerCount(options, BLOCK_CACHE_INDEX_HIT);
  value = Get(1, "key");
  ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_INDEX_MISS));
  ASSERT_EQ(index_block_hit + 1,
            TestGetTickerCount(options, BLOCK_CACHE_INDEX_HIT));

  value = Get(1, "key");
  ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_INDEX_MISS));
  ASSERT_EQ(index_block_hit + 2,
            TestGetTickerCount(options, BLOCK_CACHE_INDEX_HIT));
}

// With fill_cache = false, fills up the cache, then iterates over the entire
// db, verify dummy entries inserted in `BlockBasedTable::NewDataBlockIterator`
// does not cause heap-use-after-free errors in COMPILE_WITH_ASAN=1 runs
TEST_F(DBBlockCacheTest, FillCacheAndIterateDB) {
  ReadOptions read_options;
  read_options.fill_cache = false;
  auto table_options = GetTableOptions();
  auto options = GetOptions(table_options);
  InitTable(options);

  std::shared_ptr<Cache> cache = NewLRUCache(10, 0, true);
  table_options.block_cache = cache;
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  Reopen(options);
  ASSERT_OK(Put("key1", "val1"));
  ASSERT_OK(Put("key2", "val2"));
  ASSERT_OK(Flush());
  ASSERT_OK(Put("key3", "val3"));
  ASSERT_OK(Put("key4", "val4"));
  ASSERT_OK(Flush());
  ASSERT_OK(Put("key5", "val5"));
  ASSERT_OK(Put("key6", "val6"));
  ASSERT_OK(Flush());

  Iterator* iter = nullptr;

  iter = db_->NewIterator(read_options);
  iter->Seek(ToString(0));
  while (iter->Valid()) {
    iter->Next();
  }
  delete iter;
  iter = nullptr;
}

TEST_F(DBBlockCacheTest, IndexAndFilterBlocksStats) {
  Options options = CurrentOptions();
  options.create_if_missing = true;
  options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();
  BlockBasedTableOptions table_options;
  table_options.cache_index_and_filter_blocks = true;
  LRUCacheOptions co;
  // 500 bytes are enough to hold the first two blocks
  co.capacity = 500;
  co.num_shard_bits = 0;
  co.strict_capacity_limit = false;
  co.metadata_charge_policy = kDontChargeCacheMetadata;
  std::shared_ptr<Cache> cache = NewLRUCache(co);
  table_options.block_cache = cache;
  table_options.filter_policy.reset(NewBloomFilterPolicy(20, true));
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  CreateAndReopenWithCF({"pikachu"}, options);

  ASSERT_OK(Put(1, "longer_key", "val"));
  // Create a new table
  ASSERT_OK(Flush(1));
  size_t index_bytes_insert =
      TestGetTickerCount(options, BLOCK_CACHE_INDEX_BYTES_INSERT);
  size_t filter_bytes_insert =
      TestGetTickerCount(options, BLOCK_CACHE_FILTER_BYTES_INSERT);
  ASSERT_GT(index_bytes_insert, 0);
  ASSERT_GT(filter_bytes_insert, 0);
  ASSERT_EQ(cache->GetUsage(), index_bytes_insert + filter_bytes_insert);
  // set the cache capacity to the current usage
  cache->SetCapacity(index_bytes_insert + filter_bytes_insert);
  // The index and filter eviction statistics were broken by the refactoring
  // that moved the readers out of the block cache. Disabling these until we can
  // bring the stats back.
  // ASSERT_EQ(TestGetTickerCount(options, BLOCK_CACHE_INDEX_BYTES_EVICT), 0);
  // ASSERT_EQ(TestGetTickerCount(options, BLOCK_CACHE_FILTER_BYTES_EVICT), 0);
  // Note that the second key needs to be no longer than the first one.
  // Otherwise the second index block may not fit in cache.
  ASSERT_OK(Put(1, "key", "val"));
  // Create a new table
  ASSERT_OK(Flush(1));
  // cache evicted old index and block entries
  ASSERT_GT(TestGetTickerCount(options, BLOCK_CACHE_INDEX_BYTES_INSERT),
            index_bytes_insert);
  ASSERT_GT(TestGetTickerCount(options, BLOCK_CACHE_FILTER_BYTES_INSERT),
            filter_bytes_insert);
  // The index and filter eviction statistics were broken by the refactoring
  // that moved the readers out of the block cache. Disabling these until we can
  // bring the stats back.
  // ASSERT_EQ(TestGetTickerCount(options, BLOCK_CACHE_INDEX_BYTES_EVICT),
  //           index_bytes_insert);
  // ASSERT_EQ(TestGetTickerCount(options, BLOCK_CACHE_FILTER_BYTES_EVICT),
  //           filter_bytes_insert);
}

#if (defined OS_LINUX || defined OS_WIN)
TEST_F(DBBlockCacheTest, WarmCacheWithDataBlocksDuringFlush) {
  Options options = CurrentOptions();
  options.create_if_missing = true;
  options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();

  BlockBasedTableOptions table_options;
  table_options.block_cache = NewLRUCache(1 << 25, 0, false);
  table_options.cache_index_and_filter_blocks = false;
  table_options.prepopulate_block_cache =
      BlockBasedTableOptions::PrepopulateBlockCache::kFlushOnly;
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  DestroyAndReopen(options);

  std::string value(kValueSize, 'a');
  for (size_t i = 1; i <= kNumBlocks; i++) {
    ASSERT_OK(Put(ToString(i), value));
    ASSERT_OK(Flush());
    ASSERT_EQ(i, options.statistics->getTickerCount(BLOCK_CACHE_DATA_ADD));
    ASSERT_EQ(value, Get(ToString(i)));
    ASSERT_EQ(0, options.statistics->getTickerCount(BLOCK_CACHE_DATA_MISS));
    ASSERT_EQ(i, options.statistics->getTickerCount(BLOCK_CACHE_DATA_HIT));
  }
  // Verify compaction not counted
  ASSERT_OK(db_->CompactRange(CompactRangeOptions(), /*begin=*/nullptr,
                              /*end=*/nullptr));
  EXPECT_EQ(kNumBlocks,
            options.statistics->getTickerCount(BLOCK_CACHE_DATA_ADD));
}

// This test cache data, index and filter blocks during flush.
class DBBlockCacheTest1 : public DBTestBase,
                          public ::testing::WithParamInterface<uint32_t> {
 public:
  const size_t kNumBlocks = 10;
  const size_t kValueSize = 100;
  DBBlockCacheTest1() : DBTestBase("db_block_cache_test1", true) {}
};

INSTANTIATE_TEST_CASE_P(DBBlockCacheTest1, DBBlockCacheTest1,
                        ::testing::Values(1, 2, 3));

TEST_P(DBBlockCacheTest1, WarmCacheWithBlocksDuringFlush) {
  Options options = CurrentOptions();
  options.create_if_missing = true;
  options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();

  BlockBasedTableOptions table_options;
  table_options.block_cache = NewLRUCache(1 << 25, 0, false);

  uint32_t filter_type = GetParam();
  switch (filter_type) {
    case 1:  // partition_filter
      table_options.partition_filters = true;
      table_options.index_type =
          BlockBasedTableOptions::IndexType::kTwoLevelIndexSearch;
      table_options.filter_policy.reset(NewBloomFilterPolicy(10, false));
      break;
    case 2:  // block-based filter
      table_options.filter_policy.reset(NewBloomFilterPolicy(10, true));
      break;
    case 3:  // full filter
      table_options.filter_policy.reset(NewBloomFilterPolicy(10, false));
      break;
    default:
      assert(false);
  }

  table_options.cache_index_and_filter_blocks = true;
  table_options.prepopulate_block_cache =
      BlockBasedTableOptions::PrepopulateBlockCache::kFlushOnly;
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  DestroyAndReopen(options);

  std::string value(kValueSize, 'a');
  for (size_t i = 1; i <= kNumBlocks; i++) {
    ASSERT_OK(Put(ToString(i), value));
    ASSERT_OK(Flush());
    ASSERT_EQ(i, options.statistics->getTickerCount(BLOCK_CACHE_DATA_ADD));
    if (filter_type == 1) {
      ASSERT_EQ(2 * i,
                options.statistics->getTickerCount(BLOCK_CACHE_INDEX_ADD));
      ASSERT_EQ(2 * i,
                options.statistics->getTickerCount(BLOCK_CACHE_FILTER_ADD));
    } else {
      ASSERT_EQ(i, options.statistics->getTickerCount(BLOCK_CACHE_INDEX_ADD));
      ASSERT_EQ(i, options.statistics->getTickerCount(BLOCK_CACHE_FILTER_ADD));
    }
    ASSERT_EQ(value, Get(ToString(i)));

    ASSERT_EQ(0, options.statistics->getTickerCount(BLOCK_CACHE_DATA_MISS));
    ASSERT_EQ(i, options.statistics->getTickerCount(BLOCK_CACHE_DATA_HIT));

    ASSERT_EQ(0, options.statistics->getTickerCount(BLOCK_CACHE_INDEX_MISS));
    ASSERT_EQ(i * 3, options.statistics->getTickerCount(BLOCK_CACHE_INDEX_HIT));
    if (filter_type == 1) {
      ASSERT_EQ(i * 3,
                options.statistics->getTickerCount(BLOCK_CACHE_FILTER_HIT));
    } else {
      ASSERT_EQ(i * 2,
                options.statistics->getTickerCount(BLOCK_CACHE_FILTER_HIT));
    }
    ASSERT_EQ(0, options.statistics->getTickerCount(BLOCK_CACHE_FILTER_MISS));
  }

  // Verify compaction not counted
  ASSERT_OK(db_->CompactRange(CompactRangeOptions(), /*begin=*/nullptr,
                              /*end=*/nullptr));
  EXPECT_EQ(kNumBlocks,
            options.statistics->getTickerCount(BLOCK_CACHE_DATA_ADD));
  // Index and filter blocks are automatically warmed when the new table file
  // is automatically opened at the end of compaction. This is not easily
  // disabled so results in the new index and filter blocks being warmed.
  if (filter_type == 1) {
    EXPECT_EQ(2 * (1 + kNumBlocks),
              options.statistics->getTickerCount(BLOCK_CACHE_INDEX_ADD));
    EXPECT_EQ(2 * (1 + kNumBlocks),
              options.statistics->getTickerCount(BLOCK_CACHE_FILTER_ADD));
  } else {
    EXPECT_EQ(1 + kNumBlocks,
              options.statistics->getTickerCount(BLOCK_CACHE_INDEX_ADD));
    EXPECT_EQ(1 + kNumBlocks,
              options.statistics->getTickerCount(BLOCK_CACHE_FILTER_ADD));
  }
}

TEST_F(DBBlockCacheTest, DynamicallyWarmCacheDuringFlush) {
  Options options = CurrentOptions();
  options.create_if_missing = true;
  options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();

  BlockBasedTableOptions table_options;
  table_options.block_cache = NewLRUCache(1 << 25, 0, false);
  table_options.cache_index_and_filter_blocks = false;
  table_options.prepopulate_block_cache =
      BlockBasedTableOptions::PrepopulateBlockCache::kFlushOnly;

  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  DestroyAndReopen(options);

  std::string value(kValueSize, 'a');

  for (size_t i = 1; i <= 5; i++) {
    ASSERT_OK(Put(ToString(i), value));
    ASSERT_OK(Flush());
    ASSERT_EQ(1,
              options.statistics->getAndResetTickerCount(BLOCK_CACHE_DATA_ADD));

    ASSERT_EQ(value, Get(ToString(i)));
    ASSERT_EQ(0,
              options.statistics->getAndResetTickerCount(BLOCK_CACHE_DATA_ADD));
    ASSERT_EQ(
        0, options.statistics->getAndResetTickerCount(BLOCK_CACHE_DATA_MISS));
    ASSERT_EQ(1,
              options.statistics->getAndResetTickerCount(BLOCK_CACHE_DATA_HIT));
  }

  ASSERT_OK(dbfull()->SetOptions(
      {{"block_based_table_factory", "{prepopulate_block_cache=kDisable;}"}}));

  for (size_t i = 6; i <= kNumBlocks; i++) {
    ASSERT_OK(Put(ToString(i), value));
    ASSERT_OK(Flush());
    ASSERT_EQ(0,
              options.statistics->getAndResetTickerCount(BLOCK_CACHE_DATA_ADD));

    ASSERT_EQ(value, Get(ToString(i)));
    ASSERT_EQ(1,
              options.statistics->getAndResetTickerCount(BLOCK_CACHE_DATA_ADD));
    ASSERT_EQ(
        1, options.statistics->getAndResetTickerCount(BLOCK_CACHE_DATA_MISS));
    ASSERT_EQ(0,
              options.statistics->getAndResetTickerCount(BLOCK_CACHE_DATA_HIT));
  }
}
#endif

namespace {

// A mock cache wraps LRUCache, and record how many entries have been
// inserted for each priority.
class MockCache : public LRUCache {
 public:
  static uint32_t high_pri_insert_count;
  static uint32_t low_pri_insert_count;

  MockCache()
      : LRUCache((size_t)1 << 25 /*capacity*/, 0 /*num_shard_bits*/,
                 false /*strict_capacity_limit*/, 0.0 /*high_pri_pool_ratio*/) {
  }

  using ShardedCache::Insert;

  Status Insert(const Slice& key, void* value,
                const Cache::CacheItemHelper* helper_cb, size_t charge,
                Handle** handle, Priority priority) override {
    DeleterFn delete_cb = helper_cb->del_cb;
    if (priority == Priority::LOW) {
      low_pri_insert_count++;
    } else {
      high_pri_insert_count++;
    }
    return LRUCache::Insert(key, value, charge, delete_cb, handle, priority);
  }
};

uint32_t MockCache::high_pri_insert_count = 0;
uint32_t MockCache::low_pri_insert_count = 0;

}  // anonymous namespace

TEST_F(DBBlockCacheTest, IndexAndFilterBlocksCachePriority) {
  for (auto priority : {Cache::Priority::LOW, Cache::Priority::HIGH}) {
    Options options = CurrentOptions();
    options.create_if_missing = true;
    options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();
    BlockBasedTableOptions table_options;
    table_options.cache_index_and_filter_blocks = true;
    table_options.block_cache.reset(new MockCache());
    table_options.filter_policy.reset(NewBloomFilterPolicy(20));
    table_options.cache_index_and_filter_blocks_with_high_priority =
        priority == Cache::Priority::HIGH ? true : false;
    options.table_factory.reset(NewBlockBasedTableFactory(table_options));
    DestroyAndReopen(options);

    MockCache::high_pri_insert_count = 0;
    MockCache::low_pri_insert_count = 0;

    // Create a new table.
    ASSERT_OK(Put("foo", "value"));
    ASSERT_OK(Put("bar", "value"));
    ASSERT_OK(Flush());
    ASSERT_EQ(1, NumTableFilesAtLevel(0));

    // index/filter blocks added to block cache right after table creation.
    ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_INDEX_MISS));
    ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_FILTER_MISS));
    ASSERT_EQ(2, /* only index/filter were added */
              TestGetTickerCount(options, BLOCK_CACHE_ADD));
    ASSERT_EQ(0, TestGetTickerCount(options, BLOCK_CACHE_DATA_MISS));
    if (priority == Cache::Priority::LOW) {
      ASSERT_EQ(0u, MockCache::high_pri_insert_count);
      ASSERT_EQ(2u, MockCache::low_pri_insert_count);
    } else {
      ASSERT_EQ(2u, MockCache::high_pri_insert_count);
      ASSERT_EQ(0u, MockCache::low_pri_insert_count);
    }

    // Access data block.
    ASSERT_EQ("value", Get("foo"));

    ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_INDEX_MISS));
    ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_FILTER_MISS));
    ASSERT_EQ(3, /*adding data block*/
              TestGetTickerCount(options, BLOCK_CACHE_ADD));
    ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_DATA_MISS));

    // Data block should be inserted with low priority.
    if (priority == Cache::Priority::LOW) {
      ASSERT_EQ(0u, MockCache::high_pri_insert_count);
      ASSERT_EQ(3u, MockCache::low_pri_insert_count);
    } else {
      ASSERT_EQ(2u, MockCache::high_pri_insert_count);
      ASSERT_EQ(1u, MockCache::low_pri_insert_count);
    }
  }
}

namespace {

// An LRUCache wrapper that can falsely report "not found" on Lookup.
// This allows us to manipulate BlockBasedTableReader into thinking
// another thread inserted the data in between Lookup and Insert,
// while mostly preserving the LRUCache interface/behavior.
class LookupLiarCache : public CacheWrapper {
  int nth_lookup_not_found_ = 0;

 public:
  explicit LookupLiarCache(std::shared_ptr<Cache> target)
      : CacheWrapper(std::move(target)) {}

  using Cache::Lookup;
  Handle* Lookup(const Slice& key, Statistics* stats) override {
    if (nth_lookup_not_found_ == 1) {
      nth_lookup_not_found_ = 0;
      return nullptr;
    }
    if (nth_lookup_not_found_ > 1) {
      --nth_lookup_not_found_;
    }
    return CacheWrapper::Lookup(key, stats);
  }

  // 1 == next lookup, 2 == after next, etc.
  void SetNthLookupNotFound(int n) { nth_lookup_not_found_ = n; }
};

}  // anonymous namespace

TEST_F(DBBlockCacheTest, AddRedundantStats) {
  const size_t capacity = size_t{1} << 25;
  const int num_shard_bits = 0;  // 1 shard
  int iterations_tested = 0;
  for (std::shared_ptr<Cache> base_cache :
       {NewLRUCache(capacity, num_shard_bits),
        NewClockCache(capacity, num_shard_bits)}) {
    if (!base_cache) {
      // Skip clock cache when not supported
      continue;
    }
    ++iterations_tested;
    Options options = CurrentOptions();
    options.create_if_missing = true;
    options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();

    std::shared_ptr<LookupLiarCache> cache =
        std::make_shared<LookupLiarCache>(base_cache);

    BlockBasedTableOptions table_options;
    table_options.cache_index_and_filter_blocks = true;
    table_options.block_cache = cache;
    table_options.filter_policy.reset(NewBloomFilterPolicy(50));
    options.table_factory.reset(NewBlockBasedTableFactory(table_options));
    DestroyAndReopen(options);

    // Create a new table.
    ASSERT_OK(Put("foo", "value"));
    ASSERT_OK(Put("bar", "value"));
    ASSERT_OK(Flush());
    ASSERT_EQ(1, NumTableFilesAtLevel(0));

    // Normal access filter+index+data.
    ASSERT_EQ("value", Get("foo"));

    ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_INDEX_ADD));
    ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_FILTER_ADD));
    ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_DATA_ADD));
    // --------
    ASSERT_EQ(3, TestGetTickerCount(options, BLOCK_CACHE_ADD));

    ASSERT_EQ(0, TestGetTickerCount(options, BLOCK_CACHE_INDEX_ADD_REDUNDANT));
    ASSERT_EQ(0, TestGetTickerCount(options, BLOCK_CACHE_FILTER_ADD_REDUNDANT));
    ASSERT_EQ(0, TestGetTickerCount(options, BLOCK_CACHE_DATA_ADD_REDUNDANT));
    // --------
    ASSERT_EQ(0, TestGetTickerCount(options, BLOCK_CACHE_ADD_REDUNDANT));

    // Againt access filter+index+data, but force redundant load+insert on index
    cache->SetNthLookupNotFound(2);
    ASSERT_EQ("value", Get("bar"));

    ASSERT_EQ(2, TestGetTickerCount(options, BLOCK_CACHE_INDEX_ADD));
    ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_FILTER_ADD));
    ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_DATA_ADD));
    // --------
    ASSERT_EQ(4, TestGetTickerCount(options, BLOCK_CACHE_ADD));

    ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_INDEX_ADD_REDUNDANT));
    ASSERT_EQ(0, TestGetTickerCount(options, BLOCK_CACHE_FILTER_ADD_REDUNDANT));
    ASSERT_EQ(0, TestGetTickerCount(options, BLOCK_CACHE_DATA_ADD_REDUNDANT));
    // --------
    ASSERT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_ADD_REDUNDANT));

    // Access just filter (with high probability), and force redundant
    // load+insert
    cache->SetNthLookupNotFound(1);
    ASSERT_EQ("NOT_FOUND", Get("this key was not added"));

    EXPECT_EQ(2, TestGetTickerCount(options, BLOCK_CACHE_INDEX_ADD));
    EXPECT_EQ(2, TestGetTickerCount(options, BLOCK_CACHE_FILTER_ADD));
    EXPECT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_DATA_ADD));
    // --------
    EXPECT_EQ(5, TestGetTickerCount(options, BLOCK_CACHE_ADD));

    EXPECT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_INDEX_ADD_REDUNDANT));
    EXPECT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_FILTER_ADD_REDUNDANT));
    EXPECT_EQ(0, TestGetTickerCount(options, BLOCK_CACHE_DATA_ADD_REDUNDANT));
    // --------
    EXPECT_EQ(2, TestGetTickerCount(options, BLOCK_CACHE_ADD_REDUNDANT));

    // Access just data, forcing redundant load+insert
    ReadOptions read_options;
    std::unique_ptr<Iterator> iter{db_->NewIterator(read_options)};
    cache->SetNthLookupNotFound(1);
    iter->SeekToFirst();
    ASSERT_TRUE(iter->Valid());
    ASSERT_EQ(iter->key(), "bar");

    EXPECT_EQ(2, TestGetTickerCount(options, BLOCK_CACHE_INDEX_ADD));
    EXPECT_EQ(2, TestGetTickerCount(options, BLOCK_CACHE_FILTER_ADD));
    EXPECT_EQ(2, TestGetTickerCount(options, BLOCK_CACHE_DATA_ADD));
    // --------
    EXPECT_EQ(6, TestGetTickerCount(options, BLOCK_CACHE_ADD));

    EXPECT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_INDEX_ADD_REDUNDANT));
    EXPECT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_FILTER_ADD_REDUNDANT));
    EXPECT_EQ(1, TestGetTickerCount(options, BLOCK_CACHE_DATA_ADD_REDUNDANT));
    // --------
    EXPECT_EQ(3, TestGetTickerCount(options, BLOCK_CACHE_ADD_REDUNDANT));
  }
  EXPECT_GE(iterations_tested, 1);
}

TEST_F(DBBlockCacheTest, ParanoidFileChecks) {
  Options options = CurrentOptions();
  options.create_if_missing = true;
  options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();
  options.level0_file_num_compaction_trigger = 2;
  options.paranoid_file_checks = true;
  BlockBasedTableOptions table_options;
  table_options.cache_index_and_filter_blocks = false;
  table_options.filter_policy.reset(NewBloomFilterPolicy(20));
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  CreateAndReopenWithCF({"pikachu"}, options);

  ASSERT_OK(Put(1, "1_key", "val"));
  ASSERT_OK(Put(1, "9_key", "val"));
  // Create a new table.
  ASSERT_OK(Flush(1));
  ASSERT_EQ(1, /* read and cache data block */
            TestGetTickerCount(options, BLOCK_CACHE_ADD));

  ASSERT_OK(Put(1, "1_key2", "val2"));
  ASSERT_OK(Put(1, "9_key2", "val2"));
  // Create a new SST file. This will further trigger a compaction
  // and generate another file.
  ASSERT_OK(Flush(1));
  ASSERT_OK(dbfull()->TEST_WaitForCompact());
  ASSERT_EQ(3, /* Totally 3 files created up to now */
            TestGetTickerCount(options, BLOCK_CACHE_ADD));

  // After disabling options.paranoid_file_checks. NO further block
  // is added after generating a new file.
  ASSERT_OK(
      dbfull()->SetOptions(handles_[1], {{"paranoid_file_checks", "false"}}));

  ASSERT_OK(Put(1, "1_key3", "val3"));
  ASSERT_OK(Put(1, "9_key3", "val3"));
  ASSERT_OK(Flush(1));
  ASSERT_OK(Put(1, "1_key4", "val4"));
  ASSERT_OK(Put(1, "9_key4", "val4"));
  ASSERT_OK(Flush(1));
  ASSERT_OK(dbfull()->TEST_WaitForCompact());
  ASSERT_EQ(3, /* Totally 3 files created up to now */
            TestGetTickerCount(options, BLOCK_CACHE_ADD));
}

TEST_F(DBBlockCacheTest, CompressedCache) {
  if (!Snappy_Supported()) {
    return;
  }
  int num_iter = 80;

  // Run this test three iterations.
  // Iteration 1: only a uncompressed block cache
  // Iteration 2: only a compressed block cache
  // Iteration 3: both block cache and compressed cache
  // Iteration 4: both block cache and compressed cache, but DB is not
  // compressed
  for (int iter = 0; iter < 4; iter++) {
    Options options = CurrentOptions();
    options.write_buffer_size = 64 * 1024;  // small write buffer
    options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();

    BlockBasedTableOptions table_options;
    switch (iter) {
      case 0:
        // only uncompressed block cache
        table_options.block_cache = NewLRUCache(8 * 1024);
        table_options.block_cache_compressed = nullptr;
        options.table_factory.reset(NewBlockBasedTableFactory(table_options));
        break;
      case 1:
        // no block cache, only compressed cache
        table_options.no_block_cache = true;
        table_options.block_cache = nullptr;
        table_options.block_cache_compressed = NewLRUCache(8 * 1024);
        options.table_factory.reset(NewBlockBasedTableFactory(table_options));
        break;
      case 2:
        // both compressed and uncompressed block cache
        table_options.block_cache = NewLRUCache(1024);
        table_options.block_cache_compressed = NewLRUCache(8 * 1024);
        options.table_factory.reset(NewBlockBasedTableFactory(table_options));
        break;
      case 3:
        // both block cache and compressed cache, but DB is not compressed
        // also, make block cache sizes bigger, to trigger block cache hits
        table_options.block_cache = NewLRUCache(1024 * 1024);
        table_options.block_cache_compressed = NewLRUCache(8 * 1024 * 1024);
        options.table_factory.reset(NewBlockBasedTableFactory(table_options));
        options.compression = kNoCompression;
        break;
      default:
        FAIL();
    }
    CreateAndReopenWithCF({"pikachu"}, options);
    // default column family doesn't have block cache
    Options no_block_cache_opts;
    no_block_cache_opts.statistics = options.statistics;
    no_block_cache_opts = CurrentOptions(no_block_cache_opts);
    BlockBasedTableOptions table_options_no_bc;
    table_options_no_bc.no_block_cache = true;
    no_block_cache_opts.table_factory.reset(
        NewBlockBasedTableFactory(table_options_no_bc));
    ReopenWithColumnFamilies(
        {"default", "pikachu"},
        std::vector<Options>({no_block_cache_opts, options}));

    Random rnd(301);

    // Write 8MB (80 values, each 100K)
    ASSERT_EQ(NumTableFilesAtLevel(0, 1), 0);
    std::vector<std::string> values;
    std::string str;
    for (int i = 0; i < num_iter; i++) {
      if (i % 4 == 0) {  // high compression ratio
        str = rnd.RandomString(1000);
      }
      values.push_back(str);
      ASSERT_OK(Put(1, Key(i), values[i]));
    }

    // flush all data from memtable so that reads are from block cache
    ASSERT_OK(Flush(1));

    for (int i = 0; i < num_iter; i++) {
      ASSERT_EQ(Get(1, Key(i)), values[i]);
    }

    // check that we triggered the appropriate code paths in the cache
    switch (iter) {
      case 0:
        // only uncompressed block cache
        ASSERT_GT(TestGetTickerCount(options, BLOCK_CACHE_MISS), 0);
        ASSERT_EQ(TestGetTickerCount(options, BLOCK_CACHE_COMPRESSED_MISS), 0);
        break;
      case 1:
        // no block cache, only compressed cache
        ASSERT_EQ(TestGetTickerCount(options, BLOCK_CACHE_MISS), 0);
        ASSERT_GT(TestGetTickerCount(options, BLOCK_CACHE_COMPRESSED_MISS), 0);
        break;
      case 2:
        // both compressed and uncompressed block cache
        ASSERT_GT(TestGetTickerCount(options, BLOCK_CACHE_MISS), 0);
        ASSERT_GT(TestGetTickerCount(options, BLOCK_CACHE_COMPRESSED_MISS), 0);
        break;
      case 3:
        // both compressed and uncompressed block cache
        ASSERT_GT(TestGetTickerCount(options, BLOCK_CACHE_MISS), 0);
        ASSERT_GT(TestGetTickerCount(options, BLOCK_CACHE_HIT), 0);
        ASSERT_GT(TestGetTickerCount(options, BLOCK_CACHE_COMPRESSED_MISS), 0);
        // compressed doesn't have any hits since blocks are not compressed on
        // storage
        ASSERT_EQ(TestGetTickerCount(options, BLOCK_CACHE_COMPRESSED_HIT), 0);
        break;
      default:
        FAIL();
    }

    options.create_if_missing = true;
    DestroyAndReopen(options);
  }
}

TEST_F(DBBlockCacheTest, CacheCompressionDict) {
  const int kNumFiles = 4;
  const int kNumEntriesPerFile = 128;
  const int kNumBytesPerEntry = 1024;

  // Try all the available libraries that support dictionary compression
  std::vector<CompressionType> compression_types;
  if (Zlib_Supported()) {
    compression_types.push_back(kZlibCompression);
  }
  if (LZ4_Supported()) {
    compression_types.push_back(kLZ4Compression);
    compression_types.push_back(kLZ4HCCompression);
  }
  if (ZSTD_Supported()) {
    compression_types.push_back(kZSTD);
  } else if (ZSTDNotFinal_Supported()) {
    compression_types.push_back(kZSTDNotFinalCompression);
  }
  Random rnd(301);
  for (auto compression_type : compression_types) {
    Options options = CurrentOptions();
    options.bottommost_compression = compression_type;
    options.bottommost_compression_opts.max_dict_bytes = 4096;
    options.bottommost_compression_opts.enabled = true;
    options.create_if_missing = true;
    options.num_levels = 2;
    options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();
    options.target_file_size_base = kNumEntriesPerFile * kNumBytesPerEntry;
    BlockBasedTableOptions table_options;
    table_options.cache_index_and_filter_blocks = true;
    table_options.block_cache.reset(new MockCache());
    options.table_factory.reset(NewBlockBasedTableFactory(table_options));
    DestroyAndReopen(options);

    RecordCacheCountersForCompressionDict(options);

    for (int i = 0; i < kNumFiles; ++i) {
      ASSERT_EQ(i, NumTableFilesAtLevel(0, 0));
      for (int j = 0; j < kNumEntriesPerFile; ++j) {
        std::string value = rnd.RandomString(kNumBytesPerEntry);
        ASSERT_OK(Put(Key(j * kNumFiles + i), value.c_str()));
      }
      ASSERT_OK(Flush());
    }
    ASSERT_OK(dbfull()->TEST_WaitForCompact());
    ASSERT_EQ(0, NumTableFilesAtLevel(0));
    ASSERT_EQ(kNumFiles, NumTableFilesAtLevel(1));

    // Compression dictionary blocks are preloaded.
    CheckCacheCountersForCompressionDict(
        options, kNumFiles /* expected_compression_dict_misses */,
        0 /* expected_compression_dict_hits */,
        kNumFiles /* expected_compression_dict_inserts */);

    // Seek to a key in a file. It should cause the SST's dictionary meta-block
    // to be read.
    RecordCacheCounters(options);
    RecordCacheCountersForCompressionDict(options);
    ReadOptions read_options;
    ASSERT_NE("NOT_FOUND", Get(Key(kNumFiles * kNumEntriesPerFile - 1)));
    // Two block hits: index and dictionary since they are prefetched
    // One block missed/added: data block
    CheckCacheCounters(options, 1 /* expected_misses */, 2 /* expected_hits */,
                       1 /* expected_inserts */, 0 /* expected_failures */);
    CheckCacheCountersForCompressionDict(
        options, 0 /* expected_compression_dict_misses */,
        1 /* expected_compression_dict_hits */,
        0 /* expected_compression_dict_inserts */);
  }
}

static void ClearCache(Cache* cache) {
  auto roles = CopyCacheDeleterRoleMap();
  std::deque<std::string> keys;
  Cache::ApplyToAllEntriesOptions opts;
  auto callback = [&](const Slice& key, void* /*value*/, size_t /*charge*/,
                      Cache::DeleterFn deleter) {
    if (roles.find(deleter) == roles.end()) {
      // Keep the stats collector
      return;
    }
    keys.push_back(key.ToString());
  };
  cache->ApplyToAllEntries(callback, opts);
  for (auto& k : keys) {
    cache->Erase(k);
  }
}

TEST_F(DBBlockCacheTest, CacheEntryRoleStats) {
  const size_t capacity = size_t{1} << 25;
  int iterations_tested = 0;
  for (bool partition : {false, true}) {
    for (std::shared_ptr<Cache> cache :
         {NewLRUCache(capacity), NewClockCache(capacity)}) {
      if (!cache) {
        // Skip clock cache when not supported
        continue;
      }
      ++iterations_tested;

      Options options = CurrentOptions();
      SetTimeElapseOnlySleepOnReopen(&options);
      options.create_if_missing = true;
      options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();
      options.max_open_files = 13;
      options.table_cache_numshardbits = 0;
      // If this wakes up, it could interfere with test
      options.stats_dump_period_sec = 0;

      BlockBasedTableOptions table_options;
      table_options.block_cache = cache;
      table_options.cache_index_and_filter_blocks = true;
      table_options.filter_policy.reset(NewBloomFilterPolicy(50));
      if (partition) {
        table_options.index_type = BlockBasedTableOptions::kTwoLevelIndexSearch;
        table_options.partition_filters = true;
      }
      table_options.metadata_cache_options.top_level_index_pinning =
          PinningTier::kNone;
      table_options.metadata_cache_options.partition_pinning =
          PinningTier::kNone;
      table_options.metadata_cache_options.unpartitioned_pinning =
          PinningTier::kNone;
      options.table_factory.reset(NewBlockBasedTableFactory(table_options));
      DestroyAndReopen(options);

      // Create a new table.
      ASSERT_OK(Put("foo", "value"));
      ASSERT_OK(Put("bar", "value"));
      ASSERT_OK(Flush());

      ASSERT_OK(Put("zfoo", "value"));
      ASSERT_OK(Put("zbar", "value"));
      ASSERT_OK(Flush());

      ASSERT_EQ(2, NumTableFilesAtLevel(0));

      // Fresh cache
      ClearCache(cache.get());

      std::array<size_t, kNumCacheEntryRoles> expected{};
      // For CacheEntryStatsCollector
      expected[static_cast<size_t>(CacheEntryRole::kMisc)] = 1;
      EXPECT_EQ(expected, GetCacheEntryRoleCountsBg());

      std::array<size_t, kNumCacheEntryRoles> prev_expected = expected;

      // First access only filters
      ASSERT_EQ("NOT_FOUND", Get("different from any key added"));
      expected[static_cast<size_t>(CacheEntryRole::kFilterBlock)] += 2;
      if (partition) {
        expected[static_cast<size_t>(CacheEntryRole::kFilterMetaBlock)] += 2;
      }
      // Within some time window, we will get cached entry stats
      EXPECT_EQ(prev_expected, GetCacheEntryRoleCountsBg());
      // Not enough to force a miss
      env_->MockSleepForSeconds(45);
      EXPECT_EQ(prev_expected, GetCacheEntryRoleCountsBg());
      // Enough to force a miss
      env_->MockSleepForSeconds(601);
      EXPECT_EQ(expected, GetCacheEntryRoleCountsBg());

      // Now access index and data block
      ASSERT_EQ("value", Get("foo"));
      expected[static_cast<size_t>(CacheEntryRole::kIndexBlock)]++;
      if (partition) {
        // top-level
        expected[static_cast<size_t>(CacheEntryRole::kIndexBlock)]++;
      }
      expected[static_cast<size_t>(CacheEntryRole::kDataBlock)]++;
      // Enough to force a miss
      env_->MockSleepForSeconds(601);
      // But inject a simulated long scan so that we need a longer
      // interval to force a miss next time.
      SyncPoint::GetInstance()->SetCallBack(
          "CacheEntryStatsCollector::GetStats:AfterApplyToAllEntries",
          [this](void*) {
            // To spend no more than 0.2% of time scanning, we would need
            // interval of at least 10000s
            env_->MockSleepForSeconds(20);
          });
      SyncPoint::GetInstance()->EnableProcessing();
      EXPECT_EQ(expected, GetCacheEntryRoleCountsBg());
      prev_expected = expected;
      SyncPoint::GetInstance()->DisableProcessing();
      SyncPoint::GetInstance()->ClearAllCallBacks();

      // The same for other file
      ASSERT_EQ("value", Get("zfoo"));
      expected[static_cast<size_t>(CacheEntryRole::kIndexBlock)]++;
      if (partition) {
        // top-level
        expected[static_cast<size_t>(CacheEntryRole::kIndexBlock)]++;
      }
      expected[static_cast<size_t>(CacheEntryRole::kDataBlock)]++;
      // Because of the simulated long scan, this is not enough to force
      // a miss
      env_->MockSleepForSeconds(601);
      EXPECT_EQ(prev_expected, GetCacheEntryRoleCountsBg());
      // But this is enough
      env_->MockSleepForSeconds(10000);
      EXPECT_EQ(expected, GetCacheEntryRoleCountsBg());
      prev_expected = expected;

      // Also check the GetProperty interface
      std::map<std::string, std::string> values;
      ASSERT_TRUE(
          db_->GetMapProperty(DB::Properties::kBlockCacheEntryStats, &values));

      EXPECT_EQ(
          ToString(expected[static_cast<size_t>(CacheEntryRole::kIndexBlock)]),
          values["count.index-block"]);
      EXPECT_EQ(
          ToString(expected[static_cast<size_t>(CacheEntryRole::kDataBlock)]),
          values["count.data-block"]);
      EXPECT_EQ(
          ToString(expected[static_cast<size_t>(CacheEntryRole::kFilterBlock)]),
          values["count.filter-block"]);
      EXPECT_EQ(
          ToString(
              prev_expected[static_cast<size_t>(CacheEntryRole::kWriteBuffer)]),
          values["count.write-buffer"]);
      EXPECT_EQ(ToString(expected[static_cast<size_t>(CacheEntryRole::kMisc)]),
                values["count.misc"]);

      // Add one for kWriteBuffer
      {
        WriteBufferManager wbm(size_t{1} << 20, cache);
        wbm.ReserveMem(1024);
        expected[static_cast<size_t>(CacheEntryRole::kWriteBuffer)]++;
        // Now we check that the GetProperty interface is more agressive about
        // re-scanning stats, but not totally aggressive.
        // Within some time window, we will get cached entry stats
        env_->MockSleepForSeconds(1);
        EXPECT_EQ(ToString(prev_expected[static_cast<size_t>(
                      CacheEntryRole::kWriteBuffer)]),
                  values["count.write-buffer"]);
        // Not enough for a "background" miss but enough for a "foreground" miss
        env_->MockSleepForSeconds(45);

        ASSERT_TRUE(db_->GetMapProperty(DB::Properties::kBlockCacheEntryStats,
                                        &values));
        EXPECT_EQ(
            ToString(
                expected[static_cast<size_t>(CacheEntryRole::kWriteBuffer)]),
            values["count.write-buffer"]);
      }
      prev_expected = expected;

      // With collector pinned in cache, we should be able to hit
      // even if the cache is full
      ClearCache(cache.get());
      Cache::Handle* h = nullptr;
      ASSERT_OK(cache->Insert("Fill-it-up", nullptr, capacity + 1,
                              GetNoopDeleterForRole<CacheEntryRole::kMisc>(),
                              &h, Cache::Priority::HIGH));
      ASSERT_GT(cache->GetUsage(), cache->GetCapacity());
      expected = {};
      // For CacheEntryStatsCollector
      expected[static_cast<size_t>(CacheEntryRole::kMisc)] = 1;
      // For Fill-it-up
      expected[static_cast<size_t>(CacheEntryRole::kMisc)]++;
      // Still able to hit on saved stats
      EXPECT_EQ(prev_expected, GetCacheEntryRoleCountsBg());
      // Enough to force a miss
      env_->MockSleepForSeconds(1000);
      EXPECT_EQ(expected, GetCacheEntryRoleCountsBg());

      cache->Release(h);

      // Now we test that the DB mutex is not held during scans, for the ways
      // we know how to (possibly) trigger them. Without a better good way to
      // check this, we simply inject an acquire & release of the DB mutex
      // deep in the stat collection code. If we were already holding the
      // mutex, that is UB that would at least be found by TSAN.
      int scan_count = 0;
      SyncPoint::GetInstance()->SetCallBack(
          "CacheEntryStatsCollector::GetStats:AfterApplyToAllEntries",
          [this, &scan_count](void*) {
            dbfull()->TEST_LockMutex();
            dbfull()->TEST_UnlockMutex();
            ++scan_count;
          });
      SyncPoint::GetInstance()->EnableProcessing();

      // Different things that might trigger a scan, with mock sleeps to
      // force a miss.
      env_->MockSleepForSeconds(10000);
      dbfull()->DumpStats();
      ASSERT_EQ(scan_count, 1);

      env_->MockSleepForSeconds(10000);
      ASSERT_TRUE(
          db_->GetMapProperty(DB::Properties::kBlockCacheEntryStats, &values));
      ASSERT_EQ(scan_count, 2);

      env_->MockSleepForSeconds(10000);
      std::string value_str;
      ASSERT_TRUE(
          db_->GetProperty(DB::Properties::kBlockCacheEntryStats, &value_str));
      ASSERT_EQ(scan_count, 3);

      env_->MockSleepForSeconds(10000);
      ASSERT_TRUE(db_->GetProperty(DB::Properties::kCFStats, &value_str));
      // To match historical speed, querying this property no longer triggers
      // a scan, even if results are old. But periodic dump stats should keep
      // things reasonably updated.
      ASSERT_EQ(scan_count, /*unchanged*/ 3);

      SyncPoint::GetInstance()->DisableProcessing();
      SyncPoint::GetInstance()->ClearAllCallBacks();
    }
    EXPECT_GE(iterations_tested, 1);
  }
}

#endif  // ROCKSDB_LITE

class DBBlockCacheKeyTest
    : public DBTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  DBBlockCacheKeyTest()
      : DBTestBase("db_block_cache_test", /*env_do_fsync=*/false) {}

  void SetUp() override {
    use_compressed_cache_ = std::get<0>(GetParam());
    exclude_file_numbers_ = std::get<1>(GetParam());
  }

  bool use_compressed_cache_;
  bool exclude_file_numbers_;
};

// Disable LinkFile so that we can physically copy a DB using Checkpoint.
// Disable file GetUniqueId to enable stable cache keys.
class StableCacheKeyTestFS : public FaultInjectionTestFS {
 public:
  explicit StableCacheKeyTestFS(const std::shared_ptr<FileSystem>& base)
      : FaultInjectionTestFS(base) {
    SetFailGetUniqueId(true);
  }

  virtual ~StableCacheKeyTestFS() override {}

  IOStatus LinkFile(const std::string&, const std::string&, const IOOptions&,
                    IODebugContext*) override {
    return IOStatus::NotSupported("Disabled");
  }
};

TEST_P(DBBlockCacheKeyTest, StableCacheKeys) {
  std::shared_ptr<StableCacheKeyTestFS> test_fs{
      new StableCacheKeyTestFS(env_->GetFileSystem())};
  std::unique_ptr<CompositeEnvWrapper> test_env{
      new CompositeEnvWrapper(env_, test_fs)};

  Options options = CurrentOptions();
  options.create_if_missing = true;
  options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();
  options.env = test_env.get();

  BlockBasedTableOptions table_options;

  int key_count = 0;
  uint64_t expected_stat = 0;

  std::function<void()> verify_stats;
  if (use_compressed_cache_) {
    if (!Snappy_Supported()) {
      ROCKSDB_GTEST_SKIP("Compressed cache test requires snappy support");
      return;
    }
    options.compression = CompressionType::kSnappyCompression;
    table_options.no_block_cache = true;
    table_options.block_cache_compressed = NewLRUCache(1 << 25, 0, false);
    verify_stats = [&options, &expected_stat] {
      // One for ordinary SST file and one for external SST file
      ASSERT_EQ(expected_stat,
                options.statistics->getTickerCount(BLOCK_CACHE_COMPRESSED_ADD));
    };
  } else {
    table_options.cache_index_and_filter_blocks = true;
    table_options.block_cache = NewLRUCache(1 << 25, 0, false);
    verify_stats = [&options, &expected_stat] {
      ASSERT_EQ(expected_stat,
                options.statistics->getTickerCount(BLOCK_CACHE_DATA_ADD));
      ASSERT_EQ(expected_stat,
                options.statistics->getTickerCount(BLOCK_CACHE_INDEX_ADD));
      ASSERT_EQ(expected_stat,
                options.statistics->getTickerCount(BLOCK_CACHE_FILTER_ADD));
    };
  }

  table_options.filter_policy.reset(NewBloomFilterPolicy(10, false));
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  CreateAndReopenWithCF({"koko"}, options);

  if (exclude_file_numbers_) {
    // Simulate something like old behavior without file numbers in properties.
    // This is a "control" side of the test that also ensures safely degraded
    // behavior on old files.
    ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->SetCallBack(
        "BlockBasedTableBuilder::BlockBasedTableBuilder:PreSetupBaseCacheKey",
        [&](void* arg) {
          TableProperties* props = reinterpret_cast<TableProperties*>(arg);
          props->orig_file_number = 0;
        });
    ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->EnableProcessing();
  }

  std::function<void()> perform_gets = [&key_count, &expected_stat, this]() {
    if (exclude_file_numbers_) {
      // No cache key reuse should happen, because we can't rely on current
      // file number being stable
      expected_stat += key_count;
    } else {
      // Cache keys should be stable
      expected_stat = key_count;
    }
    for (int i = 0; i < key_count; ++i) {
      ASSERT_EQ(Get(1, Key(i)), "abc");
    }
  };

  // Ordinary SST files with same session id
  const std::string something_compressible(500U, 'x');
  for (int i = 0; i < 2; ++i) {
    ASSERT_OK(Put(1, Key(key_count), "abc"));
    ASSERT_OK(Put(1, Key(key_count) + "a", something_compressible));
    ASSERT_OK(Flush(1));
    ++key_count;
  }

#ifndef ROCKSDB_LITE
  // Save an export of those ordinary SST files for later
  std::string export_files_dir = dbname_ + "/exported";
  ExportImportFilesMetaData* metadata_ptr_ = nullptr;
  Checkpoint* checkpoint;
  ASSERT_OK(Checkpoint::Create(db_, &checkpoint));
  ASSERT_OK(checkpoint->ExportColumnFamily(handles_[1], export_files_dir,
                                           &metadata_ptr_));
  ASSERT_NE(metadata_ptr_, nullptr);
  delete checkpoint;
  checkpoint = nullptr;

  // External SST files with same session id
  SstFileWriter sst_file_writer(EnvOptions(), options);
  std::vector<std::string> external;
  for (int i = 0; i < 2; ++i) {
    std::string f = dbname_ + "/external" + ToString(i) + ".sst";
    external.push_back(f);
    ASSERT_OK(sst_file_writer.Open(f));
    ASSERT_OK(sst_file_writer.Put(Key(key_count), "abc"));
    ASSERT_OK(
        sst_file_writer.Put(Key(key_count) + "a", something_compressible));
    ++key_count;
    ExternalSstFileInfo external_info;
    ASSERT_OK(sst_file_writer.Finish(&external_info));
    IngestExternalFileOptions ingest_opts;
    ASSERT_OK(db_->IngestExternalFile(handles_[1], {f}, ingest_opts));
  }

  if (exclude_file_numbers_) {
    // FIXME(peterd): figure out where these extra ADDs are coming from
    options.statistics->recordTick(BLOCK_CACHE_COMPRESSED_ADD,
                                   uint64_t{0} - uint64_t{2});
  }
#endif

  perform_gets();
  verify_stats();

  // Make sure we can cache hit after re-open
  ReopenWithColumnFamilies({"default", "koko"}, options);

  perform_gets();
  verify_stats();

  // Make sure we can cache hit even on a full copy of the DB. Using
  // StableCacheKeyTestFS, Checkpoint will resort to full copy not hard link.
  // (Checkpoint  not available in LITE mode to test this.)
#ifndef ROCKSDB_LITE
  auto db_copy_name = dbname_ + "-copy";
  ASSERT_OK(Checkpoint::Create(db_, &checkpoint));
  ASSERT_OK(checkpoint->CreateCheckpoint(db_copy_name));
  delete checkpoint;

  Close();
  Destroy(options);

  // Switch to the DB copy
  SaveAndRestore<std::string> save_dbname(&dbname_, db_copy_name);
  ReopenWithColumnFamilies({"default", "koko"}, options);

  perform_gets();
  verify_stats();

  // And ensure that re-importing + ingesting the same files into a
  // different DB uses same cache keys
  DestroyAndReopen(options);

  ColumnFamilyHandle* cfh = nullptr;
  ASSERT_OK(db_->CreateColumnFamilyWithImport(ColumnFamilyOptions(), "yoyo",
                                              ImportColumnFamilyOptions(),
                                              *metadata_ptr_, &cfh));
  ASSERT_NE(cfh, nullptr);
  delete cfh;
  cfh = nullptr;
  delete metadata_ptr_;
  metadata_ptr_ = nullptr;

  DestroyDB(export_files_dir, options);

  ReopenWithColumnFamilies({"default", "yoyo"}, options);

  IngestExternalFileOptions ingest_opts;
  ASSERT_OK(db_->IngestExternalFile(handles_[1], {external}, ingest_opts));

  perform_gets();
  verify_stats();
#endif  // !ROCKSDB_LITE

  Close();
  Destroy(options);
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->DisableProcessing();
}

INSTANTIATE_TEST_CASE_P(DBBlockCacheKeyTest, DBBlockCacheKeyTest,
                        ::testing::Combine(::testing::Bool(),
                                           ::testing::Bool()));

class DBBlockCachePinningTest
    : public DBTestBase,
      public testing::WithParamInterface<
          std::tuple<bool, PinningTier, PinningTier, PinningTier>> {
 public:
  DBBlockCachePinningTest()
      : DBTestBase("db_block_cache_test", /*env_do_fsync=*/false) {}

  void SetUp() override {
    partition_index_and_filters_ = std::get<0>(GetParam());
    top_level_index_pinning_ = std::get<1>(GetParam());
    partition_pinning_ = std::get<2>(GetParam());
    unpartitioned_pinning_ = std::get<3>(GetParam());
  }

  bool partition_index_and_filters_;
  PinningTier top_level_index_pinning_;
  PinningTier partition_pinning_;
  PinningTier unpartitioned_pinning_;
};

TEST_P(DBBlockCachePinningTest, TwoLevelDB) {
  // Creates one file in L0 and one file in L1. Both files have enough data that
  // their index and filter blocks are partitioned. The L1 file will also have
  // a compression dictionary (those are trained only during compaction), which
  // must be unpartitioned.
  const int kKeySize = 32;
  const int kBlockSize = 128;
  const int kNumBlocksPerFile = 128;
  const int kNumKeysPerFile = kBlockSize * kNumBlocksPerFile / kKeySize;

  Options options = CurrentOptions();
  // `kNoCompression` makes the unit test more portable. But it relies on the
  // current behavior of persisting/accessing dictionary even when there's no
  // (de)compression happening, which seems fairly likely to change over time.
  options.compression = kNoCompression;
  options.compression_opts.max_dict_bytes = 4 << 10;
  options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();
  BlockBasedTableOptions table_options;
  table_options.block_cache = NewLRUCache(1 << 20 /* capacity */);
  table_options.block_size = kBlockSize;
  table_options.metadata_block_size = kBlockSize;
  table_options.cache_index_and_filter_blocks = true;
  table_options.metadata_cache_options.top_level_index_pinning =
      top_level_index_pinning_;
  table_options.metadata_cache_options.partition_pinning = partition_pinning_;
  table_options.metadata_cache_options.unpartitioned_pinning =
      unpartitioned_pinning_;
  table_options.filter_policy.reset(
      NewBloomFilterPolicy(10 /* bits_per_key */));
  if (partition_index_and_filters_) {
    table_options.index_type =
        BlockBasedTableOptions::IndexType::kTwoLevelIndexSearch;
    table_options.partition_filters = true;
  }
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  Reopen(options);

  Random rnd(301);
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < kNumKeysPerFile; ++j) {
      ASSERT_OK(Put(Key(i * kNumKeysPerFile + j), rnd.RandomString(kKeySize)));
    }
    ASSERT_OK(Flush());
    if (i == 0) {
      // Prevent trivial move so file will be rewritten with dictionary and
      // reopened with L1's pinning settings.
      CompactRangeOptions cro;
      cro.bottommost_level_compaction = BottommostLevelCompaction::kForce;
      ASSERT_OK(db_->CompactRange(cro, nullptr, nullptr));
    }
  }

  // Clear all unpinned blocks so unpinned blocks will show up as cache misses
  // when reading a key from a file.
  table_options.block_cache->EraseUnRefEntries();

  // Get base cache values
  uint64_t filter_misses = TestGetTickerCount(options, BLOCK_CACHE_FILTER_MISS);
  uint64_t index_misses = TestGetTickerCount(options, BLOCK_CACHE_INDEX_MISS);
  uint64_t compression_dict_misses =
      TestGetTickerCount(options, BLOCK_CACHE_COMPRESSION_DICT_MISS);

  // Read a key from the L0 file
  Get(Key(kNumKeysPerFile));
  uint64_t expected_filter_misses = filter_misses;
  uint64_t expected_index_misses = index_misses;
  uint64_t expected_compression_dict_misses = compression_dict_misses;
  if (partition_index_and_filters_) {
    if (top_level_index_pinning_ == PinningTier::kNone) {
      ++expected_filter_misses;
      ++expected_index_misses;
    }
    if (partition_pinning_ == PinningTier::kNone) {
      ++expected_filter_misses;
      ++expected_index_misses;
    }
  } else {
    if (unpartitioned_pinning_ == PinningTier::kNone) {
      ++expected_filter_misses;
      ++expected_index_misses;
    }
  }
  if (unpartitioned_pinning_ == PinningTier::kNone) {
    ++expected_compression_dict_misses;
  }
  ASSERT_EQ(expected_filter_misses,
            TestGetTickerCount(options, BLOCK_CACHE_FILTER_MISS));
  ASSERT_EQ(expected_index_misses,
            TestGetTickerCount(options, BLOCK_CACHE_INDEX_MISS));
  ASSERT_EQ(expected_compression_dict_misses,
            TestGetTickerCount(options, BLOCK_CACHE_COMPRESSION_DICT_MISS));

  // Clear all unpinned blocks so unpinned blocks will show up as cache misses
  // when reading a key from a file.
  table_options.block_cache->EraseUnRefEntries();

  // Read a key from the L1 file
  Get(Key(0));
  if (partition_index_and_filters_) {
    if (top_level_index_pinning_ == PinningTier::kNone ||
        top_level_index_pinning_ == PinningTier::kFlushedAndSimilar) {
      ++expected_filter_misses;
      ++expected_index_misses;
    }
    if (partition_pinning_ == PinningTier::kNone ||
        partition_pinning_ == PinningTier::kFlushedAndSimilar) {
      ++expected_filter_misses;
      ++expected_index_misses;
    }
  } else {
    if (unpartitioned_pinning_ == PinningTier::kNone ||
        unpartitioned_pinning_ == PinningTier::kFlushedAndSimilar) {
      ++expected_filter_misses;
      ++expected_index_misses;
    }
  }
  if (unpartitioned_pinning_ == PinningTier::kNone ||
      unpartitioned_pinning_ == PinningTier::kFlushedAndSimilar) {
    ++expected_compression_dict_misses;
  }
  ASSERT_EQ(expected_filter_misses,
            TestGetTickerCount(options, BLOCK_CACHE_FILTER_MISS));
  ASSERT_EQ(expected_index_misses,
            TestGetTickerCount(options, BLOCK_CACHE_INDEX_MISS));
  ASSERT_EQ(expected_compression_dict_misses,
            TestGetTickerCount(options, BLOCK_CACHE_COMPRESSION_DICT_MISS));
}

INSTANTIATE_TEST_CASE_P(
    DBBlockCachePinningTest, DBBlockCachePinningTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(PinningTier::kNone, PinningTier::kFlushedAndSimilar,
                          PinningTier::kAll),
        ::testing::Values(PinningTier::kNone, PinningTier::kFlushedAndSimilar,
                          PinningTier::kAll),
        ::testing::Values(PinningTier::kNone, PinningTier::kFlushedAndSimilar,
                          PinningTier::kAll)));

}  // namespace ROCKSDB_NAMESPACE

int main(int argc, char** argv) {
  ROCKSDB_NAMESPACE::port::InstallStackTraceHandler();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
