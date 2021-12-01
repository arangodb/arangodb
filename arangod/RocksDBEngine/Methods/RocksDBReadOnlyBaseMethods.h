////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
  using RocksDBTransactionMethods::RocksDBTransactionMethods;

  TRI_voc_tick_t lastOperationTick() const noexcept override { return 0; }
  
  uint64_t numCommits() const noexcept override { return 0; }
  
  bool hasOperations() const noexcept override { return false; }
  
  uint64_t numOperations() const noexcept override { return 0; }
  
  void prepareOperation(DataSourceId cid, RevisionId rid, TRI_voc_document_operation_e operationType) override;

  void rollbackOperation(TRI_voc_document_operation_e operationType) override;

  Result addOperation(TRI_voc_document_operation_e opType) override;

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

  void SetSavePoint() override {}
  rocksdb::Status RollbackToSavePoint() override {
    return rocksdb::Status::OK();
  }
  rocksdb::Status RollbackToWriteBatchSavePoint() override {
    // simply relay to the general method (which in this derived class does nothing)
    return RollbackToSavePoint();
  }
  void PopSavePoint() override {}
  
  bool iteratorMustCheckBounds(ReadOwnWrites) const override {
    // we never have to check the bounds for read-only iterators
    return false;
  }
};

}  // namespace arangodb

