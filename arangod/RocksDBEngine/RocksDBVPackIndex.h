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
/// @author Jan Steemann
/// @author Daniel H. Larkin
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ROCKSDB_VPACK_INDEX_H
#define ARANGOD_ROCKSDB_ROCKSDB_VPACK_INDEX_H 1

#include "Aql/AstNode.h"
#include "Basics/Common.h"
#include "Basics/SmallVector.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <rocksdb/comparator.h>
#include <rocksdb/iterator.h>
#include <velocypack/Buffer.h>
#include <velocypack/StringRef.h>
#include <velocypack/Slice.h>

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
  friend class RocksDBVPackIndexIterator;

 public:
  static uint64_t HashForKey(const rocksdb::Slice& key);

  RocksDBVPackIndex() = delete;

  RocksDBVPackIndex(TRI_idx_iid_t iid, LogicalCollection& collection,
                    arangodb::velocypack::Slice const& info);

  ~RocksDBVPackIndex();

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimate(arangodb::velocypack::StringRef const& = arangodb::velocypack::StringRef()) const override;

  RocksDBCuckooIndexEstimator<uint64_t>* estimator() override;
  void setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>>) override;
  void recalculateEstimates() override;

  void toVelocyPack(VPackBuilder&, std::underlying_type<Index::Serialize>::type) const override;

  bool canBeDropped() const override { return true; }

  bool hasCoveringIterator() const override { return true; }

  /// @brief return the attribute paths
  std::vector<std::vector<std::string>> const& paths() const { return _paths; }

  /// @brief return the attribute paths, a -1 entry means none is expanding,
  /// otherwise the non-negative number is the index of the expanding one.
  std::vector<int> const& expanding() const { return _expanding; }

  static constexpr size_t minimalPrefixSize() { return sizeof(TRI_voc_tick_t); }

  /// @brief attempts to locate an entry in the index
  ///
  /// Warning: who ever calls this function is responsible for destroying
  /// the velocypack::Slice and the RocksDBVPackIndexIterator* results
  IndexIterator* lookup(transaction::Methods*,
                        arangodb::velocypack::Slice const, bool reverse) const;

  bool supportsFilterCondition(std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
                               arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  bool supportsSortCondition(arangodb::aql::SortCondition const*,
                             arangodb::aql::Variable const*, size_t, double&,
                             size_t&) const override;

  arangodb::aql::AstNode* specializeCondition(arangodb::aql::AstNode*,
                                              arangodb::aql::Variable const*) const override;

  IndexIterator* iteratorForCondition(transaction::Methods*, 
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      IndexIteratorOptions const&) override;

  void afterTruncate(TRI_voc_tick_t tick) override;

 protected:
  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& documentId,
                velocypack::Slice const& doc, Index::OperationMode mode) override;

  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& documentId,
                velocypack::Slice const& doc, Index::OperationMode mode) override;

  Result update(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& oldDocumentId,
                velocypack::Slice const& oldDoc, LocalDocumentId const& newDocumentId,
                velocypack::Slice const& newDoc, Index::OperationMode mode) override;

 private:
  /// @brief return the number of paths
  inline size_t numPaths() const { return _paths.size(); }

  /// @brief helper function to transform AttributeNames into string lists
  void fillPaths(std::vector<std::vector<std::string>>& paths, std::vector<int>& expanding);

  /// @brief helper function to insert a document into any index type
  int fillElement(velocypack::Builder& leased, LocalDocumentId const& documentId,
                  VPackSlice const& doc, SmallVector<RocksDBKey>& elements,
                  SmallVector<uint64_t>& hashes);

  /// @brief helper function to build the key and value for rocksdb from the
  /// vector of slices
  /// @param hashes list of VPackSlice hashes for the estimator.
  void addIndexValue(velocypack::Builder& leased, LocalDocumentId const& documentId,
                     VPackSlice const& document, SmallVector<RocksDBKey>& elements,
                     SmallVector<uint64_t>& hashes, SmallVector<VPackSlice>& sliceStack);

  /// @brief helper function to create a set of value combinations to insert
  /// into the rocksdb index.
  /// @param elements vector of resulting index entries
  /// @param sliceStack working list of values to insert into the index
  /// @param hashes list of VPackSlice hashes for the estimator.
  void buildIndexValues(velocypack::Builder& leased, LocalDocumentId const& documentId,
                        VPackSlice const document, size_t level,
                        SmallVector<RocksDBKey>& elements, SmallVector<uint64_t>& hashes,
                        SmallVector<VPackSlice>& sliceStack);

 private:
  /// @brief the attribute paths
  std::vector<std::vector<std::string>> _paths;

  /// @brief ... and which of them expands
  std::vector<int> _expanding;

  /// @brief whether or not array indexes will de-duplicate their input values
  bool _deduplicate;

  /// @brief whether or not partial indexing is allowed
  bool _allowPartialIndex;

  /// @brief A fixed size library to estimate the selectivity of the index.
  /// On insertion of a document we have to insert it into the estimator,
  /// On removal we have to remove it in the estimator as well.
  std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>> _estimator;
};
}  // namespace arangodb

#endif
