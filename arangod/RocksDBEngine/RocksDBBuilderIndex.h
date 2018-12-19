////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

/// Dummy index class that contains the logic to build indexes
/// without an exclusive lock. It wraps the actual index implementation
/// and adds some required synchronization logic on top
class RocksDBBuilderIndex final : public arangodb::RocksDBIndex {

 public:
  /// @brief return a VelocyPack representation of the index
  void toVelocyPack(velocypack::Builder& builder,
                    std::underlying_type<Index::Serialize>::type) const override;
  
  char const* typeName() const override {
    return _wrapped->typeName();
  }
  
  IndexType type() const override {
    return _wrapped->type();
  }
  
  bool canBeDropped() const override {
    return false; // TODO ?!
  }

  /// @brief whether or not the index is sorted
  bool isSorted() const override {
    return _wrapped->isSorted();
  }
  
  /// @brief if true this index should not be shown externally
  bool isHidden() const override {
    return true; // do not show building indexes
  }
  
  size_t memory() const override {
    return _wrapped->memory();
  }
  
  Result drop() override {
    return _wrapped->drop();
  }
  
  void afterTruncate(TRI_voc_tick_t tick) override {
    _wrapped->afterTruncate(tick);
  }
  
  void load() override {
    _wrapped->load();
  }
  
  void unload() override {
    _wrapped->unload();
  }
  
  /// @brief whether or not the index has a selectivity estimate
  bool hasSelectivityEstimate() const override {
    return false;
  }

  /// insert index elements into the specified write batch.
  Result insertInternal(transaction::Methods& trx, RocksDBMethods*,
                        LocalDocumentId const& documentId,
                        arangodb::velocypack::Slice const&,
                        OperationMode mode) override;

  /// remove index elements and put it in the specified write batch.
  Result removeInternal(transaction::Methods& trx, RocksDBMethods*,
                        LocalDocumentId const& documentId,
                        arangodb::velocypack::Slice const&,
                        OperationMode mode) override;

  RocksDBBuilderIndex(std::shared_ptr<arangodb::RocksDBIndex> const&);
  
  /// @brief get index estimator, optional
  RocksDBCuckooIndexEstimator<uint64_t>* estimator() override {
    return _wrapped->estimator();
  }
  void setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>>) override {
    TRI_ASSERT(false);
  }
  void recalculateEstimates() override {
    _wrapped->recalculateEstimates();
  }
  
  /// @brief fill index, will exclusively lock the collection
  Result fillIndexFast();
  
  /// @brief fill the index, assume already locked exclusively
  /// @param unlock called when collection lock can be released
  Result fillIndexBackground(std::function<void()> const& unlock);
  
  virtual IndexIterator* iteratorForCondition(
    transaction::Methods* trx,
    ManagedDocumentResult* result,
    aql::AstNode const* condNode,
    aql::Variable const* var,
    IndexIteratorOptions const& opts
  ) override { 
    TRI_ASSERT(false);
    return nullptr;
  }
  
 private:
  std::shared_ptr<arangodb::RocksDBIndex> _wrapped;
  
  std::atomic<bool> _hasError;
  std::mutex _errorMutex;
  Result _errorResult;
  
  std::mutex _removedDocsMutex;
  std::unordered_set<LocalDocumentId::BaseType> _removedDocs;
  
  std::mutex _lockedDocsMutex;
  std::condition_variable _lockedDocsCond;
  std::unordered_set<LocalDocumentId::BaseType> _lockedDocs;
};
}  // namespace arangodb

#endif
