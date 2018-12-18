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

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_EDGE_INDEX_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_EDGE_INDEX_H 1

#include "Basics/Common.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/StringRef.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

namespace rocksdb {
class TransactionDB;
class Iterator;
}  // namespace rocksdb

namespace arangodb {
class RocksDBEdgeIndex;

class RocksDBEdgeIndexIterator final : public IndexIterator {
 public:
  RocksDBEdgeIndexIterator(LogicalCollection* collection,
                           transaction::Methods* trx,
                           arangodb::RocksDBEdgeIndex const* index,
                           std::unique_ptr<VPackBuilder> keys,
                           std::shared_ptr<cache::Cache>);
  ~RocksDBEdgeIndexIterator();
  char const* typeName() const override { return "edge-index-iterator"; }
  bool hasExtra() const override { return true; }
  bool next(LocalDocumentIdCallback const& cb, size_t limit) override;
  bool nextCovering(DocumentCallback const& cb, size_t limit) override;
  bool nextExtra(ExtraCallback const& cb, size_t limit) override;
  void reset() override;
  
  /// @brief we provide a method to provide the index attribute values
  /// while scanning the index
  bool hasCovering() const override { return true; }

 private:
  // returns true if we have one more key for the index lookup.
  // if true, sets the `key` Slice to point to the new key's value
  // note that the underlying data for the Slice must remain valid
  // as long as the iterator is used and the key is not moved forward.
  // returns false if there are no more keys to look for
  bool initKey(arangodb::velocypack::Slice& key);
  void resetInplaceMemory();
  arangodb::StringRef getFromToFromIterator(
      arangodb::velocypack::ArrayIterator const&);
  void lookupInRocksDB(StringRef edgeKey);

  std::unique_ptr<arangodb::velocypack::Builder> _keys;
  arangodb::velocypack::ArrayIterator _keysIterator;
  RocksDBEdgeIndex const* _index;

  // the following 2 values are required for correct batch handling
  std::unique_ptr<rocksdb::Iterator> _iterator;  // iterator position in rocksdb
  RocksDBKeyBounds _bounds;
  std::shared_ptr<cache::Cache> _cache;
  arangodb::velocypack::ArrayIterator _builderIterator;
  arangodb::velocypack::Builder _builder;
  arangodb::velocypack::Slice _lastKey;
};

class RocksDBEdgeIndexWarmupTask : public basics::LocalTask {
 private:
  RocksDBEdgeIndex* _index;
  transaction::Methods* _trx;
  std::string const _lower;
  std::string const _upper;

 public:
  RocksDBEdgeIndexWarmupTask(
      std::shared_ptr<basics::LocalTaskQueue> const& queue,
      RocksDBEdgeIndex* index,
      transaction::Methods* trx,
      rocksdb::Slice const& lower,
      rocksdb::Slice const& upper);
  void run() override;
};

class RocksDBEdgeIndex final : public RocksDBIndex {
  friend class RocksDBEdgeIndexIterator;
  friend class RocksDBEdgeIndexWarmupTask;

 public:
  static uint64_t HashForKey(const rocksdb::Slice& key);

  RocksDBEdgeIndex() = delete;

  RocksDBEdgeIndex(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection& collection,
    arangodb::velocypack::Slice const& info,
    std::string const& attr
  );

  ~RocksDBEdgeIndex();

  IndexType type() const override { return Index::TRI_IDX_TYPE_EDGE_INDEX; }

  char const* typeName() const override { return "edge"; }

  bool canBeDropped() const override { return false; }

  bool hasCoveringIterator() const override { return true; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimate(arangodb::StringRef const& = arangodb::StringRef()) const override;

  RocksDBCuckooIndexEstimator<uint64_t>* estimator() override;
  void setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>>) override;
  void recalculateEstimates() override;

  void toVelocyPack(VPackBuilder&,
                    std::underlying_type<Index::Serialize>::type) const override;

  void batchInsert(
    transaction::Methods& trx,
    std::vector<std::pair<LocalDocumentId, velocypack::Slice>> const& docs,
    std::shared_ptr<basics::LocalTaskQueue> queue
  ) override;

  bool hasBatchInsert() const override { return false; }

  bool supportsFilterCondition(std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
                               arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  IndexIterator* iteratorForCondition(transaction::Methods*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      IndexIteratorOptions const&) override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

  /// @brief Warmup the index caches.
  void warmup(arangodb::transaction::Methods* trx,
              std::shared_ptr<basics::LocalTaskQueue> queue) override;

  void afterTruncate(TRI_voc_tick_t tick) override;

  Result insertInternal(
    transaction::Methods& trx,
    RocksDBMethods* methods,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
  ) override;

  Result removeInternal(
    transaction::Methods& trx,
    RocksDBMethods* methods,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
  ) override;

 private:
  /// @brief create the iterator
  IndexIterator* createEqIterator(transaction::Methods*,
                                  arangodb::aql::AstNode const*,
                                  arangodb::aql::AstNode const*) const;

  IndexIterator* createInIterator(transaction::Methods*,
                                  arangodb::aql::AstNode const*,
                                  arangodb::aql::AstNode const*) const;

  /// @brief add a single value node to the iterator's keys
  void handleValNode(VPackBuilder* keys,
                     arangodb::aql::AstNode const* valNode) const;

  void warmupInternal(transaction::Methods* trx,
                      rocksdb::Slice const& lower, rocksdb::Slice const& upper);

 private:

  std::string const _directionAttr;
  bool const _isFromIndex;

  /// @brief A fixed size library to estimate the selectivity of the index.
  /// On insertion of a document we have to insert it into the estimator,
  /// On removal we have to remove it in the estimator as well.
  std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>> _estimator;
};
}  // namespace arangodb

#endif
