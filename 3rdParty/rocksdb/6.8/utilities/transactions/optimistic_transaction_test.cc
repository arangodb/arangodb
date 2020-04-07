//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#ifndef ROCKSDB_LITE

#include <functional>
#include <string>
#include <thread>

#include "db/db_impl/db_impl.h"
#include "logging/logging.h"
#include "port/port.h"
#include "rocksdb/db.h"
#include "rocksdb/perf_context.h"
#include "rocksdb/utilities/optimistic_transaction_db.h"
#include "rocksdb/utilities/transaction.h"
#include "test_util/sync_point.h"
#include "test_util/testharness.h"
#include "test_util/transaction_test_util.h"
#include "util/crc32c.h"
#include "util/random.h"

using std::string;

namespace ROCKSDB_NAMESPACE {

class OptimisticTransactionTest
    : public testing::Test,
      public testing::WithParamInterface<OccValidationPolicy> {
 public:
  OptimisticTransactionDB* txn_db;
  string dbname;
  Options options;

  OptimisticTransactionTest() {
    options.create_if_missing = true;
    options.max_write_buffer_number = 2;
    options.max_write_buffer_size_to_maintain = 1600;
    dbname = test::PerThreadDBPath("optimistic_transaction_testdb");

    DestroyDB(dbname, options);
    Open();
  }
  ~OptimisticTransactionTest() override {
    delete txn_db;
    DestroyDB(dbname, options);
  }

  void Reopen() {
    delete txn_db;
    txn_db = nullptr;
    Open();
  }

private:
  void Open() {
    ColumnFamilyOptions cf_options(options);
    OptimisticTransactionDBOptions occ_opts;
    occ_opts.validate_policy = GetParam();
    std::vector<ColumnFamilyDescriptor> column_families;
    std::vector<ColumnFamilyHandle*> handles;
    column_families.push_back(
        ColumnFamilyDescriptor(kDefaultColumnFamilyName, cf_options));
    Status s =
        OptimisticTransactionDB::Open(DBOptions(options), occ_opts, dbname,
                                      column_families, &handles, &txn_db);

    assert(s.ok());
    assert(txn_db != nullptr);
    assert(handles.size() == 1);
    delete handles[0];
  }
};

TEST_P(OptimisticTransactionTest, SuccessTest) {
  WriteOptions write_options;
  ReadOptions read_options;
  string value;
  Status s;

  txn_db->Put(write_options, Slice("foo"), Slice("bar"));
  txn_db->Put(write_options, Slice("foo2"), Slice("bar"));

  Transaction* txn = txn_db->BeginTransaction(write_options);
  ASSERT_TRUE(txn);

  txn->GetForUpdate(read_options, "foo", &value);
  ASSERT_EQ(value, "bar");

  txn->Put(Slice("foo"), Slice("bar2"));

  txn->GetForUpdate(read_options, "foo", &value);
  ASSERT_EQ(value, "bar2");

  s = txn->Commit();
  ASSERT_OK(s);

  txn_db->Get(read_options, "foo", &value);
  ASSERT_EQ(value, "bar2");

  delete txn;
}

TEST_P(OptimisticTransactionTest, WriteConflictTest) {
  WriteOptions write_options;
  ReadOptions read_options;
  string value;
  Status s;

  txn_db->Put(write_options, "foo", "bar");
  txn_db->Put(write_options, "foo2", "bar");

  Transaction* txn = txn_db->BeginTransaction(write_options);
  ASSERT_TRUE(txn);

  txn->Put("foo", "bar2");

  // This Put outside of a transaction will conflict with the previous write
  s = txn_db->Put(write_options, "foo", "barz");
  ASSERT_OK(s);

  s = txn_db->Get(read_options, "foo", &value);
  ASSERT_EQ(value, "barz");
  ASSERT_EQ(1, txn->GetNumKeys());

  s = txn->Commit();
  ASSERT_TRUE(s.IsBusy());  // Txn should not commit

  // Verify that transaction did not write anything
  txn_db->Get(read_options, "foo", &value);
  ASSERT_EQ(value, "barz");
  txn_db->Get(read_options, "foo2", &value);
  ASSERT_EQ(value, "bar");

  delete txn;
}

TEST_P(OptimisticTransactionTest, WriteConflictTest2) {
  WriteOptions write_options;
  ReadOptions read_options;
  OptimisticTransactionOptions txn_options;
  string value;
  Status s;

  txn_db->Put(write_options, "foo", "bar");
  txn_db->Put(write_options, "foo2", "bar");

  txn_options.set_snapshot = true;
  Transaction* txn = txn_db->BeginTransaction(write_options, txn_options);
  ASSERT_TRUE(txn);

  // This Put outside of a transaction will conflict with a later write
  s = txn_db->Put(write_options, "foo", "barz");
  ASSERT_OK(s);

  txn->Put("foo", "bar2");  // Conflicts with write done after snapshot taken

  s = txn_db->Get(read_options, "foo", &value);
  ASSERT_EQ(value, "barz");

  s = txn->Commit();
  ASSERT_TRUE(s.IsBusy());  // Txn should not commit

  // Verify that transaction did not write anything
  txn_db->Get(read_options, "foo", &value);
  ASSERT_EQ(value, "barz");
  txn_db->Get(read_options, "foo2", &value);
  ASSERT_EQ(value, "bar");

  delete txn;
}

TEST_P(OptimisticTransactionTest, ReadConflictTest) {
  WriteOptions write_options;
  ReadOptions read_options, snapshot_read_options;
  OptimisticTransactionOptions txn_options;
  string value;
  Status s;

  txn_db->Put(write_options, "foo", "bar");
  txn_db->Put(write_options, "foo2", "bar");

  txn_options.set_snapshot = true;
  Transaction* txn = txn_db->BeginTransaction(write_options, txn_options);
  ASSERT_TRUE(txn);

  txn->SetSnapshot();
  snapshot_read_options.snapshot = txn->GetSnapshot();

  txn->GetForUpdate(snapshot_read_options, "foo", &value);
  ASSERT_EQ(value, "bar");

  // This Put outside of a transaction will conflict with the previous read
  s = txn_db->Put(write_options, "foo", "barz");
  ASSERT_OK(s);

  s = txn_db->Get(read_options, "foo", &value);
  ASSERT_EQ(value, "barz");

  s = txn->Commit();
  ASSERT_TRUE(s.IsBusy());  // Txn should not commit

  // Verify that transaction did not write anything
  txn->GetForUpdate(read_options, "foo", &value);
  ASSERT_EQ(value, "barz");
  txn->GetForUpdate(read_options, "foo2", &value);
  ASSERT_EQ(value, "bar");

  delete txn;
}

TEST_P(OptimisticTransactionTest, TxnOnlyTest) {
  // Test to make sure transactions work when there are no other writes in an
  // empty db.

  WriteOptions write_options;
  ReadOptions read_options;
  string value;
  Status s;

  Transaction* txn = txn_db->BeginTransaction(write_options);
  ASSERT_TRUE(txn);

  txn->Put("x", "y");

  s = txn->Commit();
  ASSERT_OK(s);

  delete txn;
}

TEST_P(OptimisticTransactionTest, FlushTest) {
  WriteOptions write_options;
  ReadOptions read_options, snapshot_read_options;
  string value;
  Status s;

  txn_db->Put(write_options, Slice("foo"), Slice("bar"));
  txn_db->Put(write_options, Slice("foo2"), Slice("bar"));

  Transaction* txn = txn_db->BeginTransaction(write_options);
  ASSERT_TRUE(txn);

  snapshot_read_options.snapshot = txn->GetSnapshot();

  txn->GetForUpdate(snapshot_read_options, "foo", &value);
  ASSERT_EQ(value, "bar");

  txn->Put(Slice("foo"), Slice("bar2"));

  txn->GetForUpdate(snapshot_read_options, "foo", &value);
  ASSERT_EQ(value, "bar2");

  // Put a random key so we have a memtable to flush
  s = txn_db->Put(write_options, "dummy", "dummy");
  ASSERT_OK(s);

  // force a memtable flush
  FlushOptions flush_ops;
  txn_db->Flush(flush_ops);

  s = txn->Commit();
  // txn should commit since the flushed table is still in MemtableList History
  ASSERT_OK(s);

  txn_db->Get(read_options, "foo", &value);
  ASSERT_EQ(value, "bar2");

  delete txn;
}

TEST_P(OptimisticTransactionTest, FlushTest2) {
  WriteOptions write_options;
  ReadOptions read_options, snapshot_read_options;
  string value;
  Status s;

  txn_db->Put(write_options, Slice("foo"), Slice("bar"));
  txn_db->Put(write_options, Slice("foo2"), Slice("bar"));

  Transaction* txn = txn_db->BeginTransaction(write_options);
  ASSERT_TRUE(txn);

  snapshot_read_options.snapshot = txn->GetSnapshot();

  txn->GetForUpdate(snapshot_read_options, "foo", &value);
  ASSERT_EQ(value, "bar");

  txn->Put(Slice("foo"), Slice("bar2"));

  txn->GetForUpdate(snapshot_read_options, "foo", &value);
  ASSERT_EQ(value, "bar2");

  // Put a random key so we have a MemTable to flush
  s = txn_db->Put(write_options, "dummy", "dummy");
  ASSERT_OK(s);

  // force a memtable flush
  FlushOptions flush_ops;
  txn_db->Flush(flush_ops);

  // Put a random key so we have a MemTable to flush
  s = txn_db->Put(write_options, "dummy", "dummy2");
  ASSERT_OK(s);

  // force a memtable flush
  txn_db->Flush(flush_ops);

  s = txn_db->Put(write_options, "dummy", "dummy3");
  ASSERT_OK(s);

  // force a memtable flush
  // Since our test db has max_write_buffer_number=2, this flush will cause
  // the first memtable to get purged from the MemtableList history.
  txn_db->Flush(flush_ops);

  s = txn->Commit();
  // txn should not commit since MemTableList History is not large enough
  ASSERT_TRUE(s.IsTryAgain());

  txn_db->Get(read_options, "foo", &value);
  ASSERT_EQ(value, "bar");

  delete txn;
}

// Trigger the condition where some old memtables are skipped when doing
// TransactionUtil::CheckKey(), and make sure the result is still correct.
TEST_P(OptimisticTransactionTest, CheckKeySkipOldMemtable) {
  const int kAttemptHistoryMemtable = 0;
  const int kAttemptImmMemTable = 1;
  for (int attempt = kAttemptHistoryMemtable; attempt <= kAttemptImmMemTable;
       attempt++) {
    options.max_write_buffer_number_to_maintain = 3;
    Reopen();

    WriteOptions write_options;
    ReadOptions read_options;
    ReadOptions snapshot_read_options;
    ReadOptions snapshot_read_options2;
    string value;
    Status s;

    ASSERT_OK(txn_db->Put(write_options, Slice("foo"), Slice("bar")));
    ASSERT_OK(txn_db->Put(write_options, Slice("foo2"), Slice("bar")));

    Transaction* txn = txn_db->BeginTransaction(write_options);
    ASSERT_TRUE(txn != nullptr);

    Transaction* txn2 = txn_db->BeginTransaction(write_options);
    ASSERT_TRUE(txn2 != nullptr);

    snapshot_read_options.snapshot = txn->GetSnapshot();
    ASSERT_OK(txn->GetForUpdate(snapshot_read_options, "foo", &value));
    ASSERT_EQ(value, "bar");
    ASSERT_OK(txn->Put(Slice("foo"), Slice("bar2")));

    snapshot_read_options2.snapshot = txn2->GetSnapshot();
    ASSERT_OK(txn2->GetForUpdate(snapshot_read_options2, "foo2", &value));
    ASSERT_EQ(value, "bar");
    ASSERT_OK(txn2->Put(Slice("foo2"), Slice("bar2")));

    // txn updates "foo" and txn2 updates "foo2", and now a write is
    // issued for "foo", which conflicts with txn but not txn2
    ASSERT_OK(txn_db->Put(write_options, "foo", "bar"));

    if (attempt == kAttemptImmMemTable) {
      // For the second attempt, hold flush from beginning. The memtable
      // will be switched to immutable after calling TEST_SwitchMemtable()
      // while CheckKey() is called.
      ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->LoadDependency(
          {{"OptimisticTransactionTest.CheckKeySkipOldMemtable",
            "FlushJob::Start"}});
      ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->EnableProcessing();
    }

    // force a memtable flush. The memtable should still be kept
    FlushOptions flush_ops;
    if (attempt == kAttemptHistoryMemtable) {
      ASSERT_OK(txn_db->Flush(flush_ops));
    } else {
      assert(attempt == kAttemptImmMemTable);
      DBImpl* db_impl = static_cast<DBImpl*>(txn_db->GetRootDB());
      db_impl->TEST_SwitchMemtable();
    }
    uint64_t num_imm_mems;
    ASSERT_TRUE(txn_db->GetIntProperty(DB::Properties::kNumImmutableMemTable,
                                       &num_imm_mems));
    if (attempt == kAttemptHistoryMemtable) {
      ASSERT_EQ(0, num_imm_mems);
    } else {
      assert(attempt == kAttemptImmMemTable);
      ASSERT_EQ(1, num_imm_mems);
    }

    // Put something in active memtable
    ASSERT_OK(txn_db->Put(write_options, Slice("foo3"), Slice("bar")));

    // Create txn3 after flushing, when this transaction is commited,
    // only need to check the active memtable
    Transaction* txn3 = txn_db->BeginTransaction(write_options);
    ASSERT_TRUE(txn3 != nullptr);

    // Commit both of txn and txn2. txn will conflict but txn2 will
    // pass. In both ways, both memtables are queried.
    SetPerfLevel(PerfLevel::kEnableCount);

    get_perf_context()->Reset();
    s = txn->Commit();
    // We should have checked two memtables
    ASSERT_EQ(2, get_perf_context()->get_from_memtable_count);
    // txn should fail because of conflict, even if the memtable
    // has flushed, because it is still preserved in history.
    ASSERT_TRUE(s.IsBusy());

    get_perf_context()->Reset();
    s = txn2->Commit();
    // We should have checked two memtables
    ASSERT_EQ(2, get_perf_context()->get_from_memtable_count);
    ASSERT_TRUE(s.ok());

    txn3->Put(Slice("foo2"), Slice("bar2"));
    get_perf_context()->Reset();
    s = txn3->Commit();
    // txn3 is created after the active memtable is created, so that is the only
    // memtable to check.
    ASSERT_EQ(1, get_perf_context()->get_from_memtable_count);
    ASSERT_TRUE(s.ok());

    TEST_SYNC_POINT("OptimisticTransactionTest.CheckKeySkipOldMemtable");
    ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->DisableProcessing();

    SetPerfLevel(PerfLevel::kDisable);

    delete txn;
    delete txn2;
    delete txn3;
  }
}

TEST_P(OptimisticTransactionTest, NoSnapshotTest) {
  WriteOptions write_options;
  ReadOptions read_options;
  string value;
  Status s;

  txn_db->Put(write_options, "AAA", "bar");

  Transaction* txn = txn_db->BeginTransaction(write_options);
  ASSERT_TRUE(txn);

  // Modify key after transaction start
  txn_db->Put(write_options, "AAA", "bar1");

  // Read and write without a snapshot
  txn->GetForUpdate(read_options, "AAA", &value);
  ASSERT_EQ(value, "bar1");
  txn->Put("AAA", "bar2");

  // Should commit since read/write was done after data changed
  s = txn->Commit();
  ASSERT_OK(s);

  txn->GetForUpdate(read_options, "AAA", &value);
  ASSERT_EQ(value, "bar2");

  delete txn;
}

TEST_P(OptimisticTransactionTest, MultipleSnapshotTest) {
  WriteOptions write_options;
  ReadOptions read_options, snapshot_read_options;
  string value;
  Status s;

  txn_db->Put(write_options, "AAA", "bar");
  txn_db->Put(write_options, "BBB", "bar");
  txn_db->Put(write_options, "CCC", "bar");

  Transaction* txn = txn_db->BeginTransaction(write_options);
  ASSERT_TRUE(txn);

  txn_db->Put(write_options, "AAA", "bar1");

  // Read and write without a snapshot
  txn->GetForUpdate(read_options, "AAA", &value);
  ASSERT_EQ(value, "bar1");
  txn->Put("AAA", "bar2");

  // Modify BBB before snapshot is taken
  txn_db->Put(write_options, "BBB", "bar1");

  txn->SetSnapshot();
  snapshot_read_options.snapshot = txn->GetSnapshot();

  // Read and write with snapshot
  txn->GetForUpdate(snapshot_read_options, "BBB", &value);
  ASSERT_EQ(value, "bar1");
  txn->Put("BBB", "bar2");

  txn_db->Put(write_options, "CCC", "bar1");

  // Set a new snapshot
  txn->SetSnapshot();
  snapshot_read_options.snapshot = txn->GetSnapshot();

  // Read and write with snapshot
  txn->GetForUpdate(snapshot_read_options, "CCC", &value);
  ASSERT_EQ(value, "bar1");
  txn->Put("CCC", "bar2");

  s = txn->GetForUpdate(read_options, "AAA", &value);
  ASSERT_OK(s);
  ASSERT_EQ(value, "bar2");
  s = txn->GetForUpdate(read_options, "BBB", &value);
  ASSERT_OK(s);
  ASSERT_EQ(value, "bar2");
  s = txn->GetForUpdate(read_options, "CCC", &value);
  ASSERT_OK(s);
  ASSERT_EQ(value, "bar2");

  s = txn_db->Get(read_options, "AAA", &value);
  ASSERT_OK(s);
  ASSERT_EQ(value, "bar1");
  s = txn_db->Get(read_options, "BBB", &value);
  ASSERT_OK(s);
  ASSERT_EQ(value, "bar1");
  s = txn_db->Get(read_options, "CCC", &value);
  ASSERT_OK(s);
  ASSERT_EQ(value, "bar1");

  s = txn->Commit();
  ASSERT_OK(s);

  s = txn_db->Get(read_options, "AAA", &value);
  ASSERT_OK(s);
  ASSERT_EQ(value, "bar2");
  s = txn_db->Get(read_options, "BBB", &value);
  ASSERT_OK(s);
  ASSERT_EQ(value, "bar2");
  s = txn_db->Get(read_options, "CCC", &value);
  ASSERT_OK(s);
  ASSERT_EQ(value, "bar2");

  // verify that we track multiple writes to the same key at different snapshots
  delete txn;
  txn = txn_db->BeginTransaction(write_options);

  // Potentially conflicting writes
  txn_db->Put(write_options, "ZZZ", "zzz");
  txn_db->Put(write_options, "XXX", "xxx");

  txn->SetSnapshot();

  OptimisticTransactionOptions txn_options;
  txn_options.set_snapshot = true;
  Transaction* txn2 = txn_db->BeginTransaction(write_options, txn_options);
  txn2->SetSnapshot();

  // This should not conflict in txn since the snapshot is later than the
  // previous write (spoiler alert:  it will later conflict with txn2).
  txn->Put("ZZZ", "zzzz");
  s = txn->Commit();
  ASSERT_OK(s);

  delete txn;

  // This will conflict since the snapshot is earlier than another write to ZZZ
  txn2->Put("ZZZ", "xxxxx");

  s = txn2->Commit();
  ASSERT_TRUE(s.IsBusy());

  delete txn2;
}

TEST_P(OptimisticTransactionTest, ColumnFamiliesTest) {
  WriteOptions write_options;
  ReadOptions read_options, snapshot_read_options;
  OptimisticTransactionOptions txn_options;
  string value;
  Status s;

  ColumnFamilyHandle *cfa, *cfb;
  ColumnFamilyOptions cf_options;

  // Create 2 new column families
  s = txn_db->CreateColumnFamily(cf_options, "CFA", &cfa);
  ASSERT_OK(s);
  s = txn_db->CreateColumnFamily(cf_options, "CFB", &cfb);
  ASSERT_OK(s);

  delete cfa;
  delete cfb;
  delete txn_db;
  txn_db = nullptr;

  // open DB with three column families
  std::vector<ColumnFamilyDescriptor> column_families;
  // have to open default column family
  column_families.push_back(
      ColumnFamilyDescriptor(kDefaultColumnFamilyName, ColumnFamilyOptions()));
  // open the new column families
  column_families.push_back(
      ColumnFamilyDescriptor("CFA", ColumnFamilyOptions()));
  column_families.push_back(
      ColumnFamilyDescriptor("CFB", ColumnFamilyOptions()));
  std::vector<ColumnFamilyHandle*> handles;
  s = OptimisticTransactionDB::Open(options, dbname, column_families, &handles,
                                    &txn_db);
  ASSERT_OK(s);
  assert(txn_db != nullptr);

  Transaction* txn = txn_db->BeginTransaction(write_options);
  ASSERT_TRUE(txn);

  txn->SetSnapshot();
  snapshot_read_options.snapshot = txn->GetSnapshot();

  txn_options.set_snapshot = true;
  Transaction* txn2 = txn_db->BeginTransaction(write_options, txn_options);
  ASSERT_TRUE(txn2);

  // Write some data to the db
  WriteBatch batch;
  batch.Put("foo", "foo");
  batch.Put(handles[1], "AAA", "bar");
  batch.Put(handles[1], "AAAZZZ", "bar");
  s = txn_db->Write(write_options, &batch);
  ASSERT_OK(s);
  txn_db->Delete(write_options, handles[1], "AAAZZZ");

  // These keys do no conflict with existing writes since they're in
  // different column families
  txn->Delete("AAA");
  txn->GetForUpdate(snapshot_read_options, handles[1], "foo", &value);
  Slice key_slice("AAAZZZ");
  Slice value_slices[2] = {Slice("bar"), Slice("bar")};
  txn->Put(handles[2], SliceParts(&key_slice, 1), SliceParts(value_slices, 2));

  ASSERT_EQ(3, txn->GetNumKeys());

  // Txn should commit
  s = txn->Commit();
  ASSERT_OK(s);
  s = txn_db->Get(read_options, "AAA", &value);
  ASSERT_TRUE(s.IsNotFound());
  s = txn_db->Get(read_options, handles[2], "AAAZZZ", &value);
  ASSERT_EQ(value, "barbar");

  Slice key_slices[3] = {Slice("AAA"), Slice("ZZ"), Slice("Z")};
  Slice value_slice("barbarbar");
  // This write will cause a conflict with the earlier batch write
  txn2->Put(handles[1], SliceParts(key_slices, 3), SliceParts(&value_slice, 1));

  txn2->Delete(handles[2], "XXX");
  txn2->Delete(handles[1], "XXX");
  s = txn2->GetForUpdate(snapshot_read_options, handles[1], "AAA", &value);
  ASSERT_TRUE(s.IsNotFound());

  // Verify txn did not commit
  s = txn2->Commit();
  ASSERT_TRUE(s.IsBusy());
  s = txn_db->Get(read_options, handles[1], "AAAZZZ", &value);
  ASSERT_EQ(value, "barbar");

  delete txn;
  delete txn2;

  txn = txn_db->BeginTransaction(write_options, txn_options);
  snapshot_read_options.snapshot = txn->GetSnapshot();

  txn2 = txn_db->BeginTransaction(write_options, txn_options);
  ASSERT_TRUE(txn);

  std::vector<ColumnFamilyHandle*> multiget_cfh = {handles[1], handles[2],
                                                   handles[0], handles[2]};
  std::vector<Slice> multiget_keys = {"AAA", "AAAZZZ", "foo", "foo"};
  std::vector<std::string> values(4);

  std::vector<Status> results = txn->MultiGetForUpdate(
      snapshot_read_options, multiget_cfh, multiget_keys, &values);
  ASSERT_OK(results[0]);
  ASSERT_OK(results[1]);
  ASSERT_OK(results[2]);
  ASSERT_TRUE(results[3].IsNotFound());
  ASSERT_EQ(values[0], "bar");
  ASSERT_EQ(values[1], "barbar");
  ASSERT_EQ(values[2], "foo");

  txn->Delete(handles[2], "ZZZ");
  txn->Put(handles[2], "ZZZ", "YYY");
  txn->Put(handles[2], "ZZZ", "YYYY");
  txn->Delete(handles[2], "ZZZ");
  txn->Put(handles[2], "AAAZZZ", "barbarbar");

  ASSERT_EQ(5, txn->GetNumKeys());

  // Txn should commit
  s = txn->Commit();
  ASSERT_OK(s);
  s = txn_db->Get(read_options, handles[2], "ZZZ", &value);
  ASSERT_TRUE(s.IsNotFound());

  // Put a key which will conflict with the next txn using the previous snapshot
  txn_db->Put(write_options, handles[2], "foo", "000");

  results = txn2->MultiGetForUpdate(snapshot_read_options, multiget_cfh,
                                    multiget_keys, &values);
  ASSERT_OK(results[0]);
  ASSERT_OK(results[1]);
  ASSERT_OK(results[2]);
  ASSERT_TRUE(results[3].IsNotFound());
  ASSERT_EQ(values[0], "bar");
  ASSERT_EQ(values[1], "barbar");
  ASSERT_EQ(values[2], "foo");

  // Verify Txn Did not Commit
  s = txn2->Commit();
  ASSERT_TRUE(s.IsBusy());

  s = txn_db->DropColumnFamily(handles[1]);
  ASSERT_OK(s);
  s = txn_db->DropColumnFamily(handles[2]);
  ASSERT_OK(s);

  delete txn;
  delete txn2;

  for (auto handle : handles) {
    delete handle;
  }
}

TEST_P(OptimisticTransactionTest, EmptyTest) {
  WriteOptions write_options;
  ReadOptions read_options;
  string value;
  Status s;

  s = txn_db->Put(write_options, "aaa", "aaa");
  ASSERT_OK(s);

  Transaction* txn = txn_db->BeginTransaction(write_options);
  s = txn->Commit();
  ASSERT_OK(s);
  delete txn;

  txn = txn_db->BeginTransaction(write_options);
  txn->Rollback();
  delete txn;

  txn = txn_db->BeginTransaction(write_options);
  s = txn->GetForUpdate(read_options, "aaa", &value);
  ASSERT_EQ(value, "aaa");

  s = txn->Commit();
  ASSERT_OK(s);
  delete txn;

  txn = txn_db->BeginTransaction(write_options);
  txn->SetSnapshot();
  s = txn->GetForUpdate(read_options, "aaa", &value);
  ASSERT_EQ(value, "aaa");

  s = txn_db->Put(write_options, "aaa", "xxx");
  s = txn->Commit();
  ASSERT_TRUE(s.IsBusy());
  delete txn;
}

TEST_P(OptimisticTransactionTest, PredicateManyPreceders) {
  WriteOptions write_options;
  ReadOptions read_options1, read_options2;
  OptimisticTransactionOptions txn_options;
  string value;
  Status s;

  txn_options.set_snapshot = true;
  Transaction* txn1 = txn_db->BeginTransaction(write_options, txn_options);
  read_options1.snapshot = txn1->GetSnapshot();

  Transaction* txn2 = txn_db->BeginTransaction(write_options);
  txn2->SetSnapshot();
  read_options2.snapshot = txn2->GetSnapshot();

  std::vector<Slice> multiget_keys = {"1", "2", "3"};
  std::vector<std::string> multiget_values;

  std::vector<Status> results =
      txn1->MultiGetForUpdate(read_options1, multiget_keys, &multiget_values);
  ASSERT_TRUE(results[1].IsNotFound());

  txn2->Put("2", "x");

  s = txn2->Commit();
  ASSERT_OK(s);

  multiget_values.clear();
  results =
      txn1->MultiGetForUpdate(read_options1, multiget_keys, &multiget_values);
  ASSERT_TRUE(results[1].IsNotFound());

  // should not commit since txn2 wrote a key txn has read
  s = txn1->Commit();
  ASSERT_TRUE(s.IsBusy());

  delete txn1;
  delete txn2;

  txn1 = txn_db->BeginTransaction(write_options, txn_options);
  read_options1.snapshot = txn1->GetSnapshot();

  txn2 = txn_db->BeginTransaction(write_options, txn_options);
  read_options2.snapshot = txn2->GetSnapshot();

  txn1->Put("4", "x");

  txn2->Delete("4");

  // txn1 can commit since txn2's delete hasn't happened yet (it's just batched)
  s = txn1->Commit();
  ASSERT_OK(s);

  s = txn2->GetForUpdate(read_options2, "4", &value);
  ASSERT_TRUE(s.IsNotFound());

  // txn2 cannot commit since txn1 changed "4"
  s = txn2->Commit();
  ASSERT_TRUE(s.IsBusy());

  delete txn1;
  delete txn2;
}

TEST_P(OptimisticTransactionTest, LostUpdate) {
  WriteOptions write_options;
  ReadOptions read_options, read_options1, read_options2;
  OptimisticTransactionOptions txn_options;
  string value;
  Status s;

  // Test 2 transactions writing to the same key in multiple orders and
  // with/without snapshots

  Transaction* txn1 = txn_db->BeginTransaction(write_options);
  Transaction* txn2 = txn_db->BeginTransaction(write_options);

  txn1->Put("1", "1");
  txn2->Put("1", "2");

  s = txn1->Commit();
  ASSERT_OK(s);

  s = txn2->Commit();
  ASSERT_TRUE(s.IsBusy());

  delete txn1;
  delete txn2;

  txn_options.set_snapshot = true;
  txn1 = txn_db->BeginTransaction(write_options, txn_options);
  read_options1.snapshot = txn1->GetSnapshot();

  txn2 = txn_db->BeginTransaction(write_options, txn_options);
  read_options2.snapshot = txn2->GetSnapshot();

  txn1->Put("1", "3");
  txn2->Put("1", "4");

  s = txn1->Commit();
  ASSERT_OK(s);

  s = txn2->Commit();
  ASSERT_TRUE(s.IsBusy());

  delete txn1;
  delete txn2;

  txn1 = txn_db->BeginTransaction(write_options, txn_options);
  read_options1.snapshot = txn1->GetSnapshot();

  txn2 = txn_db->BeginTransaction(write_options, txn_options);
  read_options2.snapshot = txn2->GetSnapshot();

  txn1->Put("1", "5");
  s = txn1->Commit();
  ASSERT_OK(s);

  txn2->Put("1", "6");
  s = txn2->Commit();
  ASSERT_TRUE(s.IsBusy());

  delete txn1;
  delete txn2;

  txn1 = txn_db->BeginTransaction(write_options, txn_options);
  read_options1.snapshot = txn1->GetSnapshot();

  txn2 = txn_db->BeginTransaction(write_options, txn_options);
  read_options2.snapshot = txn2->GetSnapshot();

  txn1->Put("1", "5");
  s = txn1->Commit();
  ASSERT_OK(s);

  txn2->SetSnapshot();
  txn2->Put("1", "6");
  s = txn2->Commit();
  ASSERT_OK(s);

  delete txn1;
  delete txn2;

  txn1 = txn_db->BeginTransaction(write_options);
  txn2 = txn_db->BeginTransaction(write_options);

  txn1->Put("1", "7");
  s = txn1->Commit();
  ASSERT_OK(s);

  txn2->Put("1", "8");
  s = txn2->Commit();
  ASSERT_OK(s);

  delete txn1;
  delete txn2;

  s = txn_db->Get(read_options, "1", &value);
  ASSERT_OK(s);
  ASSERT_EQ(value, "8");
}

TEST_P(OptimisticTransactionTest, UntrackedWrites) {
  WriteOptions write_options;
  ReadOptions read_options;
  string value;
  Status s;

  // Verify transaction rollback works for untracked keys.
  Transaction* txn = txn_db->BeginTransaction(write_options);
  txn->PutUntracked("untracked", "0");
  txn->Rollback();
  s = txn_db->Get(read_options, "untracked", &value);
  ASSERT_TRUE(s.IsNotFound());

  delete txn;
  txn = txn_db->BeginTransaction(write_options);

  txn->Put("tracked", "1");
  txn->PutUntracked("untracked", "1");
  txn->MergeUntracked("untracked", "2");
  txn->DeleteUntracked("untracked");

  // Write to the untracked key outside of the transaction and verify
  // it doesn't prevent the transaction from committing.
  s = txn_db->Put(write_options, "untracked", "x");
  ASSERT_OK(s);

  s = txn->Commit();
  ASSERT_OK(s);

  s = txn_db->Get(read_options, "untracked", &value);
  ASSERT_TRUE(s.IsNotFound());

  delete txn;
  txn = txn_db->BeginTransaction(write_options);

  txn->Put("tracked", "10");
  txn->PutUntracked("untracked", "A");

  // Write to tracked key outside of the transaction and verify that the
  // untracked keys are not written when the commit fails.
  s = txn_db->Delete(write_options, "tracked");

  s = txn->Commit();
  ASSERT_TRUE(s.IsBusy());

  s = txn_db->Get(read_options, "untracked", &value);
  ASSERT_TRUE(s.IsNotFound());

  delete txn;
}

TEST_P(OptimisticTransactionTest, IteratorTest) {
  WriteOptions write_options;
  ReadOptions read_options, snapshot_read_options;
  OptimisticTransactionOptions txn_options;
  string value;
  Status s;

  // Write some keys to the db
  s = txn_db->Put(write_options, "A", "a");
  ASSERT_OK(s);

  s = txn_db->Put(write_options, "G", "g");
  ASSERT_OK(s);

  s = txn_db->Put(write_options, "F", "f");
  ASSERT_OK(s);

  s = txn_db->Put(write_options, "C", "c");
  ASSERT_OK(s);

  s = txn_db->Put(write_options, "D", "d");
  ASSERT_OK(s);

  Transaction* txn = txn_db->BeginTransaction(write_options);
  ASSERT_TRUE(txn);

  // Write some keys in a txn
  s = txn->Put("B", "b");
  ASSERT_OK(s);

  s = txn->Put("H", "h");
  ASSERT_OK(s);

  s = txn->Delete("D");
  ASSERT_OK(s);

  s = txn->Put("E", "e");
  ASSERT_OK(s);

  txn->SetSnapshot();
  const Snapshot* snapshot = txn->GetSnapshot();

  // Write some keys to the db after the snapshot
  s = txn_db->Put(write_options, "BB", "xx");
  ASSERT_OK(s);

  s = txn_db->Put(write_options, "C", "xx");
  ASSERT_OK(s);

  read_options.snapshot = snapshot;
  Iterator* iter = txn->GetIterator(read_options);
  ASSERT_OK(iter->status());
  iter->SeekToFirst();

  // Read all keys via iter and lock them all
  std::string results[] = {"a", "b", "c", "e", "f", "g", "h"};
  for (int i = 0; i < 7; i++) {
    ASSERT_OK(iter->status());
    ASSERT_TRUE(iter->Valid());
    ASSERT_EQ(results[i], iter->value().ToString());

    s = txn->GetForUpdate(read_options, iter->key(), nullptr);
    ASSERT_OK(s);

    iter->Next();
  }
  ASSERT_FALSE(iter->Valid());

  iter->Seek("G");
  ASSERT_OK(iter->status());
  ASSERT_TRUE(iter->Valid());
  ASSERT_EQ("g", iter->value().ToString());

  iter->Prev();
  ASSERT_OK(iter->status());
  ASSERT_TRUE(iter->Valid());
  ASSERT_EQ("f", iter->value().ToString());

  iter->Seek("D");
  ASSERT_OK(iter->status());
  ASSERT_TRUE(iter->Valid());
  ASSERT_EQ("e", iter->value().ToString());

  iter->Seek("C");
  ASSERT_OK(iter->status());
  ASSERT_TRUE(iter->Valid());
  ASSERT_EQ("c", iter->value().ToString());

  iter->Next();
  ASSERT_OK(iter->status());
  ASSERT_TRUE(iter->Valid());
  ASSERT_EQ("e", iter->value().ToString());

  iter->Seek("");
  ASSERT_OK(iter->status());
  ASSERT_TRUE(iter->Valid());
  ASSERT_EQ("a", iter->value().ToString());

  iter->Seek("X");
  ASSERT_OK(iter->status());
  ASSERT_FALSE(iter->Valid());

  iter->SeekToLast();
  ASSERT_OK(iter->status());
  ASSERT_TRUE(iter->Valid());
  ASSERT_EQ("h", iter->value().ToString());

  // key "C" was modified in the db after txn's snapshot.  txn will not commit.
  s = txn->Commit();
  ASSERT_TRUE(s.IsBusy());

  delete iter;
  delete txn;
}

TEST_P(OptimisticTransactionTest, SavepointTest) {
  WriteOptions write_options;
  ReadOptions read_options, snapshot_read_options;
  OptimisticTransactionOptions txn_options;
  string value;
  Status s;

  Transaction* txn = txn_db->BeginTransaction(write_options);
  ASSERT_TRUE(txn);

  s = txn->RollbackToSavePoint();
  ASSERT_TRUE(s.IsNotFound());

  txn->SetSavePoint();  // 1

  ASSERT_OK(txn->RollbackToSavePoint());  // Rollback to beginning of txn
  s = txn->RollbackToSavePoint();
  ASSERT_TRUE(s.IsNotFound());

  s = txn->Put("B", "b");
  ASSERT_OK(s);

  s = txn->Commit();
  ASSERT_OK(s);

  s = txn_db->Get(read_options, "B", &value);
  ASSERT_OK(s);
  ASSERT_EQ("b", value);

  delete txn;
  txn = txn_db->BeginTransaction(write_options);
  ASSERT_TRUE(txn);

  s = txn->Put("A", "a");
  ASSERT_OK(s);

  s = txn->Put("B", "bb");
  ASSERT_OK(s);

  s = txn->Put("C", "c");
  ASSERT_OK(s);

  txn->SetSavePoint();  // 2

  s = txn->Delete("B");
  ASSERT_OK(s);

  s = txn->Put("C", "cc");
  ASSERT_OK(s);

  s = txn->Put("D", "d");
  ASSERT_OK(s);

  ASSERT_OK(txn->RollbackToSavePoint());  // Rollback to 2

  s = txn->Get(read_options, "A", &value);
  ASSERT_OK(s);
  ASSERT_EQ("a", value);

  s = txn->Get(read_options, "B", &value);
  ASSERT_OK(s);
  ASSERT_EQ("bb", value);

  s = txn->Get(read_options, "C", &value);
  ASSERT_OK(s);
  ASSERT_EQ("c", value);

  s = txn->Get(read_options, "D", &value);
  ASSERT_TRUE(s.IsNotFound());

  s = txn->Put("A", "a");
  ASSERT_OK(s);

  s = txn->Put("E", "e");
  ASSERT_OK(s);

  // Rollback to beginning of txn
  s = txn->RollbackToSavePoint();
  ASSERT_TRUE(s.IsNotFound());
  txn->Rollback();

  s = txn->Get(read_options, "A", &value);
  ASSERT_TRUE(s.IsNotFound());

  s = txn->Get(read_options, "B", &value);
  ASSERT_OK(s);
  ASSERT_EQ("b", value);

  s = txn->Get(read_options, "D", &value);
  ASSERT_TRUE(s.IsNotFound());

  s = txn->Get(read_options, "D", &value);
  ASSERT_TRUE(s.IsNotFound());

  s = txn->Get(read_options, "E", &value);
  ASSERT_TRUE(s.IsNotFound());

  s = txn->Put("A", "aa");
  ASSERT_OK(s);

  s = txn->Put("F", "f");
  ASSERT_OK(s);

  txn->SetSavePoint();  // 3
  txn->SetSavePoint();  // 4

  s = txn->Put("G", "g");
  ASSERT_OK(s);

  s = txn->Delete("F");
  ASSERT_OK(s);

  s = txn->Delete("B");
  ASSERT_OK(s);

  s = txn->Get(read_options, "A", &value);
  ASSERT_OK(s);
  ASSERT_EQ("aa", value);

  s = txn->Get(read_options, "F", &value);
  ASSERT_TRUE(s.IsNotFound());

  s = txn->Get(read_options, "B", &value);
  ASSERT_TRUE(s.IsNotFound());

  ASSERT_OK(txn->RollbackToSavePoint());  // Rollback to 3

  s = txn->Get(read_options, "F", &value);
  ASSERT_OK(s);
  ASSERT_EQ("f", value);

  s = txn->Get(read_options, "G", &value);
  ASSERT_TRUE(s.IsNotFound());

  s = txn->Commit();
  ASSERT_OK(s);

  s = txn_db->Get(read_options, "F", &value);
  ASSERT_OK(s);
  ASSERT_EQ("f", value);

  s = txn_db->Get(read_options, "G", &value);
  ASSERT_TRUE(s.IsNotFound());

  s = txn_db->Get(read_options, "A", &value);
  ASSERT_OK(s);
  ASSERT_EQ("aa", value);

  s = txn_db->Get(read_options, "B", &value);
  ASSERT_OK(s);
  ASSERT_EQ("b", value);

  s = txn_db->Get(read_options, "C", &value);
  ASSERT_TRUE(s.IsNotFound());

  s = txn_db->Get(read_options, "D", &value);
  ASSERT_TRUE(s.IsNotFound());

  s = txn_db->Get(read_options, "E", &value);
  ASSERT_TRUE(s.IsNotFound());

  delete txn;
}

TEST_P(OptimisticTransactionTest, UndoGetForUpdateTest) {
  WriteOptions write_options;
  ReadOptions read_options, snapshot_read_options;
  OptimisticTransactionOptions txn_options;
  string value;
  Status s;

  txn_db->Put(write_options, "A", "");

  Transaction* txn1 = txn_db->BeginTransaction(write_options);
  ASSERT_TRUE(txn1);

  s = txn1->GetForUpdate(read_options, "A", &value);
  ASSERT_OK(s);

  txn1->UndoGetForUpdate("A");

  Transaction* txn2 = txn_db->BeginTransaction(write_options);
  txn2->Put("A", "x");
  s = txn2->Commit();
  ASSERT_OK(s);
  delete txn2;

  // Verify that txn1 can commit since A isn't conflict checked
  s = txn1->Commit();
  ASSERT_OK(s);
  delete txn1;

  txn1 = txn_db->BeginTransaction(write_options);
  txn1->Put("A", "a");

  s = txn1->GetForUpdate(read_options, "A", &value);
  ASSERT_OK(s);

  txn1->UndoGetForUpdate("A");

  txn2 = txn_db->BeginTransaction(write_options);
  txn2->Put("A", "x");
  s = txn2->Commit();
  ASSERT_OK(s);
  delete txn2;

  // Verify that txn1 cannot commit since A will still be conflict checked
  s = txn1->Commit();
  ASSERT_TRUE(s.IsBusy());
  delete txn1;

  txn1 = txn_db->BeginTransaction(write_options);

  s = txn1->GetForUpdate(read_options, "A", &value);
  ASSERT_OK(s);
  s = txn1->GetForUpdate(read_options, "A", &value);
  ASSERT_OK(s);

  txn1->UndoGetForUpdate("A");

  txn2 = txn_db->BeginTransaction(write_options);
  txn2->Put("A", "x");
  s = txn2->Commit();
  ASSERT_OK(s);
  delete txn2;

  // Verify that txn1 cannot commit since A will still be conflict checked
  s = txn1->Commit();
  ASSERT_TRUE(s.IsBusy());
  delete txn1;

  txn1 = txn_db->BeginTransaction(write_options);

  s = txn1->GetForUpdate(read_options, "A", &value);
  ASSERT_OK(s);
  s = txn1->GetForUpdate(read_options, "A", &value);
  ASSERT_OK(s);

  txn1->UndoGetForUpdate("A");
  txn1->UndoGetForUpdate("A");

  txn2 = txn_db->BeginTransaction(write_options);
  txn2->Put("A", "x");
  s = txn2->Commit();
  ASSERT_OK(s);
  delete txn2;

  // Verify that txn1 can commit since A isn't conflict checked
  s = txn1->Commit();
  ASSERT_OK(s);
  delete txn1;

  txn1 = txn_db->BeginTransaction(write_options);

  s = txn1->GetForUpdate(read_options, "A", &value);
  ASSERT_OK(s);

  txn1->SetSavePoint();
  txn1->UndoGetForUpdate("A");

  txn2 = txn_db->BeginTransaction(write_options);
  txn2->Put("A", "x");
  s = txn2->Commit();
  ASSERT_OK(s);
  delete txn2;

  // Verify that txn1 cannot commit since A will still be conflict checked
  s = txn1->Commit();
  ASSERT_TRUE(s.IsBusy());
  delete txn1;

  txn1 = txn_db->BeginTransaction(write_options);

  s = txn1->GetForUpdate(read_options, "A", &value);
  ASSERT_OK(s);

  txn1->SetSavePoint();
  s = txn1->GetForUpdate(read_options, "A", &value);
  ASSERT_OK(s);
  txn1->UndoGetForUpdate("A");

  txn2 = txn_db->BeginTransaction(write_options);
  txn2->Put("A", "x");
  s = txn2->Commit();
  ASSERT_OK(s);
  delete txn2;

  // Verify that txn1 cannot commit since A will still be conflict checked
  s = txn1->Commit();
  ASSERT_TRUE(s.IsBusy());
  delete txn1;

  txn1 = txn_db->BeginTransaction(write_options);

  s = txn1->GetForUpdate(read_options, "A", &value);
  ASSERT_OK(s);

  txn1->SetSavePoint();
  s = txn1->GetForUpdate(read_options, "A", &value);
  ASSERT_OK(s);
  txn1->UndoGetForUpdate("A");

  txn1->RollbackToSavePoint();
  txn1->UndoGetForUpdate("A");

  txn2 = txn_db->BeginTransaction(write_options);
  txn2->Put("A", "x");
  s = txn2->Commit();
  ASSERT_OK(s);
  delete txn2;

  // Verify that txn1 can commit since A isn't conflict checked
  s = txn1->Commit();
  ASSERT_OK(s);
  delete txn1;
}

namespace {
Status OptimisticTransactionStressTestInserter(OptimisticTransactionDB* db,
                                               const size_t num_transactions,
                                               const size_t num_sets,
                                               const size_t num_keys_per_set) {
  size_t seed = std::hash<std::thread::id>()(std::this_thread::get_id());
  Random64 _rand(seed);
  WriteOptions write_options;
  ReadOptions read_options;
  OptimisticTransactionOptions txn_options;
  txn_options.set_snapshot = true;

  RandomTransactionInserter inserter(&_rand, write_options, read_options,
                                     num_keys_per_set,
                                     static_cast<uint16_t>(num_sets));

  for (size_t t = 0; t < num_transactions; t++) {
    bool success = inserter.OptimisticTransactionDBInsert(db, txn_options);
    if (!success) {
      // unexpected failure
      return inserter.GetLastStatus();
    }
  }

  // Make sure at least some of the transactions succeeded.  It's ok if
  // some failed due to write-conflicts.
  if (inserter.GetFailureCount() > num_transactions / 2) {
    return Status::TryAgain("Too many transactions failed! " +
                            std::to_string(inserter.GetFailureCount()) + " / " +
                            std::to_string(num_transactions));
  }

  return Status::OK();
}
}  // namespace

TEST_P(OptimisticTransactionTest, OptimisticTransactionStressTest) {
  const size_t num_threads = 4;
  const size_t num_transactions_per_thread = 10000;
  const size_t num_sets = 3;
  const size_t num_keys_per_set = 100;
  // Setting the key-space to be 100 keys should cause enough write-conflicts
  // to make this test interesting.

  std::vector<port::Thread> threads;

  std::function<void()> call_inserter = [&] {
    ASSERT_OK(OptimisticTransactionStressTestInserter(
        txn_db, num_transactions_per_thread, num_sets, num_keys_per_set));
  };

  // Create N threads that use RandomTransactionInserter to write
  // many transactions.
  for (uint32_t i = 0; i < num_threads; i++) {
    threads.emplace_back(call_inserter);
  }

  // Wait for all threads to run
  for (auto& t : threads) {
    t.join();
  }

  // Verify that data is consistent
  Status s = RandomTransactionInserter::Verify(txn_db, num_sets);
  ASSERT_OK(s);
}

TEST_P(OptimisticTransactionTest, SequenceNumberAfterRecoverTest) {
  WriteOptions write_options;
  OptimisticTransactionOptions transaction_options;

  Transaction* transaction(txn_db->BeginTransaction(write_options, transaction_options));
  Status s = transaction->Put("foo", "val");
  ASSERT_OK(s);
  s = transaction->Put("foo2", "val");
  ASSERT_OK(s);
  s = transaction->Put("foo3", "val");
  ASSERT_OK(s);
  s = transaction->Commit();
  ASSERT_OK(s);
  delete transaction;

  Reopen();
  transaction = txn_db->BeginTransaction(write_options, transaction_options);
  s = transaction->Put("bar", "val");
  ASSERT_OK(s);
  s = transaction->Put("bar2", "val");
  ASSERT_OK(s);
  s = transaction->Commit();
  ASSERT_OK(s);

  delete transaction;
}

INSTANTIATE_TEST_CASE_P(
    InstanceOccGroup, OptimisticTransactionTest,
    testing::Values(OccValidationPolicy::kValidateSerial,
                    OccValidationPolicy::kValidateParallel));

}  // namespace ROCKSDB_NAMESPACE

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#else
#include <stdio.h>

int main(int /*argc*/, char** /*argv*/) {
  fprintf(
      stderr,
      "SKIPPED as optimistic_transaction is not supported in ROCKSDB_LITE\n");
  return 0;
}

#endif  // !ROCKSDB_LITE
