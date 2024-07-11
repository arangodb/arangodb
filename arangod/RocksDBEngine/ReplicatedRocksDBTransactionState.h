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

#include "RocksDBEngine/RocksDBTransactionState.h"

namespace arangodb {
class RocksDBTransactionMethods;

class ReplicatedRocksDBTransactionState final : public RocksDBTransactionState {
 public:
  ReplicatedRocksDBTransactionState(
      TRI_vocbase_t& vocbase, TransactionId tid,
      transaction::Options const& options,
      transaction::OperationOrigin operationOrigin);

  ~ReplicatedRocksDBTransactionState() override;

  /// @brief begin a transaction
  futures::Future<Result> beginTransaction(transaction::Hints hints) override;

  RocksDBTransactionMethods* rocksdbMethods(
      DataSourceId collectionId) const override;

  void beginQuery(std::shared_ptr<ResourceMonitor> resourceMonitor,
                  bool isModificationQuery) override;
  void endQuery(bool isModificationQuery) noexcept override;

  /// @returns tick of last operation in a transaction
  /// @note the value is guaranteed to be valid only after
  ///       transaction is committed
  TRI_voc_tick_t lastOperationTick() const noexcept override;

  /// @brief number of commits, including intermediate commits
  uint64_t numCommits() const noexcept override;

  uint64_t numIntermediateCommits() const noexcept override;

  void addIntermediateCommits(uint64_t value) override;

  arangodb::Result triggerIntermediateCommit() override;

  [[nodiscard]] futures::Future<Result> performIntermediateCommitIfRequired(
      DataSourceId collectionId) override;

  bool hasOperations() const noexcept override;

  uint64_t numOperations() const noexcept override;

  uint64_t numPrimitiveOperations() const noexcept override;

  bool ensureSnapshot() override;

  rocksdb::SequenceNumber beginSeq() const override;

  /// @brief returns a lock_guard for the internal commit lock.
  /// This lock is necessary to serialize the individual collection commits
  /// because each commit places a blocker for the current transaction id and
  /// we cannot have multiple blockers with the same id at the same time.
  std::lock_guard<std::mutex> lockCommit();

 protected:
  std::unique_ptr<TransactionCollection> createTransactionCollection(
      DataSourceId cid, AccessMode::Type accessType) override;

  futures::Future<Result> doCommit() override;
  Result doAbort() override;

 private:
  void maybeDisableIndexing();
  bool mustBeReplicated() const;
  bool _hasActiveTrx = false;
  std::mutex _commitLock;
};

}  // namespace arangodb
