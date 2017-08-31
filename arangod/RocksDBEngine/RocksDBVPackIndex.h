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
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>

namespace rocksdb {
class Iterator;
}

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

/// @brief Iterator structure for RocksDB unique index. 
/// This iterator can be used only for equality lookups that use all
/// index attributes. It uses a point lookup and no seeks
class RocksDBVPackUniqueIndexIterator final : public IndexIterator {
 private:
  friend class RocksDBVPackIndex;

 public:
  RocksDBVPackUniqueIndexIterator(LogicalCollection* collection,
                                  transaction::Methods* trx,
                                  ManagedDocumentResult* mmdr,
                                  arangodb::RocksDBVPackIndex const* index,
                                  VPackSlice const& indexValues);

  ~RocksDBVPackUniqueIndexIterator() = default;

 public:
  char const* typeName() const override {
    return "rocksdb-unique-index-iterator";
  }

  /// @brief Get the next limit many element in the index
  bool next(TokenCallback const& cb, size_t limit) override;

  /// @brief Reset the cursor
  void reset() override;

 private:
  arangodb::RocksDBVPackIndex const* _index;
  rocksdb::Comparator const* _cmp;
  RocksDBKeyLeaser _key;
  bool _done;
};

/// @brief Iterator structure for RocksDB. We require a start and stop node
class RocksDBVPackIndexIterator final : public IndexIterator {
 private:
  friend class RocksDBVPackIndex;

 public:
  RocksDBVPackIndexIterator(LogicalCollection* collection,
                            transaction::Methods* trx,
                            ManagedDocumentResult* mmdr,
                            arangodb::RocksDBVPackIndex const* index,
                            bool reverse, 
                            RocksDBKeyBounds&& bounds);

  ~RocksDBVPackIndexIterator() = default;

 public:
  char const* typeName() const override {
    return "rocksdb-index-iterator";
  }

  /// @brief Get the next limit many element in the index
  bool next(TokenCallback const& cb, size_t limit) override;

  /// @brief Reset the cursor
  void reset() override;

 private:
  bool outOfRange() const;

  arangodb::RocksDBVPackIndex const* _index;
  rocksdb::Comparator const* _cmp;
  std::unique_ptr<rocksdb::Iterator> _iterator;
  bool const _reverse;
  RocksDBKeyBounds _bounds;
  rocksdb::Slice _upperBound;  // used for iterate_upper_bound
};

class RocksDBVPackIndex : public RocksDBIndex {
  friend class RocksDBVPackIndexIterator;

 public:
  static uint64_t HashForKey(const rocksdb::Slice& key);

  RocksDBVPackIndex() = delete;

  RocksDBVPackIndex(TRI_idx_iid_t, LogicalCollection*,
                    arangodb::velocypack::Slice const&);

  ~RocksDBVPackIndex();

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimateLocal(
      arangodb::StringRef const* = nullptr) const override;

  void toVelocyPack(VPackBuilder&, bool, bool) const override;

  bool allowExpansion() const override { return true; }

  bool canBeDropped() const override { return true; }

  /// @brief return the attribute paths
  std::vector<std::vector<std::string>> const& paths() const { return _paths; }

  /// @brief return the attribute paths, a -1 entry means none is expanding,
  /// otherwise the non-negative number is the index of the expanding one.
  std::vector<int> const& expanding() const { return _expanding; }

  bool implicitlyUnique() const override;

  static constexpr size_t minimalPrefixSize() { return sizeof(TRI_voc_tick_t); }

  /// @brief attempts to locate an entry in the index
  ///
  /// Warning: who ever calls this function is responsible for destroying
  /// the velocypack::Slice and the RocksDBVPackIndexIterator* results
  IndexIterator* lookup(transaction::Methods*,
                                    ManagedDocumentResult* mmdr,
                                    arangodb::velocypack::Slice const,
                                    bool reverse) const;

  bool supportsFilterCondition(arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  bool supportsSortCondition(arangodb::aql::SortCondition const*,
                             arangodb::aql::Variable const*, size_t, double&,
                             size_t&) const override;

  IndexIterator* iteratorForCondition(transaction::Methods*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      bool) override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

  void serializeEstimate(std::string& output) const override;

  bool deserializeEstimate(arangodb::RocksDBCounterManager* mgr) override;

  void recalculateEstimates() override;

 protected:
  Result insertInternal(transaction::Methods*, RocksDBMethods*, TRI_voc_rid_t,
                        arangodb::velocypack::Slice const&) override;
  
  Result updateInternal(transaction::Methods* trx, RocksDBMethods*,
                        TRI_voc_rid_t oldRevision,
                        arangodb::velocypack::Slice const& oldDoc,
                        TRI_voc_rid_t newRevision,
                        velocypack::Slice const& newDoc) override;

  Result removeInternal(transaction::Methods*, RocksDBMethods*, TRI_voc_rid_t,
                        arangodb::velocypack::Slice const&) override;

  Result postprocessRemove(transaction::Methods* trx, rocksdb::Slice const& key,
                           rocksdb::Slice const& value) override;

 private:
  bool isDuplicateOperator(arangodb::aql::AstNode const*,
                           std::unordered_set<int> const&) const;

  bool accessFitsIndex(
      arangodb::aql::AstNode const*, arangodb::aql::AstNode const*,
      arangodb::aql::AstNode const*, arangodb::aql::Variable const*,
      std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&,
      std::unordered_set<std::string>& nonNullAttributes, bool) const;

  void matchAttributes(
      arangodb::aql::AstNode const*, arangodb::aql::Variable const*,
      std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&,
      size_t& values, std::unordered_set<std::string>& nonNullAttributes,
      bool) const;

 private:
  /// @brief return the number of paths
  inline size_t numPaths() const { return _paths.size(); }

  /// @brief helper function to transform AttributeNames into string lists
  void fillPaths(std::vector<std::vector<std::string>>& paths,
                 std::vector<int>& expanding);

  /// @brief helper function to insert a document into any index type
  int fillElement(velocypack::Builder& leased, TRI_voc_rid_t revisionId,
                  VPackSlice const& doc, std::vector<RocksDBKey>& elements,
                  std::vector<uint64_t>& hashes);

  /// @brief helper function to build the key and value for rocksdb from the
  /// vector of slices
  /// @param hashes list of VPackSlice hashes for the estimator.
  void addIndexValue(velocypack::Builder& leased, TRI_voc_rid_t revisionId,
                     VPackSlice const& document,
                     std::vector<RocksDBKey>& elements,
                     std::vector<VPackSlice>& sliceStack,
                     std::vector<uint64_t>& hashes);

  /// @brief helper function to create a set of value combinations to insert
  /// into the rocksdb index.
  /// @param elements vector of resulting index entries
  /// @param sliceStack working list of values to insert into the index
  /// @param hashes list of VPackSlice hashes for the estimator.
  void buildIndexValues(velocypack::Builder& leased, TRI_voc_rid_t revisionId,
                        VPackSlice const document, size_t level,
                        std::vector<RocksDBKey>& elements,
                        std::vector<VPackSlice>& sliceStack,
                        std::vector<uint64_t>& hashes);

 private:
  /// @brief the attribute paths
  std::vector<std::vector<std::string>> _paths;

  /// @brief ... and which of them expands
  std::vector<int> _expanding;

  /// @brief whether or not array indexes will de-duplicate their input values
  bool _deduplicate;

  /// @brief whether or not at least one attribute is expanded
  bool _useExpansion;

  /// @brief whether or not partial indexing is allowed
  bool _allowPartialIndex;

  /// @brief A fixed size library to estimate the selectivity of the index.
  /// On insertion of a document we have to insert it into the estimator,
  /// On removal we have to remove it in the estimator as well.
  std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>> _estimator;
};
}  // namespace arangodb

#endif
