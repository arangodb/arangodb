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

#include "RocksDBEngine/Methods/RocksDBTrxBaseMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "VocBase/Identifiers/DataSourceId.h"

namespace arangodb {
class RocksDBTransactionMethods;

class SimpleRocksDBTransactionState final : public RocksDBTransactionState,
                                            public IRocksDBTransactionCallback {
 public:
  SimpleRocksDBTransactionState(TRI_vocbase_t& vocbase, TransactionId tid,
                                transaction::Options const& options,
                                transaction::OperationOrigin operationOrigin);

  ~SimpleRocksDBTransactionState() override;

  /// @brief begin a transaction
  futures::Future<Result> beginTransaction(transaction::Hints hints) override;

  RocksDBTransactionMethods* rocksdbMethods(DataSourceId) const override {
    return _rocksMethods.get();
  }

  void beginQuery(std::shared_ptr<ResourceMonitor> resourceMonitor,
                  bool isModificationQuery) override;
  void endQuery(bool isModificationQuery) noexcept override;

  /// @returns tick of last operation in a transaction
  /// @note the value is guaranteed to be valid only after
  ///       transaction is committed
  TRI_voc_tick_t lastOperationTick() const noexcept override;

  /// @brief number of commits, including intermediate commits
  uint64_t numCommits() const noexcept override;

  /// @brief number of intermediate commits
  uint64_t numIntermediateCommits() const noexcept override;

  void addIntermediateCommits(uint64_t value) override;

  bool hasOperations() const noexcept override;

  uint64_t numOperations() const noexcept override;

  uint64_t numPrimitiveOperations() const noexcept override;

  bool ensureSnapshot() override;

  rocksdb::SequenceNumber beginSeq() const override;

  /// @brief only called on replication2 follower
  arangodb::Result triggerIntermediateCommit() override;

  [[nodiscard]] futures::Future<Result> performIntermediateCommitIfRequired(
      DataSourceId collectionId) override;

  /// @brief provide debug info for transaction state
  std::string debugInfo() const override;

 protected:
  // IRocksDBTransactionCallback methods
  rocksdb::SequenceNumber prepare() override;
  void cleanup() override;
  void commit(rocksdb::SequenceNumber lastWritten) override;

  std::unique_ptr<TransactionCollection> createTransactionCollection(
      DataSourceId cid, AccessMode::Type accessType) override;

  futures::Future<Result> doCommit() override;
  Result doAbort() override;

 private:
  void maybeDisableIndexing();

  /// @brief wrapper to use outside this class to access rocksdb
  std::unique_ptr<RocksDBTransactionMethods> _rocksMethods;
};

}  // namespace arangodb
