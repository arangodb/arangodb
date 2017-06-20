//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//  This source code is also licensed under the GPLv2 license found in the
//  COPYING file in the root directory of this source tree.
#include <string>
#include <vector>

#include "db/db_test_util.h"
#include "db/forward_iterator.h"
#include "port/stack_trace.h"
#include "utilities/merge_operators.h"

namespace rocksdb {

// Test merge operator functionality.
class DBMergeOperatorTest : public DBTestBase {
 public:
  DBMergeOperatorTest() : DBTestBase("/db_merge_operator_test") {}
};

TEST_F(DBMergeOperatorTest, MergeErrorOnRead) {
  Options options;
  options.create_if_missing = true;
  options.merge_operator.reset(new TestPutOperator());
  options.env = env_;
  Reopen(options);
  ASSERT_OK(Merge("k1", "v1"));
  ASSERT_OK(Merge("k1", "corrupted"));
  std::string value;
  ASSERT_TRUE(db_->Get(ReadOptions(), "k1", &value).IsCorruption());
  VerifyDBInternal({{"k1", "corrupted"}, {"k1", "v1"}});
}

TEST_F(DBMergeOperatorTest, MergeErrorOnWrite) {
  Options options;
  options.create_if_missing = true;
  options.merge_operator.reset(new TestPutOperator());
  options.max_successive_merges = 3;
  options.env = env_;
  Reopen(options);
  ASSERT_OK(Merge("k1", "v1"));
  ASSERT_OK(Merge("k1", "v2"));
  // Will trigger a merge when hitting max_successive_merges and the merge
  // will fail. The delta will be inserted nevertheless.
  ASSERT_OK(Merge("k1", "corrupted"));
  // Data should stay unmerged after the error.
  VerifyDBInternal({{"k1", "corrupted"}, {"k1", "v2"}, {"k1", "v1"}});
}

TEST_F(DBMergeOperatorTest, MergeErrorOnIteration) {
  Options options;
  options.create_if_missing = true;
  options.merge_operator.reset(new TestPutOperator());
  options.env = env_;

  DestroyAndReopen(options);
  ASSERT_OK(Merge("k1", "v1"));
  ASSERT_OK(Merge("k1", "corrupted"));
  ASSERT_OK(Put("k2", "v2"));
  VerifyDBFromMap({{"k1", ""}, {"k2", "v2"}}, nullptr, false,
                  {{"k1", Status::Corruption()}});
  VerifyDBInternal({{"k1", "corrupted"}, {"k1", "v1"}, {"k2", "v2"}});

  DestroyAndReopen(options);
  ASSERT_OK(Merge("k1", "v1"));
  ASSERT_OK(Put("k2", "v2"));
  ASSERT_OK(Merge("k2", "corrupted"));
  VerifyDBFromMap({{"k1", "v1"}, {"k2", ""}}, nullptr, false,
                  {{"k2", Status::Corruption()}});
  VerifyDBInternal({{"k1", "v1"}, {"k2", "corrupted"}, {"k2", "v2"}});
}


class MergeOperatorPinningTest : public DBMergeOperatorTest,
                                 public testing::WithParamInterface<bool> {
 public:
  MergeOperatorPinningTest() { disable_block_cache_ = GetParam(); }

  bool disable_block_cache_;
};

INSTANTIATE_TEST_CASE_P(MergeOperatorPinningTest, MergeOperatorPinningTest,
                        ::testing::Bool());

#ifndef ROCKSDB_LITE
TEST_P(MergeOperatorPinningTest, OperandsMultiBlocks) {
  Options options = CurrentOptions();
  BlockBasedTableOptions table_options;
  table_options.block_size = 1;  // every block will contain one entry
  table_options.no_block_cache = disable_block_cache_;
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));
  options.merge_operator = MergeOperators::CreateStringAppendTESTOperator();
  options.level0_slowdown_writes_trigger = (1 << 30);
  options.level0_stop_writes_trigger = (1 << 30);
  options.disable_auto_compactions = true;
  DestroyAndReopen(options);

  const int kKeysPerFile = 10;
  const int kOperandsPerKeyPerFile = 7;
  const int kOperandSize = 100;
  // Filse to write in L0 before compacting to lower level
  const int kFilesPerLevel = 3;

  Random rnd(301);
  std::map<std::string, std::string> true_data;
  int batch_num = 1;
  int lvl_to_fill = 4;
  int key_id = 0;
  while (true) {
    for (int j = 0; j < kKeysPerFile; j++) {
      std::string key = Key(key_id % 35);
      key_id++;
      for (int k = 0; k < kOperandsPerKeyPerFile; k++) {
        std::string val = RandomString(&rnd, kOperandSize);
        ASSERT_OK(db_->Merge(WriteOptions(), key, val));
        if (true_data[key].size() == 0) {
          true_data[key] = val;
        } else {
          true_data[key] += "," + val;
        }
      }
    }

    if (lvl_to_fill == -1) {
      // Keep last batch in memtable and stop
      break;
    }

    ASSERT_OK(Flush());
    if (batch_num % kFilesPerLevel == 0) {
      if (lvl_to_fill != 0) {
        MoveFilesToLevel(lvl_to_fill);
      }
      lvl_to_fill--;
    }
    batch_num++;
  }

  // 3 L0 files
  // 1 L1 file
  // 3 L2 files
  // 1 L3 file
  // 3 L4 Files
  ASSERT_EQ(FilesPerLevel(), "3,1,3,1,3");

  VerifyDBFromMap(true_data);
}

TEST_P(MergeOperatorPinningTest, Randomized) {
  do {
    Options options = CurrentOptions();
    options.merge_operator = MergeOperators::CreateMaxOperator();
    BlockBasedTableOptions table_options;
    table_options.no_block_cache = disable_block_cache_;
    options.table_factory.reset(NewBlockBasedTableFactory(table_options));
    DestroyAndReopen(options);

    Random rnd(301);
    std::map<std::string, std::string> true_data;

    const int kTotalMerges = 10000;
    // Every key gets ~10 operands
    const int kKeyRange = kTotalMerges / 10;
    const int kOperandSize = 20;
    const int kNumPutBefore = kKeyRange / 10;  // 10% value
    const int kNumPutAfter = kKeyRange / 10;   // 10% overwrite
    const int kNumDelete = kKeyRange / 10;     // 10% delete

    // kNumPutBefore keys will have base values
    for (int i = 0; i < kNumPutBefore; i++) {
      std::string key = Key(rnd.Next() % kKeyRange);
      std::string value = RandomString(&rnd, kOperandSize);
      ASSERT_OK(db_->Put(WriteOptions(), key, value));

      true_data[key] = value;
    }

    // Do kTotalMerges merges
    for (int i = 0; i < kTotalMerges; i++) {
      std::string key = Key(rnd.Next() % kKeyRange);
      std::string value = RandomString(&rnd, kOperandSize);
      ASSERT_OK(db_->Merge(WriteOptions(), key, value));

      if (true_data[key] < value) {
        true_data[key] = value;
      }
    }

    // Overwrite random kNumPutAfter keys
    for (int i = 0; i < kNumPutAfter; i++) {
      std::string key = Key(rnd.Next() % kKeyRange);
      std::string value = RandomString(&rnd, kOperandSize);
      ASSERT_OK(db_->Put(WriteOptions(), key, value));

      true_data[key] = value;
    }

    // Delete random kNumDelete keys
    for (int i = 0; i < kNumDelete; i++) {
      std::string key = Key(rnd.Next() % kKeyRange);
      ASSERT_OK(db_->Delete(WriteOptions(), key));

      true_data.erase(key);
    }

    VerifyDBFromMap(true_data);

    // Skip HashCuckoo since it does not support merge operators
  } while (ChangeOptions(kSkipMergePut | kSkipHashCuckoo));
}

class MergeOperatorHook : public MergeOperator {
 public:
  explicit MergeOperatorHook(std::shared_ptr<MergeOperator> _merge_op)
      : merge_op_(_merge_op) {}

  virtual bool FullMergeV2(const MergeOperationInput& merge_in,
                           MergeOperationOutput* merge_out) const override {
    before_merge_();
    bool res = merge_op_->FullMergeV2(merge_in, merge_out);
    after_merge_();
    return res;
  }

  virtual const char* Name() const override { return merge_op_->Name(); }

  std::shared_ptr<MergeOperator> merge_op_;
  std::function<void()> before_merge_ = []() {};
  std::function<void()> after_merge_ = []() {};
};

TEST_P(MergeOperatorPinningTest, EvictCacheBeforeMerge) {
  Options options = CurrentOptions();

  auto merge_hook =
      std::make_shared<MergeOperatorHook>(MergeOperators::CreateMaxOperator());
  options.merge_operator = merge_hook;
  options.disable_auto_compactions = true;
  options.level0_slowdown_writes_trigger = (1 << 30);
  options.level0_stop_writes_trigger = (1 << 30);
  options.max_open_files = 20;
  BlockBasedTableOptions bbto;
  bbto.no_block_cache = disable_block_cache_;
  if (bbto.no_block_cache == false) {
    bbto.block_cache = NewLRUCache(64 * 1024 * 1024);
  } else {
    bbto.block_cache = nullptr;
  }
  options.table_factory.reset(NewBlockBasedTableFactory(bbto));
  DestroyAndReopen(options);

  const int kNumOperands = 30;
  const int kNumKeys = 1000;
  const int kOperandSize = 100;
  Random rnd(301);

  // 1000 keys every key have 30 operands, every operand is in a different file
  std::map<std::string, std::string> true_data;
  for (int i = 0; i < kNumOperands; i++) {
    for (int j = 0; j < kNumKeys; j++) {
      std::string k = Key(j);
      std::string v = RandomString(&rnd, kOperandSize);
      ASSERT_OK(db_->Merge(WriteOptions(), k, v));

      true_data[k] = std::max(true_data[k], v);
    }
    ASSERT_OK(Flush());
  }

  std::vector<uint64_t> file_numbers = ListTableFiles(env_, dbname_);
  ASSERT_EQ(file_numbers.size(), kNumOperands);
  int merge_cnt = 0;

  // Code executed before merge operation
  merge_hook->before_merge_ = [&]() {
    // Evict all tables from cache before every merge operation
    for (uint64_t num : file_numbers) {
      TableCache::Evict(dbfull()->TEST_table_cache(), num);
    }
    // Decrease cache capacity to force all unrefed blocks to be evicted
    if (bbto.block_cache) {
      bbto.block_cache->SetCapacity(1);
    }
    merge_cnt++;
  };

  // Code executed after merge operation
  merge_hook->after_merge_ = [&]() {
    // Increase capacity again after doing the merge
    if (bbto.block_cache) {
      bbto.block_cache->SetCapacity(64 * 1024 * 1024);
    }
  };

  size_t total_reads;
  VerifyDBFromMap(true_data, &total_reads);
  ASSERT_EQ(merge_cnt, total_reads);

  db_->CompactRange(CompactRangeOptions(), nullptr, nullptr);

  VerifyDBFromMap(true_data, &total_reads);
}

TEST_P(MergeOperatorPinningTest, TailingIterator) {
  Options options = CurrentOptions();
  options.merge_operator = MergeOperators::CreateMaxOperator();
  BlockBasedTableOptions bbto;
  bbto.no_block_cache = disable_block_cache_;
  options.table_factory.reset(NewBlockBasedTableFactory(bbto));
  DestroyAndReopen(options);

  const int kNumOperands = 100;
  const int kNumWrites = 100000;

  std::function<void()> writer_func = [&]() {
    int k = 0;
    for (int i = 0; i < kNumWrites; i++) {
      db_->Merge(WriteOptions(), Key(k), Key(k));

      if (i && i % kNumOperands == 0) {
        k++;
      }
      if (i && i % 127 == 0) {
        ASSERT_OK(Flush());
      }
      if (i && i % 317 == 0) {
        ASSERT_OK(db_->CompactRange(CompactRangeOptions(), nullptr, nullptr));
      }
    }
  };

  std::function<void()> reader_func = [&]() {
    ReadOptions ro;
    ro.tailing = true;
    Iterator* iter = db_->NewIterator(ro);

    iter->SeekToFirst();
    for (int i = 0; i < (kNumWrites / kNumOperands); i++) {
      while (!iter->Valid()) {
        // wait for the key to be written
        env_->SleepForMicroseconds(100);
        iter->Seek(Key(i));
      }
      ASSERT_EQ(iter->key(), Key(i));
      ASSERT_EQ(iter->value(), Key(i));

      iter->Next();
    }

    delete iter;
  };

  rocksdb::port::Thread writer_thread(writer_func);
  rocksdb::port::Thread reader_thread(reader_func);

  writer_thread.join();
  reader_thread.join();
}
#endif  // ROCKSDB_LITE

}  // namespace rocksdb

int main(int argc, char** argv) {
  rocksdb::port::InstallStackTraceHandler();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
