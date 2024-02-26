////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RocksDBEngine/RocksDBTransactionMethods.h"

namespace rocksdb {
class TransactionDB;
}  // namespace rocksdb

namespace arangodb {

class RocksDBReadOnlyBaseMethods : public RocksDBTransactionMethods {
 public:
  explicit RocksDBReadOnlyBaseMethods(RocksDBTransactionState* state,
                                      rocksdb::TransactionDB* db);

  ~RocksDBReadOnlyBaseMethods() override;

  bool ensureSnapshot() override;

  rocksdb::SequenceNumber GetSequenceNumber() const noexcept override;

  TRI_voc_tick_t lastOperationTick() const noexcept override { return 0; }

  uint64_t numCommits() const noexcept override { return 0; }

  uint64_t numIntermediateCommits() const noexcept override { return 0; }

  bool hasOperations() const noexcept override { return false; }

  uint64_t numOperations() const noexcept override { return 0; }

  uint64_t numPrimitiveOperations() const noexcept override { return 0; }

  void prepareOperation(DataSourceId cid, RevisionId rid,
                        TRI_voc_document_operation_e operationType) override;

  void rollbackOperation(TRI_voc_document_operation_e operationType) override;

  Result addOperation(TRI_voc_document_operation_e opType) override;

  rocksdb::Status GetForUpdate(rocksdb::ColumnFamilyHandle*,
                               rocksdb::Slice const&,
                               rocksdb::PinnableSlice*) override;
  rocksdb::Status Put(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key,
                      rocksdb::Slice const& val, bool assume_tracked) override;
  rocksdb::Status PutUntracked(rocksdb::ColumnFamilyHandle*,
                               RocksDBKey const& key,
                               rocksdb::Slice const& val) override;
  rocksdb::Status Delete(rocksdb::ColumnFamilyHandle*,
                         RocksDBKey const& key) override;
  rocksdb::Status SingleDelete(rocksdb::ColumnFamilyHandle*,
                               RocksDBKey const&) override;

  rocksdb::Status SingleGet(rocksdb::Snapshot const* snapshot,
                            rocksdb::ColumnFamilyHandle& family,
                            rocksdb::Slice const& key,
                            rocksdb::PinnableSlice& value) final;
  void MultiGet(rocksdb::Snapshot const* snapshot,
                rocksdb::ColumnFamilyHandle& family, size_t count,
                rocksdb::Slice const* keys, rocksdb::PinnableSlice* values,
                rocksdb::Status* statuses) final;
  void MultiGet(rocksdb::ColumnFamilyHandle& family, size_t count,
                rocksdb::Slice const* keys, rocksdb::PinnableSlice* values,
                rocksdb::Status* statuses, ReadOwnWrites) final;

  void PutLogData(rocksdb::Slice const&) override;

  void SetSavePoint() override {}
  rocksdb::Status RollbackToSavePoint() override {
    return rocksdb::Status::OK();
  }
  rocksdb::Status RollbackToWriteBatchSavePoint() override {
    // simply relay to the general method (which in this derived class does
    // nothing)
    return RollbackToSavePoint();
  }
  void PopSavePoint() override {}

  bool iteratorMustCheckBounds(ReadOwnWrites) const override {
    // we never have to check the bounds for read-only iterators
    return false;
  }

 protected:
  void releaseSnapshot();

  rocksdb::TransactionDB* _db;

  ReadOptions _readOptions;
};

}  // namespace arangodb
