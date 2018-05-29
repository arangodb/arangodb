////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "VocBase/voc-types.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace rocksdb {
class Iterator;
class Comparator;
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
                              RocksDBPrimaryIndex* index,
                              std::unique_ptr<VPackBuilder> keys,
                              bool allowCoveringIndexOptimization);

  ~RocksDBPrimaryIndexIterator();

  char const* typeName() const override { return "primary-index-iterator"; }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override;

  bool nextCovering(DocumentCallback const& cb, size_t limit) override;

  void reset() override;

  /// @brief we provide a method to provide the index attribute values
  /// while scanning the index
  bool hasCovering() const override { return _allowCoveringIndexOptimization; }

 private:
  RocksDBPrimaryIndex* _index;
  std::unique_ptr<VPackBuilder> _keys;
  arangodb::velocypack::ArrayIterator _iterator;
  bool const _allowCoveringIndexOptimization;
};

class RocksDBPrimaryIndex final : public RocksDBIndex {
  friend class RocksDBPrimaryIndexIterator;
  friend class RocksDBAllIndexIterator;
  friend class RocksDBAnyIndexIterator;

 public:
  RocksDBPrimaryIndex() = delete;

  RocksDBPrimaryIndex(arangodb::LogicalCollection*,
                      VPackSlice const& info);

  ~RocksDBPrimaryIndex();

 public:
  IndexType type() const override { return Index::TRI_IDX_TYPE_PRIMARY_INDEX; }

  char const* typeName() const override { return "primary"; }

  bool canBeDropped() const override { return false; }

  bool hasCoveringIterator() const override { return true; } 

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimate(StringRef const* = nullptr) const override {
    return 1.0;
  }

  void load() override;

  void toVelocyPack(VPackBuilder&, bool, bool) const override;

  LocalDocumentId lookupKey(transaction::Methods* trx,
                         arangodb::StringRef key) const;

  bool supportsFilterCondition(arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  IndexIterator* iteratorForCondition(transaction::Methods*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      IndexIteratorOptions const&) override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

  void invokeOnAllElements(
      transaction::Methods* trx,
      std::function<bool(LocalDocumentId const&)> callback) const;

  /// insert index elements into the specified write batch.
  Result insertInternal(transaction::Methods* trx, RocksDBMethods*,
                        LocalDocumentId const& documentId,
                        arangodb::velocypack::Slice const&,
                        OperationMode mode) override;

  Result updateInternal(transaction::Methods* trx, RocksDBMethods*,
                        LocalDocumentId const& oldDocumentId,
                        arangodb::velocypack::Slice const& oldDoc,
                        LocalDocumentId const& newDocumentId,
                        velocypack::Slice const& newDoc,
                        OperationMode mode) override;

  /// remove index elements and put it in the specified write batch.
  Result removeInternal(transaction::Methods*, RocksDBMethods*,
                        LocalDocumentId const& documentId,
                        arangodb::velocypack::Slice const&,
                        OperationMode mode) override;

 private:
  /// @brief create the iterator, for a single attribute, IN operator
  IndexIterator* createInIterator(transaction::Methods*,
                                  arangodb::aql::AstNode const*,
                                  arangodb::aql::AstNode const*);

  /// @brief create the iterator, for a single attribute, EQ operator
  IndexIterator* createEqIterator(transaction::Methods*,
                                  arangodb::aql::AstNode const*,
                                  arangodb::aql::AstNode const*);

  /// @brief add a single value node to the iterator's keys
  void handleValNode(transaction::Methods* trx, VPackBuilder* keys,
                     arangodb::aql::AstNode const* valNode, bool isId) const;

private:
  bool const _isRunningInCluster;
};
}  // namespace arangodb

#endif
