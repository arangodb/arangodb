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
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

namespace transaction {
class Methods;
}

class RocksDBPrimaryIndex final : public RocksDBIndex {
  friend class RocksDBPrimaryIndexEqIterator;
  friend class RocksDBPrimaryIndexRangeIterator;
  friend class RocksDBPrimaryIndexInIterator;
  friend class RocksDBAllIndexIterator;
  friend class RocksDBAnyIndexIterator;

 public:
  RocksDBPrimaryIndex() = delete;

  RocksDBPrimaryIndex(arangodb::LogicalCollection& collection,
                      arangodb::velocypack::Slice const& info);

  ~RocksDBPrimaryIndex();

  IndexType type() const override { return Index::TRI_IDX_TYPE_PRIMARY_INDEX; }

  char const* typeName() const override { return "primary"; }

  bool canBeDropped() const override { return false; }

  bool hasCoveringIterator() const override { return true; }

  bool isSorted() const override { return true; }

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimate(arangodb::velocypack::StringRef const& = arangodb::velocypack::StringRef()) const override {
    return 1.0;
  }

  void load() override;

  void toVelocyPack(VPackBuilder&, std::underlying_type<Index::Serialize>::type) const override;

  LocalDocumentId lookupKey(transaction::Methods* trx,
                            arangodb::velocypack::StringRef key) const;

  /// @brief reads a revision id from the primary index
  /// if the document does not exist, this function will return false
  /// if the document exists, the function will return true
  /// the revision id will only be non-zero if the primary index
  /// value contains the document's revision id. note that this is not
  /// the case for older collections
  /// in this case the caller must fetch the revision id from the actual
  /// document
  bool lookupRevision(transaction::Methods* trx, arangodb::velocypack::StringRef key,
                      LocalDocumentId& id, TRI_voc_rid_t& revisionId) const;

  bool supportsFilterCondition(std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
                               arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;
  
  bool supportsSortCondition(arangodb::aql::SortCondition const*,
                             arangodb::aql::Variable const*, size_t, double&,
                             size_t&) const override;

  IndexIterator* iteratorForCondition(transaction::Methods*, 
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      IndexIteratorOptions const&) override;

  arangodb::aql::AstNode* specializeCondition(arangodb::aql::AstNode*,
                                              arangodb::aql::Variable const*) const override;

  void invokeOnAllElements(transaction::Methods* trx,
                           std::function<bool(LocalDocumentId const&)> callback) const;

  /// insert index elements into the specified write batch.
  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& documentId,
                velocypack::Slice const& doc, Index::OperationMode mode) override;

  /// remove index elements and put it in the specified write batch.
  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& documentId,
                velocypack::Slice const& doc, Index::OperationMode mode) override;

  Result update(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& oldDocumentId,
                velocypack::Slice const& oldDoc, LocalDocumentId const& newDocumentId,
                velocypack::Slice const& newDoc, Index::OperationMode mode) override;

 private:
  /// @brief create the iterator, for a single attribute, IN operator
  IndexIterator* createInIterator(transaction::Methods*, arangodb::aql::AstNode const*,
                                  arangodb::aql::AstNode const*, bool ascending);

  /// @brief create the iterator, for a single attribute, EQ operator
  IndexIterator* createEqIterator(transaction::Methods*, arangodb::aql::AstNode const*,
                                  arangodb::aql::AstNode const*);
  
  /// @brief populate the keys builder with the keys from the array, in either
  /// forward or backward order
  void fillInLookupValues(transaction::Methods*,
                          arangodb::velocypack::Builder& keys,
                          arangodb::aql::AstNode const* values, bool ascending,
                          bool isId) const;

  /// @brief add a single value node to the iterator's keys
  void handleValNode(transaction::Methods* trx, VPackBuilder* keys,
                     arangodb::aql::AstNode const* valNode, bool isId) const;

 private:
  bool const _isRunningInCluster;
};
}  // namespace arangodb

#endif
