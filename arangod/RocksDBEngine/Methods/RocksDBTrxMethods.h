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

#include <memory>
#include <utility>
#include <vector>

#include "RocksDBEngine/Methods/RocksDBTrxBaseMethods.h"

namespace arangodb {

/// transaction wrapper, uses the current rocksdb transaction
class RocksDBTrxMethods : public RocksDBTrxBaseMethods {
 public:
  explicit RocksDBTrxMethods(RocksDBTransactionState*,
                             rocksdb::TransactionDB* db);

  ~RocksDBTrxMethods();

  Result beginTransaction() override;

  rocksdb::ReadOptions iteratorReadOptions() const override;

  void prepareOperation(DataSourceId cid, RevisionId rid,
                        TRI_voc_document_operation_e operationType) override;

  /// @brief undo the effects of the previous prepareOperation call
  void rollbackOperation(TRI_voc_document_operation_e operationType) override;

  /// @brief performs an intermediate commit if necessary
  /// sets hasPerformedIntermediateCommit to true if an intermediate commit was
  /// performed
  Result checkIntermediateCommit(bool& hasPerformedIntermediateCommit) override;

  rocksdb::Status Get(rocksdb::ColumnFamilyHandle*, rocksdb::Slice const&,
                      rocksdb::PinnableSlice*, ReadOwnWrites) override;

  std::unique_ptr<rocksdb::Iterator> NewIterator(rocksdb::ColumnFamilyHandle*,
                                                 ReadOptionsCallback) override;

  bool iteratorMustCheckBounds(ReadOwnWrites readOwnWrites) const override;

  void beginQuery(bool isModificationQuery);
  void endQuery(bool isModificationQuery) noexcept;

 private:
  friend class RocksDBStreamingTrxMethods;

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
  Result checkIntermediateCommit(uint64_t newSize,
                                 bool& hasPerformedIntermediateCommit);

  void initializeReadWriteBatch();
  void releaseReadWriteBatch() noexcept;

  /// @brief used for read-only trx and intermediate commits
  /// For intermediate commits this MUST ONLY be used for iterators
  rocksdb::Snapshot const* _iteratorReadSnapshot{nullptr};

  /// @brief this WriteBatch can be used to satisfy read operations in a
  /// streaming trx. _readWriteBatch can have three different states:
  ///   - nullptr
  ///   - pointing to a copy (_ownsReadWriteBatch == true)
  ///   - pointing to _rockTransaction's underlying WriteBatch
  ///   (_ownsReadWriteBatch == false)
  ///
  /// If _readWriteBatch is null, read operations without read-own-writes
  /// semantic are performed directly on the DB using the snapshot, otherwise on
  /// _rocksTransaction. When a modification query is started, the current
  /// WriteBatch from _rocksTransaction is copied and stored in _readWriteBatch.
  /// Once the modification query is finished, the copy is released and
  /// _readWriteBatch is set to point to the trx's underlying WriteBatch. This
  /// is necessary to ensure that subsequent read operations within the same
  /// streaming transaction see the previously performed writes. If
  /// _readWriteBatch is not null, read-operations without read-own-writes
  /// semantic are performed on _readWriteBatch, otherwise on _rocksTransaction.
  ///
  /// If the transaction is globally managed (e.g., a streaming trx), we already
  /// set _readWriteBatch to point to the trx's underlying WriteBatch at the
  /// begin of the transaction. This is necessary to ensure that read operations
  /// performed as part of the trx observe the correct transaction state,
  /// regardless of any AQL queries.
  rocksdb::WriteBatchWithIndex* _readWriteBatch{nullptr};
  bool _ownsReadWriteBatch{false};

  std::atomic<std::size_t> _numActiveReadOnlyQueries{0};
  std::atomic<bool> _hasActiveModificationQuery{false};
};

}  // namespace arangodb
