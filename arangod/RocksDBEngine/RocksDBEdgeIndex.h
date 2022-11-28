////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

namespace arangodb {
class DatabaseFeature;
class RocksDBEdgeIndex;

class RocksDBEdgeIndexWarmupTask {
 public:
  RocksDBEdgeIndexWarmupTask(DatabaseFeature& databaseFeature,
                             std::string const& dbName,
                             std::string const& collectionName, IndexId iid,
                             rocksdb::Slice lower, rocksdb::Slice upper);
  Result run();

 private:
  DatabaseFeature& _databaseFeature;
  std::string const _dbName;
  std::string const _collectionName;
  IndexId const _iid;
  std::string const _lower;
  std::string const _upper;
};

class RocksDBEdgeIndex final : public RocksDBIndex {
  friend class RocksDBEdgeIndexLookupIterator;
  friend class RocksDBEdgeIndexWarmupTask;

 public:
  static uint64_t HashForKey(rocksdb::Slice const& key);

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

  // warm up the index caches
  Result scheduleWarmup() override;

  void afterTruncate(TRI_voc_tick_t tick, transaction::Methods* trx) override;

  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& documentId, velocypack::Slice doc,
                OperationOptions const& options,
                bool /*performChecks*/) override;

  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& documentId,
                velocypack::Slice doc) override;

  void refillCache(transaction::Methods& trx,
                   std::vector<std::string> const& keys) override;

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

  // name of direction attribute (i.e. "_from" or "_to")
  std::string const _directionAttr;
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
