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
/// @author Jan Steemann
/// @author Daniel H. Larkin
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>

#include "Containers/SmallVector.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <span>

namespace rocksdb {
class Slice;
}

namespace arangodb {
namespace aql {
struct AstNode;
class SortCondition;
struct Variable;
}  // namespace aql

class LogicalCollection;
class RocksDBPrimaryIndex;
class RocksDBVPackIndex;

namespace transaction {
class Methods;
}

enum class RocksDBVPackIndexSearchValueFormat : uint8_t {
  kDetect,
  kOperatorsAndValues,
  kValuesOnly,
  kIn,
};

class RocksDBVPackIndex : public RocksDBIndex {
  template<bool unique, bool reverse, bool mustCheckBounds>
  friend class RocksDBVPackIndexIterator;
  friend class RocksDBVPackIndexInIterator;

 public:
  static uint64_t HashForKey(rocksdb::Slice const& key);

  RocksDBVPackIndex() = delete;

  RocksDBVPackIndex(IndexId iid, LogicalCollection& collection,
                    velocypack::Slice info);

  ~RocksDBVPackIndex();

  std::vector<std::vector<basics::AttributeName>> const& coveredFields()
      const override;

  bool hasSelectivityEstimate() const override;

  double selectivityEstimate(
      std::string_view = std::string_view()) const override;

  RocksDBCuckooIndexEstimatorType* estimator() override;
  void setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimatorType>) override;
  void recalculateEstimates() override;

  void toVelocyPack(
      velocypack::Builder&,
      std::underlying_type<Index::Serialize>::type) const override;

  bool canBeDropped() const override { return true; }

  /// @brief return the attribute paths
  std::vector<std::vector<std::string>> const& paths() const { return _paths; }

  /// @brief whether or not the index has estimates
  bool hasEstimates() const noexcept { return _estimates; }

  // warm up the index cache
  Result warmup() override;

  Index::FilterCosts supportsFilterCondition(
      transaction::Methods& trx,
      std::vector<std::shared_ptr<Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const override;

  Index::SortCosts supportsSortCondition(
      aql::SortCondition const* sortCondition, aql::Variable const* reference,
      size_t itemsInIndex) const override;

  aql::AstNode* specializeCondition(
      transaction::Methods& trx, aql::AstNode* node,
      aql::Variable const* reference) const override;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      ResourceMonitor& monitor, transaction::Methods* trx,
      aql::AstNode const* node, aql::Variable const* reference,
      IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
      int) override;

  StreamSupportResult supportsStreamInterface(
      IndexStreamOptions const&) const noexcept override;
  static StreamSupportResult checkSupportsStreamInterface(
      std::vector<std::vector<basics::AttributeName>> const& coveredFields,
      std::vector<std::vector<basics::AttributeName>> const& fields,
      bool isUnique, IndexStreamOptions const&) noexcept;

  virtual std::unique_ptr<AqlIndexStreamIterator> streamForCondition(
      transaction::Methods* trx, IndexStreamOptions const&) override;

  void truncateCommit(TruncateGuard&& guard, TRI_voc_tick_t tick,
                      transaction::Methods* trx) final;

  Result drop() override;

  std::shared_ptr<cache::Cache> makeCache() const override;

  size_t numFieldsToConsiderInIndexSelection() const noexcept override {
    return _fields.size() + _storedValues.size();
  }

  bool hasStoredValues() const noexcept { return !_storedValues.empty(); }

  void buildEmptySearchValues(velocypack::Builder& result) const;

  // build new search values. this can also be called from the
  // VPackIndexIterator
  void buildSearchValues(ResourceMonitor& monitor, transaction::Methods* trx,
                         aql::AstNode const* node,
                         aql::Variable const* reference,
                         IndexIteratorOptions const& opts,
                         velocypack::Builder& searchValues,
                         RocksDBVPackIndexSearchValueFormat& format) const;

  void buildSearchValuesInner(ResourceMonitor& monitor,
                              transaction::Methods* trx,
                              aql::AstNode const* node,
                              aql::Variable const* reference,
                              IndexIteratorOptions const& opts,
                              velocypack::Builder& searchValues,
                              RocksDBVPackIndexSearchValueFormat& format) const;

 protected:
  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& options, bool performChecks) override;

  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& options) override;

  Result update(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId oldDocumentId, velocypack::Slice oldDoc,
                LocalDocumentId newDocumentId, velocypack::Slice newDoc,
                OperationOptions const& options, bool performChecks) override;

  void refillCache(transaction::Methods& trx,
                   std::vector<std::string> const& keys) override;

 private:
  Result insertUnique(transaction::Methods& trx, RocksDBMethods* mthds,
                      LocalDocumentId documentId, velocypack::Slice doc,
                      containers::SmallVector<RocksDBKey, 4> const& elements,
                      containers::SmallVector<uint64_t, 4> hashes,
                      OperationOptions const& options, bool performChecks);

  Result insertNonUnique(transaction::Methods& trx, RocksDBMethods* mthds,
                         LocalDocumentId documentId, velocypack::Slice doc,
                         containers::SmallVector<RocksDBKey, 4> const& elements,
                         containers::SmallVector<uint64_t, 4> hashes,
                         OperationOptions const& options);

  void expandInSearchValues(ResourceMonitor& monitor, velocypack::Slice base,
                            velocypack::Builder& result,
                            IndexIteratorOptions const& opts) const;

  // build an index iterator from a VelocyPack range description
  std::unique_ptr<IndexIterator> buildIterator(
      ResourceMonitor& monitor, transaction::Methods* trx,
      velocypack::Slice searchValues, IndexIteratorOptions const& opts,
      ReadOwnWrites readOwnWrites, RocksDBVPackIndexSearchValueFormat format,
      bool& isUniqueIndexIterator) const;

  // build bounds for an index range
  void buildIndexRangeBounds(transaction::Methods* trx, VPackSlice searchValues,
                             VPackBuilder& leftSearch, VPackSlice lastNonEq,
                             RocksDBKeyBounds& bounds) const;

  std::unique_ptr<IndexIterator> buildIteratorFromBounds(
      ResourceMonitor& monitor, transaction::Methods* trx, bool reverse,
      IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
      RocksDBKeyBounds&& bounds, RocksDBVPackIndexSearchValueFormat format,
      bool withCache) const;

  /// @brief returns whether the document can be inserted into the index
  /// (or if there will be a conflict)
  [[nodiscard]] Result checkInsert(transaction::Methods& trx,
                                   RocksDBMethods* methods,
                                   LocalDocumentId documentId,
                                   velocypack::Slice doc,
                                   OperationOptions const& options) override;

  /// @brief returns whether the document can be updated/replaced in the index
  /// (or if there will be a conflict)
  [[nodiscard]] Result checkReplace(transaction::Methods& trx,
                                    RocksDBMethods* methods,
                                    LocalDocumentId documentId,
                                    velocypack::Slice doc,
                                    OperationOptions const& options) override;

  [[nodiscard]] Result checkOperation(transaction::Methods& trx,
                                      RocksDBMethods* methods,
                                      LocalDocumentId documentId,
                                      velocypack::Slice doc,
                                      OperationOptions const& options,
                                      bool ignoreExisting);

  /// @brief helper function to transform AttributeNames into string lists
  void fillPaths(std::vector<std::vector<basics::AttributeName>> const& source,
                 std::vector<std::vector<std::string>>& paths,
                 std::vector<int>* expanding);

  /// @brief helper function to insert a document into any index type
  ErrorCode fillElement(velocypack::Builder& leased, LocalDocumentId documentId,
                        VPackSlice doc,
                        containers::SmallVector<RocksDBKey, 4>& elements,
                        containers::SmallVector<uint64_t, 4>& hashes);

  /// @brief helper function to build the key and value for rocksdb from the
  /// vector of slices
  /// @param hashes list of VPackSlice hashes for the estimator.
  void addIndexValue(velocypack::Builder& leased, LocalDocumentId documentId,
                     VPackSlice document,
                     containers::SmallVector<RocksDBKey, 4>& elements,
                     containers::SmallVector<uint64_t, 4>& hashes,
                     std::span<VPackSlice const> sliceStack);

  /// @brief helper function to create a set of value combinations to insert
  /// into the rocksdb index.
  /// @param elements vector of resulting index entries
  /// @param sliceStack working list of values to insert into the index
  /// @param hashes list of VPackSlice hashes for the estimator.
  void buildIndexValues(velocypack::Builder& leased, LocalDocumentId documentId,
                        VPackSlice const document, size_t level,
                        containers::SmallVector<RocksDBKey, 4>& elements,
                        containers::SmallVector<uint64_t, 4>& hashes,
                        containers::SmallVector<VPackSlice, 4>& sliceStack);

  void warmupInternal(transaction::Methods* trx);

  void handleCacheInvalidation(cache::Cache& cache, transaction::Methods& trx,
                               OperationOptions const& options,
                               rocksdb::Slice key);

  /// @brief the attribute paths (for regular fields)
  std::vector<std::vector<std::string>> _paths;
  /// @brief the attribute paths (for stored values)
  std::vector<std::vector<std::string>> _storedValuesPaths;

  /// @brief ... and which of them expands
  /// @brief a -1 entry means none is expanding,
  /// otherwise the non-negative number is the index of the expanding one.
  std::vector<int> _expanding;

  // if true, force a refill of the in-memory cache after each
  // insert/update/replace operation
  bool const _forceCacheRefill;

  /// @brief whether or not array indexes will de-duplicate their input values
  bool const _deduplicate;

  /// @brief whether or not we want to have estimates
  bool _estimates;

  /// @brief A fixed size buffer to estimate the selectivity of the index.
  /// On insertion of a document we have to insert it into the estimator,
  /// On removal we have to remove it in the estimator as well.
  std::unique_ptr<RocksDBCuckooIndexEstimatorType> _estimator;

  std::vector<std::vector<basics::AttributeName>> const _storedValues;

  std::vector<std::vector<basics::AttributeName>> const _coveredFields;
};

}  // namespace arangodb
