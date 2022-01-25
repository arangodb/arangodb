//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
#include <limits>
#include <string>
#include <unordered_map>

#include "db/column_family.h"
#include "db/db_impl/db_impl.h"
#include "db/db_test_util.h"
#include "options/options_helper.h"
#include "port/stack_trace.h"
#include "rocksdb/cache.h"
#include "rocksdb/convenience.h"
#include "rocksdb/rate_limiter.h"
#include "rocksdb/stats_history.h"
#include "test_util/sync_point.h"
#include "test_util/testutil.h"
#include "util/random.h"

namespace ROCKSDB_NAMESPACE {

class DBOptionsTest : public DBTestBase {
 public:
  DBOptionsTest() : DBTestBase("db_options_test", /*env_do_fsync=*/true) {}

#ifndef ROCKSDB_LITE
  std::unordered_map<std::string, std::string> GetMutableDBOptionsMap(
      const DBOptions& options) {
    std::string options_str;
    std::unordered_map<std::string, std::string> mutable_map;
    ConfigOptions config_options(options);
    config_options.delimiter = "; ";

    EXPECT_OK(GetStringFromMutableDBOptions(
        config_options, MutableDBOptions(options), &options_str));
    EXPECT_OK(StringToMap(options_str, &mutable_map));

    return mutable_map;
  }

  std::unordered_map<std::string, std::string> GetMutableCFOptionsMap(
      const ColumnFamilyOptions& options) {
    std::string options_str;
    ConfigOptions config_options;
    config_options.delimiter = "; ";

    std::unordered_map<std::string, std::string> mutable_map;
    EXPECT_OK(GetStringFromMutableCFOptions(
        config_options, MutableCFOptions(options), &options_str));
    EXPECT_OK(StringToMap(options_str, &mutable_map));
    return mutable_map;
  }

  std::unordered_map<std::string, std::string> GetRandomizedMutableCFOptionsMap(
      Random* rnd) {
    Options options = CurrentOptions();
    options.env = env_;
    ImmutableDBOptions db_options(options);
    test::RandomInitCFOptions(&options, options, rnd);
    auto sanitized_options = SanitizeOptions(db_options, options);
    auto opt_map = GetMutableCFOptionsMap(sanitized_options);
    delete options.compaction_filter;
    return opt_map;
  }

  std::unordered_map<std::string, std::string> GetRandomizedMutableDBOptionsMap(
      Random* rnd) {
    DBOptions db_options;
    test::RandomInitDBOptions(&db_options, rnd);
    auto sanitized_options = SanitizeOptions(dbname_, db_options);
    return GetMutableDBOptionsMap(sanitized_options);
  }
#endif  // ROCKSDB_LITE
};

TEST_F(DBOptionsTest, ImmutableTrackAndVerifyWalsInManifest) {
  Options options;
  options.env = env_;
  options.track_and_verify_wals_in_manifest = true;

  ImmutableDBOptions db_options(options);
  ASSERT_TRUE(db_options.track_and_verify_wals_in_manifest);

  Reopen(options);
  ASSERT_TRUE(dbfull()->GetDBOptions().track_and_verify_wals_in_manifest);

  Status s =
      dbfull()->SetDBOptions({{"track_and_verify_wals_in_manifest", "false"}});
  ASSERT_FALSE(s.ok());
}

// RocksDB lite don't support dynamic options.
#ifndef ROCKSDB_LITE

TEST_F(DBOptionsTest, AvoidUpdatingOptions) {
  Options options;
  options.env = env_;
  options.max_background_jobs = 4;
  options.delayed_write_rate = 1024;

  Reopen(options);

  SyncPoint::GetInstance()->DisableProcessing();
  SyncPoint::GetInstance()->ClearAllCallBacks();
  bool is_changed_stats = false;
  SyncPoint::GetInstance()->SetCallBack(
      "DBImpl::WriteOptionsFile:PersistOptions", [&](void* /*arg*/) {
        ASSERT_FALSE(is_changed_stats);  // should only save options file once
        is_changed_stats = true;
      });
  SyncPoint::GetInstance()->EnableProcessing();

  // helper function to check the status and reset after each check
  auto is_changed = [&] {
    bool ret = is_changed_stats;
    is_changed_stats = false;
    return ret;
  };

  // without changing the value, but it's sanitized to a different value
  ASSERT_OK(dbfull()->SetDBOptions({{"bytes_per_sync", "0"}}));
  ASSERT_TRUE(is_changed());

  // without changing the value
  ASSERT_OK(dbfull()->SetDBOptions({{"max_background_jobs", "4"}}));
  ASSERT_FALSE(is_changed());

  // changing the value
  ASSERT_OK(dbfull()->SetDBOptions({{"bytes_per_sync", "123"}}));
  ASSERT_TRUE(is_changed());

  // update again
  ASSERT_OK(dbfull()->SetDBOptions({{"bytes_per_sync", "123"}}));
  ASSERT_FALSE(is_changed());

  // without changing a default value
  ASSERT_OK(dbfull()->SetDBOptions({{"strict_bytes_per_sync", "false"}}));
  ASSERT_FALSE(is_changed());

  // now change
  ASSERT_OK(dbfull()->SetDBOptions({{"strict_bytes_per_sync", "true"}}));
  ASSERT_TRUE(is_changed());

  // multiple values without change
  ASSERT_OK(dbfull()->SetDBOptions(
      {{"max_total_wal_size", "0"}, {"stats_dump_period_sec", "600"}}));
  ASSERT_FALSE(is_changed());

  // multiple values with change
  ASSERT_OK(dbfull()->SetDBOptions(
      {{"max_open_files", "100"}, {"stats_dump_period_sec", "600"}}));
  ASSERT_TRUE(is_changed());
}

TEST_F(DBOptionsTest, GetLatestDBOptions) {
  // GetOptions should be able to get latest option changed by SetOptions.
  Options options;
  options.create_if_missing = true;
  options.env = env_;
  Random rnd(228);
  Reopen(options);
  auto new_options = GetRandomizedMutableDBOptionsMap(&rnd);
  ASSERT_OK(dbfull()->SetDBOptions(new_options));
  ASSERT_EQ(new_options, GetMutableDBOptionsMap(dbfull()->GetDBOptions()));
}

TEST_F(DBOptionsTest, GetLatestCFOptions) {
  // GetOptions should be able to get latest option changed by SetOptions.
  Options options;
  options.create_if_missing = true;
  options.env = env_;
  Random rnd(228);
  Reopen(options);
  CreateColumnFamilies({"foo"}, options);
  ReopenWithColumnFamilies({"default", "foo"}, options);
  auto options_default = GetRandomizedMutableCFOptionsMap(&rnd);
  auto options_foo = GetRandomizedMutableCFOptionsMap(&rnd);
  ASSERT_OK(dbfull()->SetOptions(handles_[0], options_default));
  ASSERT_OK(dbfull()->SetOptions(handles_[1], options_foo));
  ASSERT_EQ(options_default,
            GetMutableCFOptionsMap(dbfull()->GetOptions(handles_[0])));
  ASSERT_EQ(options_foo,
            GetMutableCFOptionsMap(dbfull()->GetOptions(handles_[1])));
}

TEST_F(DBOptionsTest, SetMutableTableOptions) {
  Options options;
  options.create_if_missing = true;
  options.env = env_;
  options.blob_file_size = 16384;
  BlockBasedTableOptions bbto;
  bbto.no_block_cache = true;
  bbto.block_size = 8192;
  bbto.block_restart_interval = 7;

  options.table_factory.reset(NewBlockBasedTableFactory(bbto));
  Reopen(options);

  ColumnFamilyHandle* cfh = dbfull()->DefaultColumnFamily();
  Options c_opts = dbfull()->GetOptions(cfh);
  const auto* c_bbto =
      c_opts.table_factory->GetOptions<BlockBasedTableOptions>();
  ASSERT_NE(c_bbto, nullptr);
  ASSERT_EQ(c_opts.blob_file_size, 16384);
  ASSERT_EQ(c_bbto->no_block_cache, true);
  ASSERT_EQ(c_bbto->block_size, 8192);
  ASSERT_EQ(c_bbto->block_restart_interval, 7);
  ASSERT_OK(dbfull()->SetOptions(
      cfh, {{"table_factory.block_size", "16384"},
            {"table_factory.block_restart_interval", "11"}}));
  ASSERT_EQ(c_bbto->block_size, 16384);
  ASSERT_EQ(c_bbto->block_restart_interval, 11);

  // Now set an option that is not mutable - options should not change
  ASSERT_NOK(
      dbfull()->SetOptions(cfh, {{"table_factory.no_block_cache", "false"}}));
  ASSERT_EQ(c_bbto->no_block_cache, true);
  ASSERT_EQ(c_bbto->block_size, 16384);
  ASSERT_EQ(c_bbto->block_restart_interval, 11);

  // Set some that are mutable and some that are not - options should not change
  ASSERT_NOK(dbfull()->SetOptions(
      cfh, {{"table_factory.no_block_cache", "false"},
            {"table_factory.block_size", "8192"},
            {"table_factory.block_restart_interval", "7"}}));
  ASSERT_EQ(c_bbto->no_block_cache, true);
  ASSERT_EQ(c_bbto->block_size, 16384);
  ASSERT_EQ(c_bbto->block_restart_interval, 11);

  // Set some that are mutable and some that do not exist - options should not
  // change
  ASSERT_NOK(dbfull()->SetOptions(
      cfh, {{"table_factory.block_size", "8192"},
            {"table_factory.does_not_exist", "true"},
            {"table_factory.block_restart_interval", "7"}}));
  ASSERT_EQ(c_bbto->no_block_cache, true);
  ASSERT_EQ(c_bbto->block_size, 16384);
  ASSERT_EQ(c_bbto->block_restart_interval, 11);

  // Trying to change the table factory fails
  ASSERT_NOK(dbfull()->SetOptions(
      cfh, {{"table_factory", TableFactory::kPlainTableName()}}));

  // Set some on the table and some on the Column Family
  ASSERT_OK(dbfull()->SetOptions(
      cfh, {{"table_factory.block_size", "16384"},
            {"blob_file_size", "32768"},
            {"table_factory.block_restart_interval", "13"}}));
  c_opts = dbfull()->GetOptions(cfh);
  ASSERT_EQ(c_opts.blob_file_size, 32768);
  ASSERT_EQ(c_bbto->block_size, 16384);
  ASSERT_EQ(c_bbto->block_restart_interval, 13);
  // Set some on the table and a bad one on the ColumnFamily - options should
  // not change
  ASSERT_NOK(dbfull()->SetOptions(
      cfh, {{"table_factory.block_size", "1024"},
            {"no_such_option", "32768"},
            {"table_factory.block_restart_interval", "7"}}));
  ASSERT_EQ(c_bbto->block_size, 16384);
  ASSERT_EQ(c_bbto->block_restart_interval, 13);
}

TEST_F(DBOptionsTest, SetWithCustomMemTableFactory) {
  class DummySkipListFactory : public SkipListFactory {
   public:
    static const char* kClassName() { return "DummySkipListFactory"; }
    const char* Name() const override { return kClassName(); }
    explicit DummySkipListFactory() : SkipListFactory(2) {}
  };
  {
    // Verify the DummySkipList cannot be created
    ConfigOptions config_options;
    config_options.ignore_unsupported_options = false;
    std::unique_ptr<MemTableRepFactory> factory;
    ASSERT_NOK(MemTableRepFactory::CreateFromString(
        config_options, DummySkipListFactory::kClassName(), &factory));
  }
  Options options;
  options.create_if_missing = true;
  // Try with fail_if_options_file_error=false/true to update the options
  for (bool on_error : {false, true}) {
    options.fail_if_options_file_error = on_error;
    options.env = env_;
    options.disable_auto_compactions = false;

    options.memtable_factory.reset(new DummySkipListFactory());
    Reopen(options);

    ColumnFamilyHandle* cfh = dbfull()->DefaultColumnFamily();
    ASSERT_OK(
        dbfull()->SetOptions(cfh, {{"disable_auto_compactions", "true"}}));
    ColumnFamilyDescriptor cfd;
    ASSERT_OK(cfh->GetDescriptor(&cfd));
    ASSERT_STREQ(cfd.options.memtable_factory->Name(),
                 DummySkipListFactory::kClassName());
    ColumnFamilyHandle* test = nullptr;
    ASSERT_OK(dbfull()->CreateColumnFamily(options, "test", &test));
    ASSERT_OK(test->GetDescriptor(&cfd));
    ASSERT_STREQ(cfd.options.memtable_factory->Name(),
                 DummySkipListFactory::kClassName());

    ASSERT_OK(dbfull()->DropColumnFamily(test));
    delete test;
  }
}

TEST_F(DBOptionsTest, SetBytesPerSync) {
  const size_t kValueSize = 1024 * 1024;  // 1MB
  Options options;
  options.create_if_missing = true;
  options.bytes_per_sync = 1024 * 1024;
  options.use_direct_reads = false;
  options.write_buffer_size = 400 * kValueSize;
  options.disable_auto_compactions = true;
  options.compression = kNoCompression;
  options.env = env_;
  Reopen(options);
  int counter = 0;
  int low_bytes_per_sync = 0;
  int i = 0;
  const std::string kValue(kValueSize, 'v');
  ASSERT_EQ(options.bytes_per_sync, dbfull()->GetDBOptions().bytes_per_sync);
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->SetCallBack(
      "WritableFileWriter::RangeSync:0", [&](void* /*arg*/) { counter++; });

  WriteOptions write_opts;
  // should sync approximately 40MB/1MB ~= 40 times.
  for (i = 0; i < 40; i++) {
    ASSERT_OK(Put(Key(i), kValue, write_opts));
  }
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->EnableProcessing();
  ASSERT_OK(dbfull()->CompactRange(CompactRangeOptions(), nullptr, nullptr));
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->DisableProcessing();
  low_bytes_per_sync = counter;
  ASSERT_GT(low_bytes_per_sync, 35);
  ASSERT_LT(low_bytes_per_sync, 45);

  counter = 0;
  // 8388608 = 8 * 1024 * 1024
  ASSERT_OK(dbfull()->SetDBOptions({{"bytes_per_sync", "8388608"}}));
  ASSERT_EQ(8388608, dbfull()->GetDBOptions().bytes_per_sync);
  // should sync approximately 40MB*2/8MB ~= 10 times.
  // data will be 40*2MB because of previous Puts too.
  for (i = 0; i < 40; i++) {
    ASSERT_OK(Put(Key(i), kValue, write_opts));
  }
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->EnableProcessing();
  ASSERT_OK(dbfull()->CompactRange(CompactRangeOptions(), nullptr, nullptr));
  ASSERT_GT(counter, 5);
  ASSERT_LT(counter, 15);

  // Redundant assert. But leaving it here just to get the point across that
  // low_bytes_per_sync > counter.
  ASSERT_GT(low_bytes_per_sync, counter);
}

TEST_F(DBOptionsTest, SetWalBytesPerSync) {
  const size_t kValueSize = 1024 * 1024 * 3;
  Options options;
  options.create_if_missing = true;
  options.wal_bytes_per_sync = 512;
  options.write_buffer_size = 100 * kValueSize;
  options.disable_auto_compactions = true;
  options.compression = kNoCompression;
  options.env = env_;
  Reopen(options);
  ASSERT_EQ(512, dbfull()->GetDBOptions().wal_bytes_per_sync);
  std::atomic_int counter{0};
  int low_bytes_per_sync = 0;
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->SetCallBack(
      "WritableFileWriter::RangeSync:0",
      [&](void* /*arg*/) { counter.fetch_add(1); });
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->EnableProcessing();
  const std::string kValue(kValueSize, 'v');
  int i = 0;
  for (; i < 10; i++) {
    ASSERT_OK(Put(Key(i), kValue));
  }
  // Do not flush. If we flush here, SwitchWAL will reuse old WAL file since its
  // empty and will not get the new wal_bytes_per_sync value.
  low_bytes_per_sync = counter;
  //5242880 = 1024 * 1024 * 5
  ASSERT_OK(dbfull()->SetDBOptions({{"wal_bytes_per_sync", "5242880"}}));
  ASSERT_EQ(5242880, dbfull()->GetDBOptions().wal_bytes_per_sync);
  counter = 0;
  i = 0;
  for (; i < 10; i++) {
    ASSERT_OK(Put(Key(i), kValue));
  }
  ASSERT_GT(counter, 0);
  ASSERT_GT(low_bytes_per_sync, 0);
  ASSERT_GT(low_bytes_per_sync, counter);
}

TEST_F(DBOptionsTest, WritableFileMaxBufferSize) {
  Options options;
  options.create_if_missing = true;
  options.writable_file_max_buffer_size = 1024 * 1024;
  options.level0_file_num_compaction_trigger = 3;
  options.max_manifest_file_size = 1;
  options.env = env_;
  int buffer_size = 1024 * 1024;
  Reopen(options);
  ASSERT_EQ(buffer_size,
            dbfull()->GetDBOptions().writable_file_max_buffer_size);

  std::atomic<int> match_cnt(0);
  std::atomic<int> unmatch_cnt(0);
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->SetCallBack(
      "WritableFileWriter::WritableFileWriter:0", [&](void* arg) {
        int value = static_cast<int>(reinterpret_cast<uintptr_t>(arg));
        if (value == buffer_size) {
          match_cnt++;
        } else {
          unmatch_cnt++;
        }
      });
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->EnableProcessing();
  int i = 0;
  for (; i < 3; i++) {
    ASSERT_OK(Put("foo", ToString(i)));
    ASSERT_OK(Put("bar", ToString(i)));
    ASSERT_OK(Flush());
  }
  ASSERT_OK(dbfull()->TEST_WaitForCompact());
  ASSERT_EQ(unmatch_cnt, 0);
  ASSERT_GE(match_cnt, 11);

  ASSERT_OK(
      dbfull()->SetDBOptions({{"writable_file_max_buffer_size", "524288"}}));
  buffer_size = 512 * 1024;
  match_cnt = 0;
  unmatch_cnt = 0;  // SetDBOptions() will create a WriteableFileWriter

  ASSERT_EQ(buffer_size,
            dbfull()->GetDBOptions().writable_file_max_buffer_size);
  i = 0;
  for (; i < 3; i++) {
    ASSERT_OK(Put("foo", ToString(i)));
    ASSERT_OK(Put("bar", ToString(i)));
    ASSERT_OK(Flush());
  }
  ASSERT_OK(dbfull()->TEST_WaitForCompact());
  ASSERT_EQ(unmatch_cnt, 0);
  ASSERT_GE(match_cnt, 11);
}

TEST_F(DBOptionsTest, SetOptionsAndReopen) {
  Random rnd(1044);
  auto rand_opts = GetRandomizedMutableCFOptionsMap(&rnd);
  ASSERT_OK(dbfull()->SetOptions(rand_opts));
  // Verify if DB can be reopen after setting options.
  Options options;
  options.env = env_;
  ASSERT_OK(TryReopen(options));
}

TEST_F(DBOptionsTest, EnableAutoCompactionAndTriggerStall) {
  const std::string kValue(1024, 'v');
  for (int method_type = 0; method_type < 2; method_type++) {
    for (int option_type = 0; option_type < 4; option_type++) {
      Options options;
      options.create_if_missing = true;
      options.disable_auto_compactions = true;
      options.write_buffer_size = 1024 * 1024 * 10;
      options.compression = CompressionType::kNoCompression;
      options.level0_file_num_compaction_trigger = 1;
      options.level0_stop_writes_trigger = std::numeric_limits<int>::max();
      options.level0_slowdown_writes_trigger = std::numeric_limits<int>::max();
      options.hard_pending_compaction_bytes_limit =
          std::numeric_limits<uint64_t>::max();
      options.soft_pending_compaction_bytes_limit =
          std::numeric_limits<uint64_t>::max();
      options.env = env_;

      DestroyAndReopen(options);
      int i = 0;
      for (; i < 1024; i++) {
        ASSERT_OK(Put(Key(i), kValue));
      }
      ASSERT_OK(Flush());
      for (; i < 1024 * 2; i++) {
        ASSERT_OK(Put(Key(i), kValue));
      }
      ASSERT_OK(Flush());
      ASSERT_OK(dbfull()->TEST_WaitForFlushMemTable());
      ASSERT_EQ(2, NumTableFilesAtLevel(0));
      uint64_t l0_size = SizeAtLevel(0);

      switch (option_type) {
        case 0:
          // test with level0_stop_writes_trigger
          options.level0_stop_writes_trigger = 2;
          options.level0_slowdown_writes_trigger = 2;
          break;
        case 1:
          options.level0_slowdown_writes_trigger = 2;
          break;
        case 2:
          options.hard_pending_compaction_bytes_limit = l0_size;
          options.soft_pending_compaction_bytes_limit = l0_size;
          break;
        case 3:
          options.soft_pending_compaction_bytes_limit = l0_size;
          break;
      }
      Reopen(options);
      ASSERT_OK(dbfull()->TEST_WaitForCompact());
      ASSERT_FALSE(dbfull()->TEST_write_controler().IsStopped());
      ASSERT_FALSE(dbfull()->TEST_write_controler().NeedsDelay());

      SyncPoint::GetInstance()->LoadDependency(
          {{"DBOptionsTest::EnableAutoCompactionAndTriggerStall:1",
            "BackgroundCallCompaction:0"},
           {"DBImpl::BackgroundCompaction():BeforePickCompaction",
            "DBOptionsTest::EnableAutoCompactionAndTriggerStall:2"},
           {"DBOptionsTest::EnableAutoCompactionAndTriggerStall:3",
            "DBImpl::BackgroundCompaction():AfterPickCompaction"}});
      // Block background compaction.
      SyncPoint::GetInstance()->EnableProcessing();

      switch (method_type) {
        case 0:
          ASSERT_OK(
              dbfull()->SetOptions({{"disable_auto_compactions", "false"}}));
          break;
        case 1:
          ASSERT_OK(dbfull()->EnableAutoCompaction(
              {dbfull()->DefaultColumnFamily()}));
          break;
      }
      TEST_SYNC_POINT("DBOptionsTest::EnableAutoCompactionAndTriggerStall:1");
      // Wait for stall condition recalculate.
      TEST_SYNC_POINT("DBOptionsTest::EnableAutoCompactionAndTriggerStall:2");

      switch (option_type) {
        case 0:
          ASSERT_TRUE(dbfull()->TEST_write_controler().IsStopped());
          break;
        case 1:
          ASSERT_FALSE(dbfull()->TEST_write_controler().IsStopped());
          ASSERT_TRUE(dbfull()->TEST_write_controler().NeedsDelay());
          break;
        case 2:
          ASSERT_TRUE(dbfull()->TEST_write_controler().IsStopped());
          break;
        case 3:
          ASSERT_FALSE(dbfull()->TEST_write_controler().IsStopped());
          ASSERT_TRUE(dbfull()->TEST_write_controler().NeedsDelay());
          break;
      }
      TEST_SYNC_POINT("DBOptionsTest::EnableAutoCompactionAndTriggerStall:3");

      // Background compaction executed.
      ASSERT_OK(dbfull()->TEST_WaitForCompact());
      ASSERT_FALSE(dbfull()->TEST_write_controler().IsStopped());
      ASSERT_FALSE(dbfull()->TEST_write_controler().NeedsDelay());
    }
  }
}

TEST_F(DBOptionsTest, SetOptionsMayTriggerCompaction) {
  Options options;
  options.create_if_missing = true;
  options.level0_file_num_compaction_trigger = 1000;
  options.env = env_;
  Reopen(options);
  for (int i = 0; i < 3; i++) {
    // Need to insert two keys to avoid trivial move.
    ASSERT_OK(Put("foo", ToString(i)));
    ASSERT_OK(Put("bar", ToString(i)));
    ASSERT_OK(Flush());
  }
  ASSERT_EQ("3", FilesPerLevel());
  ASSERT_OK(
      dbfull()->SetOptions({{"level0_file_num_compaction_trigger", "3"}}));
  ASSERT_OK(dbfull()->TEST_WaitForCompact());
  ASSERT_EQ("0,1", FilesPerLevel());
}

TEST_F(DBOptionsTest, SetBackgroundCompactionThreads) {
  Options options;
  options.create_if_missing = true;
  options.max_background_compactions = 1;   // default value
  options.env = env_;
  Reopen(options);
  ASSERT_EQ(1, dbfull()->TEST_BGCompactionsAllowed());
  ASSERT_OK(dbfull()->SetDBOptions({{"max_background_compactions", "3"}}));
  ASSERT_EQ(1, dbfull()->TEST_BGCompactionsAllowed());
  auto stop_token = dbfull()->TEST_write_controler().GetStopToken();
  ASSERT_EQ(3, dbfull()->TEST_BGCompactionsAllowed());
}

TEST_F(DBOptionsTest, SetBackgroundFlushThreads) {
  Options options;
  options.create_if_missing = true;
  options.max_background_flushes = 1;
  options.env = env_;
  Reopen(options);
  ASSERT_EQ(1, dbfull()->TEST_BGFlushesAllowed());
  ASSERT_EQ(1, env_->GetBackgroundThreads(Env::Priority::HIGH));
  ASSERT_OK(dbfull()->SetDBOptions({{"max_background_flushes", "3"}}));
  ASSERT_EQ(3, env_->GetBackgroundThreads(Env::Priority::HIGH));
  ASSERT_EQ(3, dbfull()->TEST_BGFlushesAllowed());
}


TEST_F(DBOptionsTest, SetBackgroundJobs) {
  Options options;
  options.create_if_missing = true;
  options.max_background_jobs = 8;
  options.env = env_;
  Reopen(options);

  for (int i = 0; i < 2; ++i) {
    if (i > 0) {
      options.max_background_jobs = 12;
      ASSERT_OK(dbfull()->SetDBOptions(
          {{"max_background_jobs",
            std::to_string(options.max_background_jobs)}}));
    }

    const int expected_max_flushes = options.max_background_jobs / 4;

    ASSERT_EQ(expected_max_flushes, dbfull()->TEST_BGFlushesAllowed());
    ASSERT_EQ(1, dbfull()->TEST_BGCompactionsAllowed());

    auto stop_token = dbfull()->TEST_write_controler().GetStopToken();

    const int expected_max_compactions = 3 * expected_max_flushes;

    ASSERT_EQ(expected_max_flushes, dbfull()->TEST_BGFlushesAllowed());
    ASSERT_EQ(expected_max_compactions, dbfull()->TEST_BGCompactionsAllowed());

    ASSERT_EQ(expected_max_flushes,
              env_->GetBackgroundThreads(Env::Priority::HIGH));
    ASSERT_EQ(expected_max_compactions,
              env_->GetBackgroundThreads(Env::Priority::LOW));
  }
}

TEST_F(DBOptionsTest, AvoidFlushDuringShutdown) {
  Options options;
  options.create_if_missing = true;
  options.disable_auto_compactions = true;
  options.env = env_;
  WriteOptions write_without_wal;
  write_without_wal.disableWAL = true;

  ASSERT_FALSE(options.avoid_flush_during_shutdown);
  DestroyAndReopen(options);
  ASSERT_OK(Put("foo", "v1", write_without_wal));
  Reopen(options);
  ASSERT_EQ("v1", Get("foo"));
  ASSERT_EQ("1", FilesPerLevel());

  DestroyAndReopen(options);
  ASSERT_OK(Put("foo", "v2", write_without_wal));
  ASSERT_OK(dbfull()->SetDBOptions({{"avoid_flush_during_shutdown", "true"}}));
  Reopen(options);
  ASSERT_EQ("NOT_FOUND", Get("foo"));
  ASSERT_EQ("", FilesPerLevel());
}

TEST_F(DBOptionsTest, SetDelayedWriteRateOption) {
  Options options;
  options.create_if_missing = true;
  options.delayed_write_rate = 2 * 1024U * 1024U;
  options.env = env_;
  Reopen(options);
  ASSERT_EQ(2 * 1024U * 1024U, dbfull()->TEST_write_controler().max_delayed_write_rate());

  ASSERT_OK(dbfull()->SetDBOptions({{"delayed_write_rate", "20000"}}));
  ASSERT_EQ(20000, dbfull()->TEST_write_controler().max_delayed_write_rate());
}

TEST_F(DBOptionsTest, MaxTotalWalSizeChange) {
  Random rnd(1044);
  const auto value_size = size_t(1024);
  std::string value = rnd.RandomString(value_size);

  Options options;
  options.create_if_missing = true;
  options.env = env_;
  CreateColumnFamilies({"1", "2", "3"}, options);
  ReopenWithColumnFamilies({"default", "1", "2", "3"}, options);

  WriteOptions write_options;

  const int key_count = 100;
  for (int i = 0; i < key_count; ++i) {
    for (size_t cf = 0; cf < handles_.size(); ++cf) {
      ASSERT_OK(Put(static_cast<int>(cf), Key(i), value));
    }
  }
  ASSERT_OK(dbfull()->SetDBOptions({{"max_total_wal_size", "10"}}));

  for (size_t cf = 0; cf < handles_.size(); ++cf) {
    ASSERT_OK(dbfull()->TEST_WaitForFlushMemTable(handles_[cf]));
    ASSERT_EQ("1", FilesPerLevel(static_cast<int>(cf)));
  }
}

TEST_F(DBOptionsTest, SetStatsDumpPeriodSec) {
  Options options;
  options.create_if_missing = true;
  options.stats_dump_period_sec = 5;
  options.env = env_;
  Reopen(options);
  ASSERT_EQ(5u, dbfull()->GetDBOptions().stats_dump_period_sec);

  for (int i = 0; i < 20; i++) {
    unsigned int num = rand() % 5000 + 1;
    ASSERT_OK(
        dbfull()->SetDBOptions({{"stats_dump_period_sec", ToString(num)}}));
    ASSERT_EQ(num, dbfull()->GetDBOptions().stats_dump_period_sec);
  }
  Close();
}

TEST_F(DBOptionsTest, SetOptionsStatsPersistPeriodSec) {
  Options options;
  options.create_if_missing = true;
  options.stats_persist_period_sec = 5;
  options.env = env_;
  Reopen(options);
  ASSERT_EQ(5u, dbfull()->GetDBOptions().stats_persist_period_sec);

  ASSERT_OK(dbfull()->SetDBOptions({{"stats_persist_period_sec", "12345"}}));
  ASSERT_EQ(12345u, dbfull()->GetDBOptions().stats_persist_period_sec);
  ASSERT_NOK(dbfull()->SetDBOptions({{"stats_persist_period_sec", "abcde"}}));
  ASSERT_EQ(12345u, dbfull()->GetDBOptions().stats_persist_period_sec);
}

static void assert_candidate_files_empty(DBImpl* dbfull, const bool empty) {
  dbfull->TEST_LockMutex();
  JobContext job_context(0);
  dbfull->FindObsoleteFiles(&job_context, false);
  ASSERT_EQ(empty, job_context.full_scan_candidate_files.empty());
  dbfull->TEST_UnlockMutex();
  if (job_context.HaveSomethingToDelete()) {
    // fulfill the contract of FindObsoleteFiles by calling PurgeObsoleteFiles
    // afterwards; otherwise the test may hang on shutdown
    dbfull->PurgeObsoleteFiles(job_context);
  }
  job_context.Clean();
}

TEST_F(DBOptionsTest, DeleteObsoleteFilesPeriodChange) {
  Options options;
  options.env = env_;
  SetTimeElapseOnlySleepOnReopen(&options);
  options.create_if_missing = true;
  ASSERT_OK(TryReopen(options));

  // Verify that candidate files set is empty when no full scan requested.
  assert_candidate_files_empty(dbfull(), true);

  ASSERT_OK(
      dbfull()->SetDBOptions({{"delete_obsolete_files_period_micros", "0"}}));

  // After delete_obsolete_files_period_micros updated to 0, the next call
  // to FindObsoleteFiles should make a full scan
  assert_candidate_files_empty(dbfull(), false);

  ASSERT_OK(
      dbfull()->SetDBOptions({{"delete_obsolete_files_period_micros", "20"}}));

  assert_candidate_files_empty(dbfull(), true);

  env_->MockSleepForMicroseconds(20);
  assert_candidate_files_empty(dbfull(), true);

  env_->MockSleepForMicroseconds(1);
  assert_candidate_files_empty(dbfull(), false);

  Close();
}

TEST_F(DBOptionsTest, MaxOpenFilesChange) {
  SpecialEnv env(env_);
  Options options;
  options.env = CurrentOptions().env;
  options.max_open_files = -1;

  Reopen(options);

  Cache* tc = dbfull()->TEST_table_cache();

  ASSERT_EQ(-1, dbfull()->GetDBOptions().max_open_files);
  ASSERT_LT(2000, tc->GetCapacity());
  ASSERT_OK(dbfull()->SetDBOptions({{"max_open_files", "1024"}}));
  ASSERT_EQ(1024, dbfull()->GetDBOptions().max_open_files);
  // examine the table cache (actual size should be 1014)
  ASSERT_GT(1500, tc->GetCapacity());
  Close();
}

TEST_F(DBOptionsTest, SanitizeDelayedWriteRate) {
  Options options;
  options.env = CurrentOptions().env;
  options.delayed_write_rate = 0;
  Reopen(options);
  ASSERT_EQ(16 * 1024 * 1024, dbfull()->GetDBOptions().delayed_write_rate);

  options.rate_limiter.reset(NewGenericRateLimiter(31 * 1024 * 1024));
  Reopen(options);
  ASSERT_EQ(31 * 1024 * 1024, dbfull()->GetDBOptions().delayed_write_rate);
}

TEST_F(DBOptionsTest, SanitizeUniversalTTLCompaction) {
  Options options;
  options.env = CurrentOptions().env;
  options.compaction_style = kCompactionStyleUniversal;

  options.ttl = 0;
  options.periodic_compaction_seconds = 0;
  Reopen(options);
  ASSERT_EQ(0, dbfull()->GetOptions().ttl);
  ASSERT_EQ(0, dbfull()->GetOptions().periodic_compaction_seconds);

  options.ttl = 0;
  options.periodic_compaction_seconds = 100;
  Reopen(options);
  ASSERT_EQ(0, dbfull()->GetOptions().ttl);
  ASSERT_EQ(100, dbfull()->GetOptions().periodic_compaction_seconds);

  options.ttl = 100;
  options.periodic_compaction_seconds = 0;
  Reopen(options);
  ASSERT_EQ(100, dbfull()->GetOptions().ttl);
  ASSERT_EQ(100, dbfull()->GetOptions().periodic_compaction_seconds);

  options.ttl = 100;
  options.periodic_compaction_seconds = 500;
  Reopen(options);
  ASSERT_EQ(100, dbfull()->GetOptions().ttl);
  ASSERT_EQ(100, dbfull()->GetOptions().periodic_compaction_seconds);
}

TEST_F(DBOptionsTest, SanitizeTtlDefault) {
  Options options;
  options.env = CurrentOptions().env;
  Reopen(options);
  ASSERT_EQ(30 * 24 * 60 * 60, dbfull()->GetOptions().ttl);

  options.compaction_style = kCompactionStyleLevel;
  options.ttl = 0;
  Reopen(options);
  ASSERT_EQ(0, dbfull()->GetOptions().ttl);

  options.ttl = 100;
  Reopen(options);
  ASSERT_EQ(100, dbfull()->GetOptions().ttl);
}

TEST_F(DBOptionsTest, SanitizeFIFOPeriodicCompaction) {
  Options options;
  options.compaction_style = kCompactionStyleFIFO;
  options.env = CurrentOptions().env;
  options.ttl = 0;
  Reopen(options);
  ASSERT_EQ(30 * 24 * 60 * 60, dbfull()->GetOptions().ttl);

  options.ttl = 100;
  Reopen(options);
  ASSERT_EQ(100, dbfull()->GetOptions().ttl);

  options.ttl = 100 * 24 * 60 * 60;
  Reopen(options);
  ASSERT_EQ(100 * 24 * 60 * 60, dbfull()->GetOptions().ttl);

  options.ttl = 200;
  options.periodic_compaction_seconds = 300;
  Reopen(options);
  ASSERT_EQ(200, dbfull()->GetOptions().ttl);

  options.ttl = 500;
  options.periodic_compaction_seconds = 300;
  Reopen(options);
  ASSERT_EQ(300, dbfull()->GetOptions().ttl);
}

TEST_F(DBOptionsTest, SetFIFOCompactionOptions) {
  Options options;
  options.env = CurrentOptions().env;
  options.compaction_style = kCompactionStyleFIFO;
  options.write_buffer_size = 10 << 10;  // 10KB
  options.arena_block_size = 4096;
  options.compression = kNoCompression;
  options.create_if_missing = true;
  options.compaction_options_fifo.allow_compaction = false;
  env_->SetMockSleep();
  options.env = env_;

  // NOTE: Presumed unnecessary and removed: resetting mock time in env

  // Test dynamically changing ttl.
  options.ttl = 1 * 60 * 60;  // 1 hour
  ASSERT_OK(TryReopen(options));

  Random rnd(301);
  for (int i = 0; i < 10; i++) {
    // Generate and flush a file about 10KB.
    for (int j = 0; j < 10; j++) {
      ASSERT_OK(Put(ToString(i * 20 + j), rnd.RandomString(980)));
    }
    ASSERT_OK(Flush());
  }
  ASSERT_OK(dbfull()->TEST_WaitForCompact());
  ASSERT_EQ(NumTableFilesAtLevel(0), 10);

  env_->MockSleepForSeconds(61);

  // No files should be compacted as ttl is set to 1 hour.
  ASSERT_EQ(dbfull()->GetOptions().ttl, 3600);
  ASSERT_OK(dbfull()->CompactRange(CompactRangeOptions(), nullptr, nullptr));
  ASSERT_EQ(NumTableFilesAtLevel(0), 10);

  // Set ttl to 1 minute. So all files should get deleted.
  ASSERT_OK(dbfull()->SetOptions({{"ttl", "60"}}));
  ASSERT_EQ(dbfull()->GetOptions().ttl, 60);
  ASSERT_OK(dbfull()->CompactRange(CompactRangeOptions(), nullptr, nullptr));
  ASSERT_OK(dbfull()->TEST_WaitForCompact());
  ASSERT_EQ(NumTableFilesAtLevel(0), 0);

  // NOTE: Presumed unnecessary and removed: resetting mock time in env

  // Test dynamically changing compaction_options_fifo.max_table_files_size
  options.compaction_options_fifo.max_table_files_size = 500 << 10;  // 00KB
  options.ttl = 0;
  DestroyAndReopen(options);

  for (int i = 0; i < 10; i++) {
    // Generate and flush a file about 10KB.
    for (int j = 0; j < 10; j++) {
      ASSERT_OK(Put(ToString(i * 20 + j), rnd.RandomString(980)));
    }
    ASSERT_OK(Flush());
  }
  ASSERT_OK(dbfull()->TEST_WaitForCompact());
  ASSERT_EQ(NumTableFilesAtLevel(0), 10);

  // No files should be compacted as max_table_files_size is set to 500 KB.
  ASSERT_EQ(dbfull()->GetOptions().compaction_options_fifo.max_table_files_size,
            500 << 10);
  ASSERT_OK(dbfull()->CompactRange(CompactRangeOptions(), nullptr, nullptr));
  ASSERT_EQ(NumTableFilesAtLevel(0), 10);

  // Set max_table_files_size to 12 KB. So only 1 file should remain now.
  ASSERT_OK(dbfull()->SetOptions(
      {{"compaction_options_fifo", "{max_table_files_size=12288;}"}}));
  ASSERT_EQ(dbfull()->GetOptions().compaction_options_fifo.max_table_files_size,
            12 << 10);
  ASSERT_OK(dbfull()->CompactRange(CompactRangeOptions(), nullptr, nullptr));
  ASSERT_OK(dbfull()->TEST_WaitForCompact());
  ASSERT_EQ(NumTableFilesAtLevel(0), 1);

  // Test dynamically changing compaction_options_fifo.allow_compaction
  options.compaction_options_fifo.max_table_files_size = 500 << 10;  // 500KB
  options.ttl = 0;
  options.compaction_options_fifo.allow_compaction = false;
  options.level0_file_num_compaction_trigger = 6;
  DestroyAndReopen(options);

  for (int i = 0; i < 10; i++) {
    // Generate and flush a file about 10KB.
    for (int j = 0; j < 10; j++) {
      ASSERT_OK(Put(ToString(i * 20 + j), rnd.RandomString(980)));
    }
    ASSERT_OK(Flush());
  }
  ASSERT_OK(dbfull()->TEST_WaitForCompact());
  ASSERT_EQ(NumTableFilesAtLevel(0), 10);

  // No files should be compacted as max_table_files_size is set to 500 KB and
  // allow_compaction is false
  ASSERT_EQ(dbfull()->GetOptions().compaction_options_fifo.allow_compaction,
            false);
  ASSERT_OK(dbfull()->CompactRange(CompactRangeOptions(), nullptr, nullptr));
  ASSERT_EQ(NumTableFilesAtLevel(0), 10);

  // Set allow_compaction to true. So number of files should be between 1 and 5.
  ASSERT_OK(dbfull()->SetOptions(
      {{"compaction_options_fifo", "{allow_compaction=true;}"}}));
  ASSERT_EQ(dbfull()->GetOptions().compaction_options_fifo.allow_compaction,
            true);
  ASSERT_OK(dbfull()->CompactRange(CompactRangeOptions(), nullptr, nullptr));
  ASSERT_OK(dbfull()->TEST_WaitForCompact());
  ASSERT_GE(NumTableFilesAtLevel(0), 1);
  ASSERT_LE(NumTableFilesAtLevel(0), 5);
}

TEST_F(DBOptionsTest, CompactionReadaheadSizeChange) {
  SpecialEnv env(env_);
  Options options;
  options.env = &env;

  options.compaction_readahead_size = 0;
  options.new_table_reader_for_compaction_inputs = true;
  options.level0_file_num_compaction_trigger = 2;
  const std::string kValue(1024, 'v');
  Reopen(options);

  ASSERT_EQ(0, dbfull()->GetDBOptions().compaction_readahead_size);
  ASSERT_OK(dbfull()->SetDBOptions({{"compaction_readahead_size", "256"}}));
  ASSERT_EQ(256, dbfull()->GetDBOptions().compaction_readahead_size);
  for (int i = 0; i < 1024; i++) {
    ASSERT_OK(Put(Key(i), kValue));
  }
  ASSERT_OK(Flush());
  for (int i = 0; i < 1024 * 2; i++) {
    ASSERT_OK(Put(Key(i), kValue));
  }
  ASSERT_OK(Flush());
  ASSERT_OK(dbfull()->TEST_WaitForCompact());
  ASSERT_EQ(256, env_->compaction_readahead_size_);
  Close();
}

TEST_F(DBOptionsTest, FIFOTtlBackwardCompatible) {
  Options options;
  options.compaction_style = kCompactionStyleFIFO;
  options.write_buffer_size = 10 << 10;  // 10KB
  options.create_if_missing = true;
  options.env = CurrentOptions().env;

  ASSERT_OK(TryReopen(options));

  Random rnd(301);
  for (int i = 0; i < 10; i++) {
    // Generate and flush a file about 10KB.
    for (int j = 0; j < 10; j++) {
      ASSERT_OK(Put(ToString(i * 20 + j), rnd.RandomString(980)));
    }
    ASSERT_OK(Flush());
  }
  ASSERT_OK(dbfull()->TEST_WaitForCompact());
  ASSERT_EQ(NumTableFilesAtLevel(0), 10);

  // In release 6.0, ttl was promoted from a secondary level option under
  // compaction_options_fifo to a top level option under ColumnFamilyOptions.
  // We still need to handle old SetOptions calls but should ignore
  // ttl under compaction_options_fifo.
  ASSERT_OK(dbfull()->SetOptions(
      {{"compaction_options_fifo",
        "{allow_compaction=true;max_table_files_size=1024;ttl=731;}"},
       {"ttl", "60"}}));
  ASSERT_EQ(dbfull()->GetOptions().compaction_options_fifo.allow_compaction,
            true);
  ASSERT_EQ(dbfull()->GetOptions().compaction_options_fifo.max_table_files_size,
            1024);
  ASSERT_EQ(dbfull()->GetOptions().ttl, 60);

  // Put ttl as the first option inside compaction_options_fifo. That works as
  // it doesn't overwrite any other option.
  ASSERT_OK(dbfull()->SetOptions(
      {{"compaction_options_fifo",
        "{ttl=985;allow_compaction=true;max_table_files_size=1024;}"},
       {"ttl", "191"}}));
  ASSERT_EQ(dbfull()->GetOptions().compaction_options_fifo.allow_compaction,
            true);
  ASSERT_EQ(dbfull()->GetOptions().compaction_options_fifo.max_table_files_size,
            1024);
  ASSERT_EQ(dbfull()->GetOptions().ttl, 191);
}

TEST_F(DBOptionsTest, ChangeCompression) {
  if (!Snappy_Supported() || !LZ4_Supported()) {
    return;
  }
  Options options;
  options.write_buffer_size = 10 << 10;  // 10KB
  options.level0_file_num_compaction_trigger = 2;
  options.create_if_missing = true;
  options.compression = CompressionType::kLZ4Compression;
  options.bottommost_compression = CompressionType::kNoCompression;
  options.bottommost_compression_opts.level = 2;
  options.bottommost_compression_opts.parallel_threads = 1;
  options.env = CurrentOptions().env;

  ASSERT_OK(TryReopen(options));

  CompressionType compression_used = CompressionType::kLZ4Compression;
  CompressionOptions compression_opt_used;
  bool compacted = false;
  SyncPoint::GetInstance()->SetCallBack(
      "LevelCompactionPicker::PickCompaction:Return", [&](void* arg) {
        Compaction* c = reinterpret_cast<Compaction*>(arg);
        compression_used = c->output_compression();
        compression_opt_used = c->output_compression_opts();
        compacted = true;
      });
  SyncPoint::GetInstance()->EnableProcessing();

  ASSERT_OK(Put("foo", "foofoofoo"));
  ASSERT_OK(Put("bar", "foofoofoo"));
  ASSERT_OK(Flush());
  ASSERT_OK(Put("foo", "foofoofoo"));
  ASSERT_OK(Put("bar", "foofoofoo"));
  ASSERT_OK(Flush());
  ASSERT_OK(dbfull()->TEST_WaitForCompact());
  ASSERT_TRUE(compacted);
  ASSERT_EQ(CompressionType::kNoCompression, compression_used);
  ASSERT_EQ(options.compression_opts.level, compression_opt_used.level);
  ASSERT_EQ(options.compression_opts.parallel_threads,
            compression_opt_used.parallel_threads);

  compression_used = CompressionType::kLZ4Compression;
  compacted = false;
  ASSERT_OK(dbfull()->SetOptions(
      {{"bottommost_compression", "kSnappyCompression"},
       {"bottommost_compression_opts", "0:6:0:0:4:true"}}));
  ASSERT_OK(Put("foo", "foofoofoo"));
  ASSERT_OK(Put("bar", "foofoofoo"));
  ASSERT_OK(Flush());
  ASSERT_OK(Put("foo", "foofoofoo"));
  ASSERT_OK(Put("bar", "foofoofoo"));
  ASSERT_OK(Flush());
  ASSERT_OK(dbfull()->TEST_WaitForCompact());
  ASSERT_TRUE(compacted);
  ASSERT_EQ(CompressionType::kSnappyCompression, compression_used);
  ASSERT_EQ(6, compression_opt_used.level);
  // Right now parallel_level is not yet allowed to be changed.

  SyncPoint::GetInstance()->DisableProcessing();
}

#endif  // ROCKSDB_LITE

TEST_F(DBOptionsTest, BottommostCompressionOptsWithFallbackType) {
  // Verify the bottommost compression options still take effect even when the
  // bottommost compression type is left at its default value. Verify for both
  // automatic and manual compaction.
  if (!Snappy_Supported() || !LZ4_Supported()) {
    return;
  }

  constexpr int kUpperCompressionLevel = 1;
  constexpr int kBottommostCompressionLevel = 2;
  constexpr int kNumL0Files = 2;

  Options options = CurrentOptions();
  options.level0_file_num_compaction_trigger = kNumL0Files;
  options.compression = CompressionType::kLZ4Compression;
  options.compression_opts.level = kUpperCompressionLevel;
  options.bottommost_compression_opts.level = kBottommostCompressionLevel;
  options.bottommost_compression_opts.enabled = true;
  Reopen(options);

  CompressionType compression_used = CompressionType::kDisableCompressionOption;
  CompressionOptions compression_opt_used;
  bool compacted = false;
  SyncPoint::GetInstance()->SetCallBack(
      "CompactionPicker::RegisterCompaction:Registered", [&](void* arg) {
        Compaction* c = static_cast<Compaction*>(arg);
        compression_used = c->output_compression();
        compression_opt_used = c->output_compression_opts();
        compacted = true;
      });
  SyncPoint::GetInstance()->EnableProcessing();

  // First, verify for automatic compaction.
  for (int i = 0; i < kNumL0Files; ++i) {
    ASSERT_OK(Put("foo", "foofoofoo"));
    ASSERT_OK(Put("bar", "foofoofoo"));
    ASSERT_OK(Flush());
  }
  ASSERT_OK(dbfull()->TEST_WaitForCompact());

  ASSERT_TRUE(compacted);
  ASSERT_EQ(CompressionType::kLZ4Compression, compression_used);
  ASSERT_EQ(kBottommostCompressionLevel, compression_opt_used.level);

  // Second, verify for manual compaction.
  compacted = false;
  compression_used = CompressionType::kDisableCompressionOption;
  compression_opt_used = CompressionOptions();
  CompactRangeOptions cro;
  cro.bottommost_level_compaction = BottommostLevelCompaction::kForceOptimized;
  ASSERT_OK(dbfull()->CompactRange(cro, nullptr, nullptr));

  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->DisableProcessing();
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->ClearAllCallBacks();

  ASSERT_TRUE(compacted);
  ASSERT_EQ(CompressionType::kLZ4Compression, compression_used);
  ASSERT_EQ(kBottommostCompressionLevel, compression_opt_used.level);
}

}  // namespace ROCKSDB_NAMESPACE

int main(int argc, char** argv) {
  ROCKSDB_NAMESPACE::port::InstallStackTraceHandler();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
