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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RocksDBEngine/Methods/RocksDBTrxBaseMethods.h"

namespace arangodb {
  
/// transaction wrapper, uses the current rocksdb transaction
class RocksDBTrxMethods : public RocksDBTrxBaseMethods {
 public:
  explicit RocksDBTrxMethods(RocksDBTransactionState*, rocksdb::TransactionDB* db);
  
  ~RocksDBTrxMethods();

  Result beginTransaction() override;

  rocksdb::ReadOptions iteratorReadOptions() const override;
  
  void prepareOperation(DataSourceId cid, RevisionId rid, TRI_voc_document_operation_e operationType) override;

  /// @brief undo the effects of the previous prepareOperation call
  void rollbackOperation(TRI_voc_document_operation_e operationType) override;

  /// @brief performs an intermediate commit if necessary
  /// sets hasPerformedIntermediateCommit to true if an intermediate commit was
  /// performed
  Result checkIntermediateCommit(bool& hasPerformedIntermediateCommit) override;

  std::unique_ptr<rocksdb::Iterator> NewIterator(rocksdb::ColumnFamilyHandle*,
                                                 ReadOptionsCallback) override;
 private:
  bool hasIntermediateCommitsEnabled() const noexcept;
  
  void cleanupTransaction() override;
  
  void createTransaction() override;

  /// @brief Trigger an intermediate commit.
  /// Handle with care if failing after this commit it will only
  /// be rolled back until this point of time.
  /// sets hasPerformedIntermediateCommit to true if an intermediate commit was
  /// performed Not thread safe
  Result triggerIntermediateCommit(bool& hasPerformedIntermediateCommit);

  /// @brief check sizes and call internalCommit if too big
  /// sets hasPerformedIntermediateCommit to true if an intermediate commit was
  /// performed
  Result checkIntermediateCommit(uint64_t newSize, bool& hasPerformedIntermediateCommit);

  /// @brief used for read-only trx and intermediate commits
  /// For intermediate commits this MUST ONLY be used for iterators
  rocksdb::Snapshot const* _iteratorReadSnapshot{nullptr};
};

}  // namespace arangodb

