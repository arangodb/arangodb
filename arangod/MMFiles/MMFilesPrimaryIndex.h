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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_MMFILES_PRIMARY_INDEX_H
#define ARANGOD_MMFILES_PRIMARY_INDEX_H 1

#include "Basics/AssocUnique.h"
#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

class MMFilesPrimaryIndex;
struct MMFilesSimpleIndexElement;
namespace transaction {
class Methods;
}

typedef arangodb::basics::AssocUnique<uint8_t, MMFilesSimpleIndexElement>
    MMFilesPrimaryIndexImpl;

class MMFilesPrimaryIndexIterator final : public IndexIterator {
 public:
  MMFilesPrimaryIndexIterator(LogicalCollection* collection,
                              transaction::Methods* trx,
                              ManagedDocumentResult* mmdr,
                              MMFilesPrimaryIndex const* index,
                              std::unique_ptr<VPackBuilder>& keys);

  ~MMFilesPrimaryIndexIterator();

  char const* typeName() const override { return "primary-index-iterator"; }

  bool next(TokenCallback const& cb, size_t limit) override;

  void reset() override;

 private:
  MMFilesPrimaryIndex const* _index;
  std::unique_ptr<VPackBuilder> _keys;
  arangodb::velocypack::ArrayIterator _iterator;
};

class MMFilesAllIndexIterator final : public IndexIterator {
 public:
  MMFilesAllIndexIterator(LogicalCollection* collection,
                          transaction::Methods* trx,
                          ManagedDocumentResult* mmdr,
                          MMFilesPrimaryIndex const* index,
                          MMFilesPrimaryIndexImpl const* indexImpl,
                          bool reverse);

  ~MMFilesAllIndexIterator() {}

  char const* typeName() const override { return "all-index-iterator"; }

  bool next(TokenCallback const& cb, size_t limit) override;

  void skip(uint64_t count, uint64_t& skipped) override;

  void reset() override;

 private:
  MMFilesPrimaryIndexImpl const* _index;
  arangodb::basics::BucketPosition _position;
  bool const _reverse;
  uint64_t _total;
};

class MMFilesAnyIndexIterator final : public IndexIterator {
 public:
  MMFilesAnyIndexIterator(LogicalCollection* collection,
                          transaction::Methods* trx,
                          ManagedDocumentResult* mmdr,
                          MMFilesPrimaryIndex const* index,
                          MMFilesPrimaryIndexImpl const* indexImpl);

  ~MMFilesAnyIndexIterator() {}

  char const* typeName() const override { return "any-index-iterator"; }

  bool next(TokenCallback const& cb, size_t limit) override;

  void reset() override;

 private:
  MMFilesPrimaryIndexImpl const* _index;
  arangodb::basics::BucketPosition _initial;
  arangodb::basics::BucketPosition _position;
  uint64_t _step;
  uint64_t _total;
};

class MMFilesPrimaryIndex final : public Index {
  friend class MMFilesPrimaryIndexIterator;

 public:
  MMFilesPrimaryIndex() = delete;

  explicit MMFilesPrimaryIndex(arangodb::LogicalCollection*);

  ~MMFilesPrimaryIndex();

 public:
  IndexType type() const override { return Index::TRI_IDX_TYPE_PRIMARY_INDEX; }

  char const* typeName() const override { return "primary"; }

  bool allowExpansion() const override { return false; }

  bool canBeDropped() const override { return false; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimateLocal(
      arangodb::StringRef const* = nullptr) const override {
    return 1.0;
  }

  size_t size() const;

  size_t memory() const override;

  void toVelocyPack(VPackBuilder&, bool withFigures, bool forPersistence) const override;
  void toVelocyPackFigures(VPackBuilder&) const override;

  Result insert(transaction::Methods*, TRI_voc_rid_t,
                arangodb::velocypack::Slice const&, bool isRollback) override;

  Result remove(transaction::Methods*, TRI_voc_rid_t,
                arangodb::velocypack::Slice const&, bool isRollback) override;

  void load() override {}
  void unload() override;

  MMFilesSimpleIndexElement lookupKey(transaction::Methods*,
                                      VPackSlice const&) const;
  MMFilesSimpleIndexElement lookupKey(transaction::Methods*, VPackSlice const&,
                                      ManagedDocumentResult&) const;
  MMFilesSimpleIndexElement* lookupKeyRef(transaction::Methods*,
                                          VPackSlice const&) const;
  MMFilesSimpleIndexElement* lookupKeyRef(transaction::Methods*,
                                          VPackSlice const&,
                                          ManagedDocumentResult&) const;

  /// @brief a method to iterate over all elements in the index in
  ///        a sequential order.
  ///        Returns nullptr if all documents have been returned.
  ///        Convention: position === 0 indicates a new start.
  ///        DEPRECATED
  MMFilesSimpleIndexElement lookupSequential(
      transaction::Methods*, arangodb::basics::BucketPosition& position,
      uint64_t& total);

  /// @brief request an iterator over all elements in the index in
  ///        a sequential order.
  IndexIterator* allIterator(transaction::Methods*, ManagedDocumentResult*,
                             bool reverse) const;

  /// @brief request an iterator over all elements in the index in
  ///        a random order. It is guaranteed that each element is found
  ///        exactly once unless the collection is modified.
  IndexIterator* anyIterator(transaction::Methods*,
                             ManagedDocumentResult*) const;

  /// @brief a method to iterate over all elements in the index in
  ///        reversed sequential order.
  ///        Returns nullptr if all documents have been returned.
  ///        Convention: position === UINT64_MAX indicates a new start.
  ///        DEPRECATED
  MMFilesSimpleIndexElement lookupSequentialReverse(
      transaction::Methods*, arangodb::basics::BucketPosition& position);

  Result insertKey(transaction::Methods*, TRI_voc_rid_t revisionId,
                   arangodb::velocypack::Slice const&);
  Result insertKey(transaction::Methods*, TRI_voc_rid_t revisionId,
                   arangodb::velocypack::Slice const&, ManagedDocumentResult&);

  Result removeKey(transaction::Methods*, TRI_voc_rid_t revisionId,
                   arangodb::velocypack::Slice const&);
  Result removeKey(transaction::Methods*, TRI_voc_rid_t revisionId,
                   arangodb::velocypack::Slice const&, ManagedDocumentResult&);

  int resize(transaction::Methods*, size_t);

  void invokeOnAllElements(std::function<bool(DocumentIdentifierToken const&)>);
  void invokeOnAllElementsForRemoval(
      std::function<bool(MMFilesSimpleIndexElement const&)>);

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

 private:
  /// @brief create the iterator, for a single attribute, IN operator
  IndexIterator* createInIterator(transaction::Methods*, ManagedDocumentResult*,
                                  arangodb::aql::AstNode const*,
                                  arangodb::aql::AstNode const*) const;

  /// @brief create the iterator, for a single attribute, EQ operator
  IndexIterator* createEqIterator(transaction::Methods*, ManagedDocumentResult*,
                                  arangodb::aql::AstNode const*,
                                  arangodb::aql::AstNode const*) const;

  /// @brief add a single value node to the iterator's keys
  void handleValNode(transaction::Methods* trx, VPackBuilder* keys,
                     arangodb::aql::AstNode const* valNode, bool isId) const;

  MMFilesSimpleIndexElement buildKeyElement(
      TRI_voc_rid_t revisionId, arangodb::velocypack::Slice const&) const;

 private:
  /// @brief the actual index
  MMFilesPrimaryIndexImpl* _primaryIndex;
};
}

#endif
