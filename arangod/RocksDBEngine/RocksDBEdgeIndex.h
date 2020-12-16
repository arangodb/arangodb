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

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>

#include "Basics/Common.h"
#include "Basics/LocalTaskQueue.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

namespace arangodb {
class RocksDBEdgeIndex;

class RocksDBEdgeIndexWarmupTask : public basics::LocalTask {
 private:
  RocksDBEdgeIndex* _index;
  transaction::Methods* _trx;
  std::string const _lower;
  std::string const _upper;

 public:
  RocksDBEdgeIndexWarmupTask(std::shared_ptr<basics::LocalTaskQueue> const& queue,
                             RocksDBEdgeIndex* index, transaction::Methods* trx,
                             rocksdb::Slice const& lower, rocksdb::Slice const& upper);
  void run() override;
};

class RocksDBEdgeIndex final : public RocksDBIndex {
  friend class RocksDBEdgeIndexLookupIterator;
  friend class RocksDBEdgeIndexWarmupTask;

 public:
  static uint64_t HashForKey(const rocksdb::Slice& key);

  RocksDBEdgeIndex() = delete;

  RocksDBEdgeIndex(IndexId iid, arangodb::LogicalCollection& collection,
                   arangodb::velocypack::Slice const& info, std::string const& attr);

  ~RocksDBEdgeIndex();

  IndexType type() const override { return Index::TRI_IDX_TYPE_EDGE_INDEX; }

  char const* typeName() const override { return "edge"; }

  bool canBeDropped() const override { return false; }

  bool hasCoveringIterator() const override { return true; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return true; }

  std::vector<std::vector<arangodb::basics::AttributeName>> const& coveredFields() const override;

  double selectivityEstimate(arangodb::velocypack::StringRef const& = arangodb::velocypack::StringRef()) const override;

  RocksDBCuckooIndexEstimator<uint64_t>* estimator() override;
  void setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>>) override;
  void recalculateEstimates() override;

  void toVelocyPack(VPackBuilder&, std::underlying_type<Index::Serialize>::type) const override;

  Index::FilterCosts supportsFilterCondition(std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
                                             arangodb::aql::AstNode const* node,
                                             arangodb::aql::Variable const* reference,
                                             size_t itemsInIndex) const override;

  std::unique_ptr<IndexIterator> iteratorForCondition(transaction::Methods* trx, 
                                                      arangodb::aql::AstNode const* node,
                                                      arangodb::aql::Variable const* reference,
                                                      IndexIteratorOptions const& opts) override;

  arangodb::aql::AstNode* specializeCondition(arangodb::aql::AstNode* node,
                                              arangodb::aql::Variable const* reference) const override;

  /// @brief Warmup the index caches.
  void warmup(arangodb::transaction::Methods* trx,
              std::shared_ptr<basics::LocalTaskQueue> queue) override;

  void afterTruncate(TRI_voc_tick_t tick,
                     arangodb::transaction::Methods* trx) override;

  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& documentId, velocypack::Slice const& doc,
                OperationOptions& options) override;

  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& documentId,
                velocypack::Slice const& doc, Index::OperationMode mode) override;

 private:
  /// @brief create the iterator
  std::unique_ptr<IndexIterator> createEqIterator(transaction::Methods*,
                                                  arangodb::aql::AstNode const*,
                                                  arangodb::aql::AstNode const*, bool) const;

  std::unique_ptr<IndexIterator> createInIterator(transaction::Methods*,
                                                  arangodb::aql::AstNode const*,
                                                  arangodb::aql::AstNode const*, bool) const;

  /// @brief populate the keys builder with a single (string) lookup value
  void fillLookupValue(arangodb::velocypack::Builder& keys,
                       arangodb::aql::AstNode const* value) const;

  /// @brief populate the keys builder with the keys from the array
  void fillInLookupValues(transaction::Methods* trx,
                          arangodb::velocypack::Builder& keys,
                          arangodb::aql::AstNode const* values) const;

  /// @brief add a single value node to the iterator's keys
  void handleValNode(VPackBuilder* keys, arangodb::aql::AstNode const* valNode) const;

  void warmupInternal(transaction::Methods* trx, rocksdb::Slice const& lower,
                      rocksdb::Slice const& upper);

 private:
  std::string const _directionAttr;
  bool const _isFromIndex;

  /// @brief A fixed size library to estimate the selectivity of the index.
  /// On insertion of a document we have to insert it into the estimator,
  /// On removal we have to remove it in the estimator as well.
  std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>> _estimator;

  /// @brief The list of attributes covered by this index.
  ///        First is the actual index attribute (e.g. _from), second is the
  ///        opposite (e.g. _to)
  std::vector<std::vector<arangodb::basics::AttributeName>> const _coveredFields;
};
}  // namespace arangodb

#endif
