////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ROCKSDB_METHODS_H
#define ARANGOD_ROCKSDB_ROCKSDB_METHODS_H 1

#include "Basics/Result.h"
#include "RocksDBColumnFamily.h"
#include "RocksDBCommon.h"

namespace rocksdb {
class Transaction;
class Slice;
class Iterator;
class TransactionDB;
class WriteBatch;
class WriteBatchWithIndex;
class Comparator;
struct ReadOptions;
}  // namespace rocksdb

namespace arangodb {
namespace transaction {
class Methods;
}

class RocksDBKey;
class RocksDBMethods;
class RocksDBTransactionState;

class RocksDBSavePoint {
 public:
  RocksDBSavePoint(transaction::Methods* trx, TRI_voc_document_operation_e operationType);
  ~RocksDBSavePoint();

  /// @brief acknowledges the current savepoint, so there
  /// will be no rollback when the destructor is called
  /// if an intermediate commit was performed, pass a value of
  /// true, false otherwise
  void finish(bool hasPerformedIntermediateCommit);

 private:
  void rollback();

 private:
  transaction::Methods* _trx;
  TRI_voc_document_operation_e const _operationType;
  bool _handled;
};

class RocksDBMethods {
 public:
  explicit RocksDBMethods(RocksDBTransactionState* state) : _state(state) {}
  virtual ~RocksDBMethods() = default;

  /// @brief read options for use with iterators
  rocksdb::ReadOptions iteratorReadOptions();

  virtual bool isIndexingDisabled() const { return false; }

  /// @brief returns true if indexing was disabled by this call
  /// the default implementation is to do nothing
  virtual bool DisableIndexing() { return false; }

  // the default implementation is to do nothing
  virtual bool EnableIndexing() { return false; }

  virtual rocksdb::Status Get(rocksdb::ColumnFamilyHandle*,
                              rocksdb::Slice const&, rocksdb::PinnableSlice*) = 0;
  virtual rocksdb::Status GetForUpdate(rocksdb::ColumnFamilyHandle*,
                                       rocksdb::Slice const&,
                                       rocksdb::PinnableSlice*) = 0;
  /// assume_tracked=true will assume you used GetForUpdate on this key earlier.
  /// it will still verify this, so it is slower than PutUntracked
  virtual rocksdb::Status Put(rocksdb::ColumnFamilyHandle*, RocksDBKey const&,
                              rocksdb::Slice const&, bool assume_tracked) = 0;
  /// Like Put, but will not perform any write-write conflict checks
  virtual rocksdb::Status PutUntracked(rocksdb::ColumnFamilyHandle*, RocksDBKey const&,
                                       rocksdb::Slice const&) = 0;

  virtual rocksdb::Status Delete(rocksdb::ColumnFamilyHandle*, RocksDBKey const&) = 0;
  /// contrary to Delete, a SingleDelete may only be used
  /// when keys are inserted exactly once (and never overwritten)
  virtual rocksdb::Status SingleDelete(rocksdb::ColumnFamilyHandle*, RocksDBKey const&) = 0;
  
  virtual void PutLogData(rocksdb::Slice const&) = 0;

  virtual std::unique_ptr<rocksdb::Iterator> NewIterator(rocksdb::ReadOptions const&,
                                                         rocksdb::ColumnFamilyHandle*) = 0;

  virtual void SetSavePoint() = 0;
  virtual rocksdb::Status RollbackToSavePoint() = 0;
  virtual void PopSavePoint() = 0;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  std::size_t countInBounds(RocksDBKeyBounds const& bounds, bool isElementInRange = false);
#endif

 protected:
  RocksDBTransactionState* _state;
};

// only implements GET and NewIterator
class RocksDBReadOnlyMethods final : public RocksDBMethods {
 public:
  explicit RocksDBReadOnlyMethods(RocksDBTransactionState* state);

  rocksdb::Status Get(rocksdb::ColumnFamilyHandle*, rocksdb::Slice const& key,
                      rocksdb::PinnableSlice* val) override;
  rocksdb::Status GetForUpdate(rocksdb::ColumnFamilyHandle*,
                               rocksdb::Slice const&,
                               rocksdb::PinnableSlice*) override;
  rocksdb::Status Put(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key,
                      rocksdb::Slice const& val, bool assume_tracked) override;
  rocksdb::Status PutUntracked(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key,
                               rocksdb::Slice const& val) override;
  rocksdb::Status Delete(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key) override;
  rocksdb::Status SingleDelete(rocksdb::ColumnFamilyHandle*, RocksDBKey const&) override;
  void PutLogData(rocksdb::Slice const&) override;

  std::unique_ptr<rocksdb::Iterator> NewIterator(rocksdb::ReadOptions const&,
                                                 rocksdb::ColumnFamilyHandle*) override;

  void SetSavePoint() override {}
  rocksdb::Status RollbackToSavePoint() override {
    return rocksdb::Status::OK();
  }
  void PopSavePoint() override {}

 private:
  rocksdb::TransactionDB* _db;
};

/// transaction wrapper, uses the current rocksdb transaction
class RocksDBTrxMethods : public RocksDBMethods {
 public:
  explicit RocksDBTrxMethods(RocksDBTransactionState* state);

  virtual bool isIndexingDisabled() const override{ return _indexingDisabled; }

  /// @brief returns true if indexing was disabled by this call
  bool DisableIndexing() override;

  bool EnableIndexing() override;

  rocksdb::Status Get(rocksdb::ColumnFamilyHandle*, rocksdb::Slice const& key,
                      rocksdb::PinnableSlice* val) override;
  rocksdb::Status GetForUpdate(rocksdb::ColumnFamilyHandle*,
                               rocksdb::Slice const&,
                               rocksdb::PinnableSlice*) override;
  rocksdb::Status Put(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key,
                      rocksdb::Slice const& val, bool assume_tracked) override;
  rocksdb::Status PutUntracked(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key,
                               rocksdb::Slice const& val) override;
  rocksdb::Status Delete(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key) override;
  rocksdb::Status SingleDelete(rocksdb::ColumnFamilyHandle*, RocksDBKey const&) override;
  void PutLogData(rocksdb::Slice const&) override;

  std::unique_ptr<rocksdb::Iterator> NewIterator(rocksdb::ReadOptions const&,
                                                 rocksdb::ColumnFamilyHandle*) override;

  void SetSavePoint() override;
  rocksdb::Status RollbackToSavePoint() override;
  void PopSavePoint() override;

  bool _indexingDisabled;
};


/// wraps a writebatch - non transactional
class RocksDBBatchedMethods final : public RocksDBMethods {
 public:
  RocksDBBatchedMethods(RocksDBTransactionState*, rocksdb::WriteBatch*);

  rocksdb::Status Get(rocksdb::ColumnFamilyHandle*, rocksdb::Slice const& key,
                      rocksdb::PinnableSlice* val) override;
  rocksdb::Status GetForUpdate(rocksdb::ColumnFamilyHandle*,
                               rocksdb::Slice const&,
                               rocksdb::PinnableSlice*) override;
  rocksdb::Status Put(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key,
                      rocksdb::Slice const& val, bool assume_tracked) override;
  rocksdb::Status PutUntracked(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key,
                               rocksdb::Slice const& val) override;
  rocksdb::Status Delete(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key) override;
  rocksdb::Status SingleDelete(rocksdb::ColumnFamilyHandle*, RocksDBKey const&) override;
  void PutLogData(rocksdb::Slice const&) override;

  std::unique_ptr<rocksdb::Iterator> NewIterator(rocksdb::ReadOptions const&,
                                                 rocksdb::ColumnFamilyHandle*) override;

  void SetSavePoint() override {}
  rocksdb::Status RollbackToSavePoint() override {
    return rocksdb::Status::OK();
  }
  void PopSavePoint() override {}

 private:
  rocksdb::WriteBatch* _wb;
};

/// wraps a writebatch with index - non transactional
class RocksDBBatchedWithIndexMethods final : public RocksDBMethods {
 public:
  RocksDBBatchedWithIndexMethods(RocksDBTransactionState*, rocksdb::WriteBatchWithIndex*);

  rocksdb::Status Get(rocksdb::ColumnFamilyHandle*, rocksdb::Slice const& key,
                      rocksdb::PinnableSlice* val) override;
  rocksdb::Status GetForUpdate(rocksdb::ColumnFamilyHandle*,
                               rocksdb::Slice const&,
                               rocksdb::PinnableSlice*) override;
  rocksdb::Status Put(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key,
                      rocksdb::Slice const& val, bool assume_tracked) override;
  rocksdb::Status PutUntracked(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key,
                               rocksdb::Slice const& val) override;
  rocksdb::Status Delete(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key) override;
  rocksdb::Status SingleDelete(rocksdb::ColumnFamilyHandle*, RocksDBKey const&) override;
  void PutLogData(rocksdb::Slice const&) override;

  std::unique_ptr<rocksdb::Iterator> NewIterator(rocksdb::ReadOptions const&,
                                                 rocksdb::ColumnFamilyHandle*) override;

  void SetSavePoint() override {}
  rocksdb::Status RollbackToSavePoint() override {
    return rocksdb::Status::OK();
  }
  void PopSavePoint() override {}

 private:
  rocksdb::TransactionDB* _db;
  rocksdb::WriteBatchWithIndex* _wb;
};

// INDEXING MAY ONLY BE DISABLED IN TOPLEVEL AQL TRANSACTIONS
// THIS IS BECAUSE THESE TRANSACTIONS WILL EITHER READ FROM
// OR (XOR) WRITE TO A COLLECTION. IF THIS PRECONDITION IS
// VIOLATED THE DISABLED INDEXING WILL BREAK GET OPERATIONS.
struct IndexingDisabler {
  // will only be active if condition is true

  IndexingDisabler() = delete;
  IndexingDisabler(IndexingDisabler&&) = delete;
  IndexingDisabler(IndexingDisabler const&) = delete;
  IndexingDisabler& operator=(IndexingDisabler const&) = delete;
  IndexingDisabler& operator=(IndexingDisabler&&) = delete;

  IndexingDisabler(RocksDBMethods* meth, bool condition) : _meth(nullptr) {
    if (condition) {
      bool disabledHere = meth->DisableIndexing();
      if (disabledHere) {
        _meth = meth;
      }
    }
  }

  ~IndexingDisabler() {
    if (_meth) {
      _meth->EnableIndexing();
    }
  }

 private:
  RocksDBMethods* _meth;
};

// if only single indices should be enabled during operations
struct IndexingEnabler {
  // will only be active if condition is true

  IndexingEnabler() = delete;
  IndexingEnabler(IndexingEnabler&&) = delete;
  IndexingEnabler(IndexingEnabler const&) = delete;
  IndexingEnabler& operator=(IndexingEnabler const&) = delete;
  IndexingEnabler& operator=(IndexingEnabler&&) = delete;

  IndexingEnabler(RocksDBMethods* meth, bool condition) : _meth(nullptr) {
    if (condition) {
      bool enableHere = meth->EnableIndexing();
      if (enableHere) {
        _meth = meth;
      }
    }
  }

  ~IndexingEnabler() {
    if (_meth) {
      _meth->DisableIndexing();
    }
  }

 private:
  RocksDBMethods* _meth;
};

}  // namespace arangodb

#endif
