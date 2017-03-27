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

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_PRIMARY_INDEX_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_PRIMARY_INDEX_H 1

#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <mutex>
#include <unordered_map>

namespace arangodb {

class RocksDBPrimaryMockIndex;
namespace transaction {
class Methods;
}
  
class RocksDBPrimaryMockIndexIterator final : public IndexIterator {
 public:
  RocksDBPrimaryMockIndexIterator(LogicalCollection* collection,
                       transaction::Methods* trx, 
                       ManagedDocumentResult* mmdr,
                       RocksDBPrimaryMockIndex const* index,
                       std::unique_ptr<VPackBuilder>& keys);

  ~RocksDBPrimaryMockIndexIterator();
    
  char const* typeName() const override { return "primary-index-iterator"; }

  bool next(TokenCallback const& cb, size_t limit) override;

  void reset() override;

 private:
  RocksDBPrimaryMockIndex const* _index;
  std::unique_ptr<VPackBuilder> _keys;
  arangodb::velocypack::ArrayIterator _iterator;
};

class RocksDBAllIndexIterator final : public IndexIterator {
 public:
  RocksDBAllIndexIterator(LogicalCollection* collection,
                   transaction::Methods* trx, 
                   ManagedDocumentResult* mmdr,
                   RocksDBPrimaryMockIndex const* index,
                   bool reverse);

  ~RocksDBAllIndexIterator() {}
    
  char const* typeName() const override { return "all-index-iterator"; }

  bool next(TokenCallback const& cb, size_t limit) override;

  void reset() override;

 private:
  bool const _reverse;
  uint64_t _total;
};

class RocksDBAnyIndexIterator final : public IndexIterator {
 public:
  RocksDBAnyIndexIterator(LogicalCollection* collection, transaction::Methods* trx, 
                   ManagedDocumentResult* mmdr,
                   RocksDBPrimaryMockIndex const* index);

  ~RocksDBAnyIndexIterator() {}
  
  char const* typeName() const override { return "any-index-iterator"; }

  bool next(TokenCallback const& cb, size_t limit) override;

  void reset() override;
};

class RocksDBPrimaryMockIndex final : public Index {
  friend class RocksDBPrimaryMockIndexIterator;

 public:
  RocksDBPrimaryMockIndex() = delete;

  explicit RocksDBPrimaryMockIndex(arangodb::LogicalCollection*, VPackSlice const& info);

  ~RocksDBPrimaryMockIndex();

 public:
  IndexType type() const override {
    return Index::TRI_IDX_TYPE_PRIMARY_INDEX;
  }
  
  char const* typeName() const override { return "primary"; }

  bool allowExpansion() const override { return false; }

  bool canBeDropped() const override { return false; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimate(arangodb::StringRef const* = nullptr) const override { return 1.0; }

  size_t size() const;

  size_t memory() const override;

  void toVelocyPack(VPackBuilder&, bool) const override;
  void toVelocyPackFigures(VPackBuilder&) const override;

  int insert(transaction::Methods*, TRI_voc_rid_t, arangodb::velocypack::Slice const&, bool isRollback) override;

  int remove(transaction::Methods*, TRI_voc_rid_t, arangodb::velocypack::Slice const&, bool isRollback) override;

  int unload() override;
 
  bool supportsFilterCondition(arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  IndexIterator* iteratorForCondition(transaction::Methods*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      bool) const override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;
  
  /// @brief request an iterator over all elements in the index in
  ///        a sequential order.
  IndexIterator* allIterator(transaction::Methods*, ManagedDocumentResult*, bool reverse) const;

  /// @brief request an iterator over all elements in the index in
  ///        a random order. It is guaranteed that each element is found
  ///        exactly once unless the collection is modified.
  IndexIterator* anyIterator(transaction::Methods*, ManagedDocumentResult*) const;
  uint64_t objectId() const { return _objectId; }
 private:
  uint64_t _objectId;
  std::map<std::string const,TRI_voc_rid_t const> _keyRevMap;
  std::mutex _keyRevMutex;
};
}

#endif
