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
#include "Random/RandomGenerator.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBToken.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace rocksdb {
class Iterator;
}

namespace arangodb {

class RocksDBPrimaryIndex;
namespace transaction {
class Methods;
}

class RocksDBPrimaryIndexIterator final : public IndexIterator {
 public:
  RocksDBPrimaryIndexIterator(LogicalCollection* collection,
                              transaction::Methods* trx,
                              ManagedDocumentResult* mmdr,
                              RocksDBPrimaryIndex* index,
                              std::unique_ptr<VPackBuilder>& keys);

  ~RocksDBPrimaryIndexIterator();

  char const* typeName() const override { return "primary-index-iterator"; }

  bool next(TokenCallback const& cb, size_t limit) override;

  void reset() override;

 private:
  RocksDBPrimaryIndex* _index;
  std::unique_ptr<VPackBuilder> _keys;
  arangodb::velocypack::ArrayIterator _iterator;
};

class RocksDBAllIndexIterator final : public IndexIterator {
 public:
  typedef std::function<void(DocumentIdentifierToken const& token,
                             StringRef const& key)>
      TokenKeyCallback;
  RocksDBAllIndexIterator(LogicalCollection* collection,
                          transaction::Methods* trx,
                          ManagedDocumentResult* mmdr,
                          RocksDBPrimaryIndex const* index, bool reverse);

  ~RocksDBAllIndexIterator() {}

  char const* typeName() const override { return "all-index-iterator"; }

  bool next(TokenCallback const& cb, size_t limit) override;
  void reset() override;

  // engine specific optimizations
  bool nextWithKey(TokenKeyCallback const& cb, size_t limit);
  void seek(StringRef const& key);

 private:
  bool const _reverse;
  std::unique_ptr<rocksdb::Iterator> _iterator;
  RocksDBKeyBounds _bounds;
};

class RocksDBAnyIndexIterator final : public IndexIterator {
 public:
  RocksDBAnyIndexIterator(LogicalCollection* collection,
                          transaction::Methods* trx,
                          ManagedDocumentResult* mmdr,
                          RocksDBPrimaryIndex const* index);

  ~RocksDBAnyIndexIterator() {}

  char const* typeName() const override { return "any-index-iterator"; }

  bool next(TokenCallback const& cb, size_t limit) override;

  void reset() override;

 private:
  bool outOfRange() const;
  static uint64_t newOffset(LogicalCollection* collection,
                            transaction::Methods* trx);

  RocksDBComparator const* _cmp;
  std::unique_ptr<rocksdb::Iterator> _iterator;
  RocksDBKeyBounds _bounds;
  uint64_t _total;
  uint64_t _returned;
};

class RocksDBPrimaryIndex final : public RocksDBIndex {
  friend class RocksDBPrimaryIndexIterator;
  friend class RocksDBAllIndexIterator;
  friend class RocksDBAnyIndexIterator;

 public:
  RocksDBPrimaryIndex() = delete;

  explicit RocksDBPrimaryIndex(arangodb::LogicalCollection*,
                               VPackSlice const& info);

  ~RocksDBPrimaryIndex();

 public:
  IndexType type() const override { return Index::TRI_IDX_TYPE_PRIMARY_INDEX; }

  char const* typeName() const override { return "primary"; }

  bool allowExpansion() const override { return false; }

  bool canBeDropped() const override { return false; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimate(
      arangodb::StringRef const* = nullptr) const override {
    return 1.0;
  }

  size_t size() const;

  size_t memory() const override;

  void toVelocyPack(VPackBuilder&, bool, bool) const override;
  void toVelocyPackFigures(VPackBuilder&) const override;

  RocksDBToken lookupKey(transaction::Methods* trx,
                         arangodb::StringRef key) const;
  RocksDBToken lookupKey(transaction::Methods* trx,
                         arangodb::velocypack::Slice key,
                         ManagedDocumentResult& result) const;

  int insert(transaction::Methods*, TRI_voc_rid_t,
             arangodb::velocypack::Slice const&, bool isRollback) override;

  int insertRaw(rocksdb::WriteBatchWithIndex*, TRI_voc_rid_t,
                arangodb::velocypack::Slice const&) override;

  int remove(transaction::Methods*, TRI_voc_rid_t,
             arangodb::velocypack::Slice const&, bool isRollback) override;

  /// optimization for truncateNoTrx, never called in fillIndex
  int removeRaw(rocksdb::WriteBatchWithIndex*, TRI_voc_rid_t,
                arangodb::velocypack::Slice const&) override;

  int drop() override;

  bool supportsFilterCondition(arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  IndexIterator* iteratorForCondition(transaction::Methods*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      bool) override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

  /// @brief request an iterator over all elements in the index in
  ///        a sequential order.
  IndexIterator* allIterator(transaction::Methods*, ManagedDocumentResult*,
                             bool reverse) const;

  IndexIterator* anyIterator(transaction::Methods* trx,
                             ManagedDocumentResult* mmdr) const;

  void invokeOnAllElements(
      transaction::Methods* trx,
      std::function<bool(DocumentIdentifierToken const&)> callback) const;

  int cleanup() override;

 protected:
  Result postprocessRemove(transaction::Methods* trx, rocksdb::Slice const& key,
                           rocksdb::Slice const& value) override;

 private:
  /// @brief create the iterator, for a single attribute, IN operator
  IndexIterator* createInIterator(transaction::Methods*, ManagedDocumentResult*,
                                  arangodb::aql::AstNode const*,
                                  arangodb::aql::AstNode const*);

  /// @brief create the iterator, for a single attribute, EQ operator
  IndexIterator* createEqIterator(transaction::Methods*, ManagedDocumentResult*,
                                  arangodb::aql::AstNode const*,
                                  arangodb::aql::AstNode const*);

  /// @brief add a single value node to the iterator's keys
  void handleValNode(transaction::Methods* trx, VPackBuilder* keys,
                     arangodb::aql::AstNode const* valNode, bool isId) const;
};
}  // namespace arangodb

#endif
