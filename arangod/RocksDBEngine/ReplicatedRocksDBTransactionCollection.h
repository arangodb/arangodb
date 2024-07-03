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
#include "RocksDBEngine/RocksDBTransactionCollection.h"

namespace arangodb {
struct ResourceMonitor;

namespace futures {
template<class T>
class Future;
}
namespace replication2::replicated_state::document {
struct DocumentLeaderState;
}
class RocksDBTransactionMethods;
class ReplicatedRocksDBTransactionState;

class ReplicatedRocksDBTransactionCollection final
    : public RocksDBTransactionCollection,
      public IRocksDBTransactionCallback {
 public:
  ReplicatedRocksDBTransactionCollection(ReplicatedRocksDBTransactionState* trx,
                                         DataSourceId cid,
                                         AccessMode::Type accessType);
  ~ReplicatedRocksDBTransactionCollection();

  Result beginTransaction();
  Result commitTransaction();
  Result abortTransaction();

  RocksDBTransactionMethods* rocksdbMethods() const {
    return _rocksMethods.get();
  }

  void beginQuery(std::shared_ptr<ResourceMonitor> resourceMonitor,
                  bool isModificationQuery);
  void endQuery(bool isModificationQuery) noexcept;

  /// @returns tick of last operation in a transaction
  /// @note the value is guaranteed to be valid only after
  ///       transaction is committed
  TRI_voc_tick_t lastOperationTick() const noexcept;

  /// @brief number of commits, including intermediate commits
  uint64_t numCommits() const noexcept;

  /// @brief number intermediate commits
  uint64_t numIntermediateCommits() const noexcept;

  futures::Future<Result> performIntermediateCommitIfRequired();

  uint64_t numOperations() const noexcept;

  uint64_t numPrimitiveOperations() const noexcept;

  bool ensureSnapshot();

  auto leaderState() -> std::shared_ptr<
      replication2::replicated_state::document::DocumentLeaderState>;

 protected:
  auto ensureCollection() -> Result override;

  // IRocksDBTransactionCallback methods
  rocksdb::SequenceNumber prepare() override;
  void cleanup() override;
  void commit(rocksdb::SequenceNumber lastWritten) override;

 private:
  void maybeDisableIndexing();

  std::shared_ptr<replication2::replicated_state::document::DocumentLeaderState>
      _leaderState;
  /// @brief wrapper to use outside this class to access rocksdb
  std::unique_ptr<RocksDBTransactionMethods> _rocksMethods;
};

}  // namespace arangodb
