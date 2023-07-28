////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rocksdb {
class Slice;
}

namespace arangodb {
namespace velocypack {
class Slice;
}

class DatabaseFeature;
class RocksDBEdgeIndex;

class RocksDBEdgeIndex final : public RocksDBIndex {
  friend class RocksDBEdgeIndexLookupIterator;

 public:
  static uint64_t HashForKey(rocksdb::Slice key);

  RocksDBEdgeIndex() = delete;

  RocksDBEdgeIndex(IndexId iid, LogicalCollection& collection,
                   velocypack::Slice info, std::string const& attr);

  ~RocksDBEdgeIndex();

  IndexType type() const override { return Index::TRI_IDX_TYPE_EDGE_INDEX; }

  char const* typeName() const override { return "edge"; }

  bool canBeDropped() const override { return false; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return true; }

  std::vector<std::vector<basics::AttributeName>> const& coveredFields()
      const override;

  double selectivityEstimate(
      std::string_view = std::string_view()) const override;

  RocksDBCuckooIndexEstimatorType* estimator() override;
  void setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimatorType>) override;
  void recalculateEstimates() override;

  void toVelocyPack(VPackBuilder&, std::underlying_type<Index::Serialize>::type)
      const override;

  Index::FilterCosts supportsFilterCondition(
      transaction::Methods& trx,
      std::vector<std::shared_ptr<Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const override;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      ResourceMonitor& monitor, transaction::Methods* trx,
      aql::AstNode const* node, aql::Variable const* reference,
      IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
      int) override;

  aql::AstNode* specializeCondition(
      transaction::Methods& trx, aql::AstNode* node,
      aql::Variable const* reference) const override;

  // warm up the index cache
  Result warmup() override;

  void truncateCommit(TruncateGuard&& guard, TRI_voc_tick_t tick,
                      transaction::Methods* trx) final;

  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& documentId, velocypack::Slice doc,
                OperationOptions const& options,
                bool /*performChecks*/) override;

  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& documentId, velocypack::Slice doc,
                OperationOptions const& options) override;

  void refillCache(transaction::Methods& trx,
                   std::vector<std::string> const& keys) override;

  // build a potentially prefix compressed lookup key for cache lookups.
  // the return value is either
  // - an empty std::string_view if the lookup value is syntactically
  //   invalid (syntactially invalid _from/_to value),
  // - identical to the input lookup value if the collection name in the
  //   lookup value is not identical to the cached collection name
  // - a suffix of the original lookup value (starting with the forward
  //   slash) if the collection name of the lookup value matches the
  //   cached collection name (e.g. 'someCollection/abc' will be returned
  //   as just '/abc' if the cached collection name is 'someCollection''.
  // note: previous is an in/out parameter and can be used to memoize
  // the result of the collection name lookup between multiple calls of
  // this function. this is a performance optimization to avoid repeated
  // atomic lookups of the collection name once it is already known.
  // note: returns an empty string view for invalid lookup values!
  std::string_view buildCompressedCacheKey(std::string const*& previous,
                                           std::string_view value) const;
  std::string_view buildCompressedCacheValue(std::string const*& previous,
                                             std::string_view value) const;

  std::string const* cachedValueCollection(
      std::string const*& previous) const noexcept;

  class CachedCollectionName {
   public:
    CachedCollectionName() noexcept;
    ~CachedCollectionName();

    // note: previous is an in/out parameter and can be used to memoize
    // the result of the collection name lookup between multiple calls of
    // this function. this is a performance optimization to avoid repeated
    // atomic lookups of the collection name once it is already known.
    // note: returns an empty string view for invalid lookup values!
    std::string_view buildCompressedValue(std::string const*& previous,
                                          std::string_view value) const;

    std::string const* get() const noexcept;

   private:
    std::atomic<std::string const*> mutable _name;
  };

 private:
  std::unique_ptr<IndexIterator> createEqIterator(
      ResourceMonitor& monitor, transaction::Methods* trx, aql::AstNode const*,
      aql::AstNode const* valNode, bool useCache,
      ReadOwnWrites readOwnWrites) const;

  std::unique_ptr<IndexIterator> createInIterator(ResourceMonitor& monitor,
                                                  transaction::Methods* trx,
                                                  aql::AstNode const*,
                                                  aql::AstNode const* valNode,
                                                  bool useCache) const;

  // populate the keys builder with a single (string) lookup value
  void fillLookupValue(velocypack::Builder& keys,
                       aql::AstNode const* value) const;

  // populate the keys builder with the keys from the array
  void fillInLookupValues(transaction::Methods* trx, velocypack::Builder& keys,
                          aql::AstNode const* values) const;

  // add a single value node to the iterator's keys
  void handleValNode(VPackBuilder* keys, aql::AstNode const* valNode) const;

  void warmupInternal(transaction::Methods* trx, rocksdb::Slice lower,
                      rocksdb::Slice upper);

  void handleCacheInvalidation(transaction::Methods& trx,
                               OperationOptions const& options,
                               std::string_view fromToRef);

  // name of direction attribute (i.e. "_from" or "_to")
  std::string const _directionAttr;

  // cached collection name for edge index cache keys
  CachedCollectionName _cacheKeyCollectionName;

  // cached collection name for edge index cache values
  CachedCollectionName _cacheValueCollectionName;

  // whether or not this is the _from part
  bool const _isFromIndex;

  // if true, force a refill of the in-memory cache after each
  // insert/update/replace operation
  bool const _forceCacheRefill;

  // fixed size buffer to estimate the selectivity of the index.
  // on insertion of a document we have to insert it into the estimator,
  // on removal we have to remove it in the estimator as well.
  std::unique_ptr<RocksDBCuckooIndexEstimatorType> _estimator;

  // the list of attributes covered by this index.
  //        First is the actual index attribute (e.g. _from), second is the
  //        opposite (e.g. _to)
  std::vector<std::vector<basics::AttributeName>> const _coveredFields;
};
}  // namespace arangodb
