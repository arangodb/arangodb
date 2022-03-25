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
/// @author Jan Steemann
/// @author Daniel H. Larkin
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <rocksdb/comparator.h>
#include <rocksdb/iterator.h>
#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>

#include "Aql/AstNode.h"
#include "Basics/Common.h"
#include "Containers/SmallVector.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

namespace arangodb {
namespace aql {
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
  kValuesOnly
};

class RocksDBVPackIndex : public RocksDBIndex {
  template<bool unique, bool reverse>
  friend class RocksDBVPackIndexIterator;

 public:
  static uint64_t HashForKey(rocksdb::Slice const& key);

  RocksDBVPackIndex() = delete;

  RocksDBVPackIndex(IndexId iid, LogicalCollection& collection,
                    arangodb::velocypack::Slice info);

  ~RocksDBVPackIndex();

  std::vector<std::vector<arangodb::basics::AttributeName>> const&
  coveredFields() const override;

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

  Index::FilterCosts supportsFilterCondition(
      std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
      arangodb::aql::AstNode const* node,
      arangodb::aql::Variable const* reference,
      size_t itemsInIndex) const override;

  Index::SortCosts supportsSortCondition(
      arangodb::aql::SortCondition const* sortCondition,
      arangodb::aql::Variable const* reference,
      size_t itemsInIndex) const override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode* node,
      arangodb::aql::Variable const* reference) const override;

  std::unique_ptr<IndexIterator> iteratorForCondition(
      transaction::Methods* trx, arangodb::aql::AstNode const* node,
      arangodb::aql::Variable const* reference,
      IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
      int) override;

  void afterTruncate(TRI_voc_tick_t tick,
                     arangodb::transaction::Methods* trx) override;

  std::shared_ptr<cache::Cache> makeCache() const override;

  bool hasStoredValues() const noexcept { return !_storedValues.empty(); }

  // build new search values. this can also be called from the
  // VPackIndexIterator
  void buildSearchValues(arangodb::aql::AstNode const* node,
                         arangodb::aql::Variable const* reference,
                         VPackBuilder& searchValues,
                         RocksDBVPackIndexSearchValueFormat& format,
                         bool& needNormalize) const;

 protected:
  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& documentId, velocypack::Slice doc,
                OperationOptions const& options, bool performChecks) override;

  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& documentId,
                velocypack::Slice doc) override;

  Result update(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& oldDocumentId, velocypack::Slice oldDoc,
                LocalDocumentId const& newDocumentId, velocypack::Slice newDoc,
                OperationOptions const& options, bool performChecks) override;

 private:
  // build an index iterator from a VelocyPack range description
  std::unique_ptr<IndexIterator> buildIterator(
      transaction::Methods* trx, arangodb::velocypack::Slice searchValues,
      IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
      RocksDBVPackIndexSearchValueFormat format) const;

  // build bounds for an index range
  void buildIndexRangeBounds(transaction::Methods* trx, VPackSlice searchValues,
                             VPackBuilder& leftSearch, VPackSlice lastNonEq,
                             RocksDBKeyBounds& bounds) const;

  std::unique_ptr<IndexIterator> buildIteratorFromBounds(
      transaction::Methods* trx, bool reverse, ReadOwnWrites readOwnWrites,
      RocksDBKeyBounds&& bounds, RocksDBVPackIndexSearchValueFormat format,
      bool useCache) const;

  /// @brief returns whether the document can be inserted into the index
  /// (or if there will be a conflict)
  [[nodiscard]] Result checkInsert(transaction::Methods& trx,
                                   RocksDBMethods* methods,
                                   LocalDocumentId const& documentId,
                                   velocypack::Slice doc,
                                   OperationOptions const& options) override;

  /// @brief returns whether the document can be updated/replaced in the index
  /// (or if there will be a conflict)
  [[nodiscard]] Result checkReplace(transaction::Methods& trx,
                                    RocksDBMethods* methods,
                                    LocalDocumentId const& documentId,
                                    velocypack::Slice doc,
                                    OperationOptions const& options) override;

  [[nodiscard]] Result checkOperation(transaction::Methods& trx,
                                      RocksDBMethods* methods,
                                      LocalDocumentId const& documentId,
                                      velocypack::Slice doc,
                                      OperationOptions const& options,
                                      bool ignoreExisting);

  /// @brief helper function to transform AttributeNames into string lists
  void fillPaths(
      std::vector<std::vector<arangodb::basics::AttributeName>> const& source,
      std::vector<std::vector<std::string>>& paths,
      std::vector<int>* expanding);

  /// @brief helper function to insert a document into any index type
  ErrorCode fillElement(
      velocypack::Builder& leased, LocalDocumentId const& documentId,
      VPackSlice doc, ::arangodb::containers::SmallVector<RocksDBKey>& elements,
      ::arangodb::containers::SmallVector<uint64_t>& hashes);

  /// @brief helper function to build the key and value for rocksdb from the
  /// vector of slices
  /// @param hashes list of VPackSlice hashes for the estimator.
  void addIndexValue(
      velocypack::Builder& leased, LocalDocumentId const& documentId,
      VPackSlice document,
      ::arangodb::containers::SmallVector<RocksDBKey>& elements,
      ::arangodb::containers::SmallVector<uint64_t>& hashes,
      ::arangodb::containers::SmallVector<VPackSlice>& sliceStack);

  /// @brief helper function to create a set of value combinations to insert
  /// into the rocksdb index.
  /// @param elements vector of resulting index entries
  /// @param sliceStack working list of values to insert into the index
  /// @param hashes list of VPackSlice hashes for the estimator.
  void buildIndexValues(
      velocypack::Builder& leased, LocalDocumentId const& documentId,
      VPackSlice const document, size_t level,
      ::arangodb::containers::SmallVector<RocksDBKey>& elements,
      ::arangodb::containers::SmallVector<uint64_t>& hashes,
      ::arangodb::containers::SmallVector<VPackSlice>& sliceStack);

  /// @brief the attribute paths (for regular fields)
  std::vector<std::vector<std::string>> _paths;
  /// @brief the attribute paths (for stored values)
  std::vector<std::vector<std::string>> _storedValuesPaths;

  /// @brief ... and which of them expands
  /// @brief a -1 entry means none is expanding,
  /// otherwise the non-negative number is the index of the expanding one.
  std::vector<int> _expanding;

  /// @brief whether or not the user requested to use a cache for the index.
  /// note: even if this is set to true, it may not mean that the cache is
  /// effectively in use. for example, for system collections and on the
  /// coordinator, no cache will actually be used although this flag may be true
  bool const _cacheEnabled;

  /// @brief whether or not array indexes will de-duplicate their input values
  bool const _deduplicate;

  /// @brief whether or not we want to have estimates
  bool _estimates;

  /// @brief A fixed size buffer to estimate the selectivity of the index.
  /// On insertion of a document we have to insert it into the estimator,
  /// On removal we have to remove it in the estimator as well.
  std::unique_ptr<RocksDBCuckooIndexEstimatorType> _estimator;

  std::vector<std::vector<arangodb::basics::AttributeName>> const _storedValues;

  std::vector<std::vector<arangodb::basics::AttributeName>> const
      _coveredFields;
};

}  // namespace arangodb
