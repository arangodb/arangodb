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

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_BUILDER_INDEX_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_BUILDER_INDEX_H 1

#include "RocksDBEngine/RocksDBIndex.h"

#include <mutex>

namespace arangodb {
  
class RocksDBCollection;

/// Dummy index class that contains the logic to build indexes
/// without an exclusive lock. It wraps the actual index implementation
/// and adds some required synchronization logic on top
class RocksDBBuilderIndex final : public arangodb::RocksDBIndex {
 public:
  
  explicit RocksDBBuilderIndex(std::shared_ptr<arangodb::RocksDBIndex> const&);
  
  /// @brief return a VelocyPack representation of the index
  void toVelocyPack(velocypack::Builder& builder,
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

  void afterTruncate(TRI_voc_tick_t tick,
                     arangodb::transaction::Methods* trx) override {
    _wrapped->afterTruncate(tick, trx);
  }

  void load() override { _wrapped->load(); }

  void unload() override { _wrapped->unload(); }

  /// @brief whether or not the index has a selectivity estimate
  bool hasSelectivityEstimate() const override { return false; }

  /// insert index elements into the specified write batch.
  Result insert(transaction::Methods& trx, RocksDBMethods*, LocalDocumentId const& documentId,
                arangodb::velocypack::Slice const, OperationOptions& options) override;

  /// remove index elements and put it in the specified write batch.
  Result remove(transaction::Methods& trx, RocksDBMethods*, LocalDocumentId const& documentId,
                arangodb::velocypack::Slice const) override;

  /// @brief get index estimator, optional
  RocksDBCuckooIndexEstimator<uint64_t>* estimator() override {
    return _wrapped->estimator();
  }
  void setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>>) override {
    TRI_ASSERT(false);
  }
  void recalculateEstimates() override { _wrapped->recalculateEstimates(); }

  /// @brief assumes an exclusive lock on the collection
  Result fillIndexForeground();
  
  struct Locker {
    explicit Locker(RocksDBCollection* c) : _collection(c), _locked(false) {}
    ~Locker() { unlock(); }
    bool lock();
    void unlock();
    bool isLocked() const { return _locked; }
  private:
    RocksDBCollection* const _collection;
    bool _locked;
  };

  /// @brief fill the index, assume already locked exclusively
  /// @param locker locks and unlocks the collection
  Result fillIndexBackground(Locker& locker);

 private:
  std::shared_ptr<arangodb::RocksDBIndex> _wrapped;
};
}  // namespace arangodb

#endif
