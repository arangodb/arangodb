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
enum AstNodeType : uint32_t;
}
class FixedSizeAllocator;
class LogicalCollection;
class RocksDBComparator;
class RocksDBPrimaryIndex;
class RocksDBVPackIndex;
namespace transaction {
class Methods;
}

/// @brief Iterator structure for RocksDB. We require a start and stop node
class RocksDBVPackIndexIterator final : public IndexIterator {
 private:
  friend class RocksDBVPackIndex;

 public:
  RocksDBVPackIndexIterator(LogicalCollection* collection,
                            transaction::Methods* trx,
                            ManagedDocumentResult* mmdr,
                            arangodb::RocksDBVPackIndex const* index,
                            arangodb::RocksDBPrimaryIndex* primaryIndex,
                            bool reverse,
                            arangodb::velocypack::Slice const& left,
                            arangodb::velocypack::Slice const& right);

  ~RocksDBVPackIndexIterator() = default;

 public:
  char const* typeName() const override {
    return "rocksdb-unique-index-iterator";
  }

  /// @brief Get the next limit many element in the index
  bool next(TokenCallback const& cb, size_t limit) override;

  /// @brief Reset the cursor
  void reset() override;

 private:
  bool outOfRange() const;

  arangodb::RocksDBVPackIndex const* _index;
  arangodb::RocksDBPrimaryIndex* _primaryIndex;
  arangodb::RocksDBComparator const* _cmp;
  std::unique_ptr<rocksdb::Iterator> _iterator;
  bool const _reverse;
  RocksDBKeyBounds _bounds;
};

class RocksDBVPackIndex : public RocksDBIndex {
  friend class RocksDBVPackIndexIterator;

 public:
  RocksDBVPackIndex() = delete;

  RocksDBVPackIndex(TRI_idx_iid_t, LogicalCollection*,
                    arangodb::velocypack::Slice const&);

  virtual ~RocksDBVPackIndex();

 public:
  bool hasSelectivityEstimate() const override { return _unique && true; }

  double selectivityEstimate(
      arangodb::StringRef const* = nullptr) const override {
    return 1.0;  // only valid if unique
  }

  size_t memory() const override;

  void toVelocyPack(VPackBuilder&, bool) const override;
  void toVelocyPackFigures(VPackBuilder&) const override;

  bool allowExpansion() const override { return true; }

  bool canBeDropped() const override { return true; }

  /// @brief return the attribute paths
  std::vector<std::vector<std::string>> const& paths() const { return _paths; }

  /// @brief return the attribute paths, a -1 entry means none is expanding,
  /// otherwise the non-negative number is the index of the expanding one.
  std::vector<int> const& expanding() const { return _expanding; }

  bool implicitlyUnique() const override;

  static constexpr size_t minimalPrefixSize() { return sizeof(TRI_voc_tick_t); }

  int insert(transaction::Methods*, TRI_voc_rid_t,
             arangodb::velocypack::Slice const&, bool isRollback) override;

  int remove(transaction::Methods*, TRI_voc_rid_t,
             arangodb::velocypack::Slice const&, bool isRollback) override;

  int unload() override;

  int drop() override;

  /// @brief attempts to locate an entry in the index
  ///
  /// Warning: who ever calls this function is responsible for destroying
  /// the velocypack::Slice and the RocksDBVPackIndexIterator* results
  RocksDBVPackIndexIterator* lookup(transaction::Methods*,
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
                                      bool) const override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

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

 protected:
  /// @brief helper function to insert a document into any index type
  int fillElement(transaction::Methods* trx, TRI_voc_rid_t revisionId,
                  VPackSlice const& doc,
                  std::vector<std::pair<RocksDBKey, RocksDBValue>>& elements);

  /// @brief return the number of paths
  inline size_t numPaths() const { return _paths.size(); }

 private:
  /// @brief helper function to transform AttributeNames into string lists
  void fillPaths(std::vector<std::vector<std::string>>& paths,
                 std::vector<int>& expanding);

  /// @brief helper function to create a set of index combinations to insert
  std::vector<std::pair<VPackSlice, uint32_t>> buildIndexValue(
      VPackSlice const documentSlice);

  void addIndexValue(VPackSlice const& document,
                     std::vector<std::pair<RocksDBKey, RocksDBValue>>& elements,
                     std::vector<VPackSlice>& sliceStack);

  /// @brief helper function to create a set of index combinations to insert
  void buildIndexValues(
      VPackSlice const document, size_t level,
      std::vector<std::pair<RocksDBKey, RocksDBValue>>& elements,
      std::vector<VPackSlice>& sliceStack);

 private:
  std::unique_ptr<FixedSizeAllocator> _allocator;

  /// @brief the attribute paths
  std::vector<std::vector<std::string>> _paths;

  /// @brief ... and which of them expands
  std::vector<int> _expanding;

  /// @brief whether or not at least one attribute is expanded
  bool _useExpansion;

  /// @brief whether or not partial indexing is allowed
  bool _allowPartialIndex;
};
}

#endif
