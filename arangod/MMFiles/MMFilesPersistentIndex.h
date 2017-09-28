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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_MMFILES_PERSISTENT_INDEX_H
#define ARANGOD_MMFILES_PERSISTENT_INDEX_H 1

#include "Aql/AstNode.h"
#include "Basics/Common.h"
#include "Indexes/IndexIterator.h"
#include "MMFiles/MMFilesPathBasedIndex.h"
#include "MMFiles/MMFilesPersistentIndexFeature.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <rocksdb/db.h>
#include <rocksdb/utilities/optimistic_transaction_db.h>

#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>

namespace rocksdb {
class OptimisticTransactionDB;
}

namespace arangodb {
namespace aql {
class SortCondition;
struct Variable;
}

class LogicalCollection;
class MMFilesPrimaryIndex;
class MMFilesPersistentIndex;
namespace transaction {
class Methods;
}

/// @brief Iterator structure for RocksDB. We require a start and stop node
class MMFilesPersistentIndexIterator final : public IndexIterator {
 private:
  friend class MMFilesPersistentIndex;

 public:
  MMFilesPersistentIndexIterator(LogicalCollection* collection,
                                 transaction::Methods* trx,
                                 ManagedDocumentResult* mmdr,
                                 arangodb::MMFilesPersistentIndex const* index,
                                 arangodb::MMFilesPrimaryIndex* primaryIndex,
                                 rocksdb::OptimisticTransactionDB* db,
                                 bool reverse,
                                 arangodb::velocypack::Slice const& left,
                                 arangodb::velocypack::Slice const& right);

  ~MMFilesPersistentIndexIterator() = default;

 public:
  char const* typeName() const override { return "rocksdb-index-iterator"; }

  /// @brief Get the next limit many element in the index
  bool next(TokenCallback const& cb, size_t limit) override;

  /// @brief Reset the cursor
  void reset() override;

 private:
  arangodb::MMFilesPrimaryIndex* _primaryIndex;
  rocksdb::OptimisticTransactionDB* _db;
  std::unique_ptr<rocksdb::Iterator> _cursor;
  std::unique_ptr<arangodb::velocypack::Buffer<char>>
      _leftEndpoint;  // Interval left border
  std::unique_ptr<arangodb::velocypack::Buffer<char>>
      _rightEndpoint;  // Interval right border
  bool const _reverse;
  bool _probe;
};

class MMFilesPersistentIndex final : public MMFilesPathBasedIndex {
  friend class MMFilesPersistentIndexIterator;

 public:
  MMFilesPersistentIndex() = delete;

  MMFilesPersistentIndex(TRI_idx_iid_t, LogicalCollection*,
                         arangodb::velocypack::Slice const&);

  ~MMFilesPersistentIndex();

 public:
  IndexType type() const override {
    return Index::TRI_IDX_TYPE_PERSISTENT_INDEX;
  }

  char const* typeName() const override { return "persistent"; }

  bool allowExpansion() const override { return true; }

  bool isPersistent() const override { return true; }
  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return true; }

  bool hasSelectivityEstimate() const override { return false; }

  size_t memory() const override;

  static constexpr size_t minimalPrefixSize() { return sizeof(TRI_voc_tick_t); }

  static constexpr size_t keyPrefixSize() {
    return sizeof(TRI_voc_tick_t) + sizeof(TRI_voc_cid_t) +
           sizeof(TRI_idx_iid_t);
  }

  static std::string buildPrefix(TRI_voc_tick_t databaseId) {
    std::string value;
    value.append(reinterpret_cast<char const*>(&databaseId),
                 sizeof(TRI_voc_tick_t));
    return value;
  }

  static std::string buildPrefix(TRI_voc_tick_t databaseId,
                                 TRI_voc_cid_t collectionId) {
    std::string value;
    value.append(reinterpret_cast<char const*>(&databaseId),
                 sizeof(TRI_voc_tick_t));
    value.append(reinterpret_cast<char const*>(&collectionId),
                 sizeof(TRI_voc_cid_t));
    return value;
  }

  static std::string buildPrefix(TRI_voc_tick_t databaseId,
                                 TRI_voc_cid_t collectionId,
                                 TRI_idx_iid_t indexId) {
    std::string value;
    value.reserve(keyPrefixSize());
    value.append(reinterpret_cast<char const*>(&databaseId),
                 sizeof(TRI_voc_tick_t));
    value.append(reinterpret_cast<char const*>(&collectionId),
                 sizeof(TRI_voc_cid_t));
    value.append(reinterpret_cast<char const*>(&indexId),
                 sizeof(TRI_idx_iid_t));
    return value;
  }

  Result insert(transaction::Methods*, TRI_voc_rid_t,
                arangodb::velocypack::Slice const&, bool isRollback) override;

  Result remove(transaction::Methods*, TRI_voc_rid_t,
                arangodb::velocypack::Slice const&, bool isRollback) override;

  void unload() override {}

  int drop() override;

  /// @brief attempts to locate an entry in the index
  ///
  /// Warning: who ever calls this function is responsible for destroying
  /// the velocypack::Slice and the MMFilesPersistentIndexIterator* results
  MMFilesPersistentIndexIterator* lookup(transaction::Methods*,
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
};
}

#endif
