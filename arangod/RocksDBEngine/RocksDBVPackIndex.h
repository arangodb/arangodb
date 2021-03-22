////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_ROCKSDB_ROCKSDB_VPACK_INDEX_H
#define ARANGOD_ROCKSDB_ROCKSDB_VPACK_INDEX_H 1

#include <rocksdb/comparator.h>
#include <rocksdb/iterator.h>
#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>

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

class RocksDBVPackIndex : public RocksDBIndex {
  template<bool reverse> friend class RocksDBVPackIndexIterator;

 public:
  static uint64_t HashForKey(const rocksdb::Slice& key);

  RocksDBVPackIndex() = delete;

  RocksDBVPackIndex(IndexId iid, LogicalCollection& collection,
                    arangodb::velocypack::Slice const& info);

  ~RocksDBVPackIndex();

  bool hasSelectivityEstimate() const override;

  double selectivityEstimate(arangodb::velocypack::StringRef const& = arangodb::velocypack::StringRef()) const override;

  RocksDBCuckooIndexEstimatorType* estimator() override;
  void setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimatorType>) override;
  void recalculateEstimates() override;

  void toVelocyPack(velocypack::Builder&,
                    std::underlying_type<Index::Serialize>::type) const override;

  bool canBeDropped() const override { return true; }

  bool hasCoveringIterator() const override { return true; }

  /// @brief return the attribute paths
  std::vector<std::vector<std::string>> const& paths() const { return _paths; }

  /// @brief return the attribute paths, a -1 entry means none is expanding,
  /// otherwise the non-negative number is the index of the expanding one.
  std::vector<int> const& expanding() const { return _expanding; }

  static constexpr size_t minimalPrefixSize() { return sizeof(TRI_voc_tick_t); }

  /// @brief attempts to locate an entry in the index
  std::unique_ptr<IndexIterator> lookup(transaction::Methods*,
                                        arangodb::velocypack::Slice const, bool reverse) const;

  Index::FilterCosts supportsFilterCondition(std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
                                             arangodb::aql::AstNode const* node,
                                             arangodb::aql::Variable const* reference, 
                                             size_t itemsInIndex) const override;

  Index::SortCosts supportsSortCondition(arangodb::aql::SortCondition const* sortCondition,
                                         arangodb::aql::Variable const* reference, 
                                         size_t itemsInIndex) const override;

  arangodb::aql::AstNode* specializeCondition(arangodb::aql::AstNode* node,
                                              arangodb::aql::Variable const* reference) const override;

  std::unique_ptr<IndexIterator> iteratorForCondition(transaction::Methods* trx, 
                                                      arangodb::aql::AstNode const* node,
                                                      arangodb::aql::Variable const* reference,
                                                      IndexIteratorOptions const& opts) override;

  void afterTruncate(TRI_voc_tick_t tick,
                     arangodb::transaction::Methods* trx) override;

 protected:
  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& documentId, velocypack::Slice doc,
                OperationOptions const& options) override;

  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& documentId,
                velocypack::Slice doc) override;

  Result update(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& oldDocumentId,
                velocypack::Slice oldDoc, LocalDocumentId const& newDocumentId,
                velocypack::Slice newDoc,
                OperationOptions const& options) override;

 private:
  /// @brief returns whether the document can be inserted into the index
  /// (or if there will be a conflict)
  Result checkInsert(transaction::Methods& trx, RocksDBMethods* methods,
                     LocalDocumentId const& documentId, velocypack::Slice doc,
                     OperationOptions const& options) override;
  
  /// @brief returns whether the document can be updated/replaced in the index
  /// (or if there will be a conflict)
  Result checkReplace(transaction::Methods& trx, RocksDBMethods* methods,
                      LocalDocumentId const& documentId, velocypack::Slice doc,
                      OperationOptions const& options) override;

  /// @brief return the number of paths
  inline size_t numPaths() const { return _paths.size(); }

  /// @brief helper function to transform AttributeNames into string lists
  void fillPaths(std::vector<std::vector<std::string>>& paths, std::vector<int>& expanding);

  /// @brief helper function to insert a document into any index type
  ErrorCode fillElement(velocypack::Builder& leased,
                        LocalDocumentId const& documentId, VPackSlice const& doc,
                        ::arangodb::containers::SmallVector<RocksDBKey>& elements,
                        ::arangodb::containers::SmallVector<uint64_t>& hashes);

  /// @brief helper function to build the key and value for rocksdb from the
  /// vector of slices
  /// @param hashes list of VPackSlice hashes for the estimator.
  void addIndexValue(velocypack::Builder& leased,
                     LocalDocumentId const& documentId, VPackSlice const& document,
                     ::arangodb::containers::SmallVector<RocksDBKey>& elements,
                     ::arangodb::containers::SmallVector<uint64_t>& hashes,
                     ::arangodb::containers::SmallVector<VPackSlice>& sliceStack);

  /// @brief helper function to create a set of value combinations to insert
  /// into the rocksdb index.
  /// @param elements vector of resulting index entries
  /// @param sliceStack working list of values to insert into the index
  /// @param hashes list of VPackSlice hashes for the estimator.
  void buildIndexValues(velocypack::Builder& leased, LocalDocumentId const& documentId,
                        VPackSlice const document, size_t level,
                        ::arangodb::containers::SmallVector<RocksDBKey>& elements,
                        ::arangodb::containers::SmallVector<uint64_t>& hashes,
                        ::arangodb::containers::SmallVector<VPackSlice>& sliceStack);

 private:
  /// @brief the attribute paths
  std::vector<std::vector<std::string>> _paths;

  /// @brief ... and which of them expands
  std::vector<int> _expanding;

  /// @brief whether or not array indexes will de-duplicate their input values
  bool _deduplicate;

  /// @brief whether or not partial indexing is allowed
  bool _allowPartialIndex;

  /// @brief whether or not we want to have estimates
  bool _estimates;

  /// @brief A fixed size library to estimate the selectivity of the index.
  /// On insertion of a document we have to insert it into the estimator,
  /// On removal we have to remove it in the estimator as well.
  std::unique_ptr<RocksDBCuckooIndexEstimatorType> _estimator;
};
}  // namespace arangodb

#endif
