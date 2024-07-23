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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "Transaction/OperationOrigin.h"

#include <atomic>

namespace arangodb {
class RocksDBBatchedBaseMethods;

namespace trx {
struct BuilderTrx : public transaction::Methods {
  BuilderTrx(std::shared_ptr<transaction::Context> const& transactionContext,
             LogicalDataSource const& collection, AccessMode::Type type,
             transaction::Options options = transaction::Options())
      : transaction::Methods(transactionContext, options),
        _cid(collection.id()) {
    // add the (sole) data-source
    addCollection(collection.id(), collection.name(), type);
  }

  /// @brief get the underlying transaction collection
  RocksDBTransactionCollection* resolveTrxCollection() {
    return static_cast<RocksDBTransactionCollection*>(trxCollection(_cid));
  }

 private:
  DataSourceId _cid;
};
}  // namespace trx

Result partiallyCommitInsertions(RocksDBBatchedBaseMethods& batched,
                                 rocksdb::WriteBatchBase& batch,
                                 rocksdb::DB* rootDB,
                                 RocksDBTransactionCollection* trxColl,
                                 std::atomic<uint64_t>& docsProcessed,
                                 RocksDBIndex& ridx, bool isForeground);

Result fillIndexSingleThreaded(
    bool foreground, RocksDBBatchedBaseMethods& batched,
    rocksdb::Options const& dbOptions, rocksdb::WriteBatchBase& batch,
    std::atomic<std::uint64_t>& docsProcessed, trx::BuilderTrx& trx,
    RocksDBIndex& ridx, rocksdb::Snapshot const* snap, rocksdb::DB* rootDB,
    std::unique_ptr<rocksdb::Iterator> it,
    std::shared_ptr<std::function<arangodb::Result(double)>> progress =
        nullptr);

class RocksDBCollection;

/// Dummy index class that contains the logic to build indexes
/// without an exclusive lock. It wraps the actual index implementation
/// and adds some required synchronization logic on top
class RocksDBBuilderIndex final : public RocksDBIndex {
 public:
  explicit RocksDBBuilderIndex(std::shared_ptr<RocksDBIndex>,
                               uint64_t numDocsHint, size_t parallelism);

  /// @brief return a VelocyPack representation of the index
  void toVelocyPack(
      velocypack::Builder& builder,
      std::underlying_type<Index::Serialize>::type) const override;

  char const* typeName() const override { return _wrapped->typeName(); }

  IndexType type() const override { return _wrapped->type(); }

  bool canBeDropped() const override {
    return false;  // TODO ?!
  }

  /// @brief whether or not the index is sorted
  bool isSorted() const override { return _wrapped->isSorted(); }

  /// @brief if true this index should not be shown externally
  bool isHidden() const override {
    return true;  // do not show building indexes
  }

  bool inProgress() const override {
    return true;  // do not show building indices
  }

  size_t memory() const override { return _wrapped->memory(); }

  Result drop() override { return _wrapped->drop(); }

  void truncateCommit(TruncateGuard&& guard, TRI_voc_tick_t tick,
                      transaction::Methods* trx) final {
    _wrapped->truncateCommit(std::move(guard), tick, trx);
  }

  void load() override { _wrapped->load(); }

  void unload() override { _wrapped->unload(); }

  /// @brief whether or not the index has a selectivity estimate
  bool hasSelectivityEstimate() const override { return false; }

  /// insert index elements into the specified write batch.
  Result insert(transaction::Methods& trx, RocksDBMethods*,
                LocalDocumentId documentId, velocypack::Slice slice,
                OperationOptions const& options,
                bool /*performChecks*/) override;

  /// remove index elements and put it in the specified write batch.
  Result remove(transaction::Methods& trx, RocksDBMethods*,
                LocalDocumentId documentId, velocypack::Slice slice,
                OperationOptions const& /*options*/) override;

  /// @brief get index estimator, optional
  RocksDBCuckooIndexEstimatorType* estimator() override {
    return _wrapped->estimator();
  }
  void setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimatorType>) override {
    TRI_ASSERT(false);
  }
  void recalculateEstimates() override { _wrapped->recalculateEstimates(); }

  /// @brief assumes an exclusive lock on the collection
  Result fillIndexForeground(
      std::shared_ptr<std::function<arangodb::Result(double)>> = nullptr);

  struct Locker {
    explicit Locker(RocksDBCollection* c) : _collection(c), _locked(false) {}
    ~Locker() { unlock(); }
    futures::Future<bool> lock();
    void unlock();
    bool isLocked() const { return _locked; }

   private:
    RocksDBCollection* const _collection;
    bool _locked;
  };

  /// @brief fill the index, assume already locked exclusively
  /// @param locker locks and unlocks the collection
  futures::Future<Result> fillIndexBackground(
      Locker& locker,
      std::shared_ptr<std::function<arangodb::Result(double)>> = nullptr);

 private:
  static constexpr uint64_t kThreadBatchSize = 100000;
  static constexpr size_t kSingleThreadThreshold = 120000;

  std::shared_ptr<RocksDBIndex> _wrapped;
  std::atomic<uint64_t> _docsProcessed;
  uint64_t const _numDocsHint;
  size_t const _numThreads;
};

}  // namespace arangodb
