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
#include "Indexes/IndexIterator.h"
#include "MMFiles/MMFilesIndexLookupContext.h"
#include "MMFiles/MMFilesIndex.h"
#include "MMFiles/MMFilesIndexElement.h"
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

struct MMFilesPrimaryIndexHelper {
  static inline uint64_t HashKey(uint8_t const* key) {
    return MMFilesSimpleIndexElement::hash(VPackSlice(key));
  }

  static inline uint64_t HashElement(MMFilesSimpleIndexElement const& element, bool) {
    return element.hash();
  }

  /// @brief determines if a key corresponds to an element
  inline bool IsEqualKeyElement(void* userData, uint8_t const* key,
                                MMFilesSimpleIndexElement const& right) const {
    MMFilesIndexLookupContext* context = static_cast<MMFilesIndexLookupContext*>(userData);
    TRI_ASSERT(context != nullptr);

    try {
      VPackSlice tmp = right.slice(context);
      TRI_ASSERT(tmp.isString());
      return VPackSlice(key).equals(tmp);
    } catch (...) {
      return false;
    }
  }

  /// @brief determines if two elements are equal
  inline bool IsEqualElementElement(void*,
                                    MMFilesSimpleIndexElement const& left,
                                    MMFilesSimpleIndexElement const& right) const {
    return (left.localDocumentId() == right.localDocumentId());
  }

  inline bool IsEqualElementElementByKey(void* userData,
                                         MMFilesSimpleIndexElement const& left,
                                         MMFilesSimpleIndexElement const& right) const {
    if (left.hash() != right.hash()) {
      // TODO: check if we have many collisions here
      return false;
    }
    MMFilesIndexLookupContext* context = static_cast<MMFilesIndexLookupContext*>(userData);
    TRI_ASSERT(context != nullptr);

    VPackSlice l = left.slice(context);
    VPackSlice r = right.slice(context);
    TRI_ASSERT(l.isString());
    TRI_ASSERT(r.isString());
    return l.equals(r);
  }
};

typedef arangodb::basics::AssocUnique<uint8_t, MMFilesSimpleIndexElement, MMFilesPrimaryIndexHelper>
    MMFilesPrimaryIndexImpl;

class MMFilesPrimaryIndexEqIterator final : public IndexIterator {
 public:
  MMFilesPrimaryIndexEqIterator(LogicalCollection* collection,
                                transaction::Methods* trx,
                                MMFilesPrimaryIndex const* index,
                                std::unique_ptr<VPackBuilder> keys);

  ~MMFilesPrimaryIndexEqIterator();

  char const* typeName() const override { return "primary-index-eq-iterator"; }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override;
  
  bool nextDocument(DocumentCallback const& cb, size_t limit) override;

  void reset() override;

 private:
  MMFilesPrimaryIndex const* _index;
  std::unique_ptr<VPackBuilder> _key;
  bool _done;
};

class MMFilesPrimaryIndexInIterator final : public IndexIterator {
 public:
  MMFilesPrimaryIndexInIterator(LogicalCollection* collection,
                                transaction::Methods* trx,
                                MMFilesPrimaryIndex const* index,
                                std::unique_ptr<VPackBuilder> keys);

  ~MMFilesPrimaryIndexInIterator();

  char const* typeName() const override { return "primary-index-in-iterator"; }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override;

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
                          MMFilesPrimaryIndex const* index,
                          MMFilesPrimaryIndexImpl const* indexImpl);

  ~MMFilesAllIndexIterator() {}

  char const* typeName() const override { return "all-index-iterator"; }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override;
  bool nextDocument(DocumentCallback const& cb, size_t limit) override;

  void skip(uint64_t count, uint64_t& skipped) override;

  void reset() override;

 private:
  MMFilesPrimaryIndexImpl const* _index;
  arangodb::basics::BucketPosition _position;
  std::vector<std::pair<LocalDocumentId, uint8_t const*>> _documentIds;
  uint64_t _total;
};

class MMFilesAnyIndexIterator final : public IndexIterator {
 public:
  MMFilesAnyIndexIterator(LogicalCollection* collection,
                          transaction::Methods* trx,
                          MMFilesPrimaryIndex const* index,
                          MMFilesPrimaryIndexImpl const* indexImpl);

  ~MMFilesAnyIndexIterator() {}

  char const* typeName() const override { return "any-index-iterator"; }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override;

  void reset() override;

 private:
  MMFilesPrimaryIndexImpl const* _index;
  arangodb::basics::BucketPosition _initial;
  arangodb::basics::BucketPosition _position;
  uint64_t _step;
  uint64_t _total;
};

class MMFilesPrimaryIndex final : public MMFilesIndex {
  friend class MMFilesPrimaryIndexIterator;

 public:
  MMFilesPrimaryIndex() = delete;

  explicit MMFilesPrimaryIndex(arangodb::LogicalCollection& collection);

  IndexType type() const override { return Index::TRI_IDX_TYPE_PRIMARY_INDEX; }

  char const* typeName() const override { return "primary"; }

  bool canBeDropped() const override { return false; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimate(StringRef const& = StringRef()) const override {
    return 1.0;
  }

  size_t size() const;

  size_t memory() const override;

  void toVelocyPack(VPackBuilder&,
                    std::underlying_type<Index::Serialize>::type) const override;
  void toVelocyPackFigures(VPackBuilder&) const override;

  Result insert(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
  ) override;

  Result remove(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    arangodb::velocypack::Slice const& doc,
    Index::OperationMode mode
  ) override;

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
  IndexIterator* allIterator(transaction::Methods*) const;

  /// @brief request an iterator over all elements in the index in
  ///        a random order. It is guaranteed that each element is found
  ///        exactly once unless the collection is modified.
  IndexIterator* anyIterator(transaction::Methods*) const;

  /// @brief a method to iterate over all elements in the index in
  ///        reversed sequential order.
  ///        Returns nullptr if all documents have been returned.
  ///        Convention: position === UINT64_MAX indicates a new start.
  ///        DEPRECATED
  MMFilesSimpleIndexElement lookupSequentialReverse(
      transaction::Methods*, arangodb::basics::BucketPosition& position);

  Result insertKey(transaction::Methods*, LocalDocumentId const& documentId,
                   arangodb::velocypack::Slice const&, OperationMode mode);
  Result insertKey(transaction::Methods*, LocalDocumentId const& documentId,
                   arangodb::velocypack::Slice const&, ManagedDocumentResult&,
                   OperationMode mode);

  Result removeKey(transaction::Methods*, LocalDocumentId const& documentId,
                   arangodb::velocypack::Slice const&, OperationMode mode);
  Result removeKey(transaction::Methods*, LocalDocumentId const& documentId,
                   arangodb::velocypack::Slice const&, ManagedDocumentResult&,
                   OperationMode mode);

  int resize(transaction::Methods*, size_t);

  void invokeOnAllElements(std::function<bool(LocalDocumentId const&)>);
  void invokeOnAllElementsForRemoval(
      std::function<bool(MMFilesSimpleIndexElement const&)>);

  bool supportsFilterCondition(std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
                               arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  IndexIterator* iteratorForCondition(transaction::Methods*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      IndexIteratorOptions const&) override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

 private:
  /// @brief create the iterator, for a single attribute, IN operator
  IndexIterator* createInIterator(transaction::Methods*,
                                  arangodb::aql::AstNode const*,
                                  arangodb::aql::AstNode const*) const;

  /// @brief create the iterator, for a single attribute, EQ operator
  IndexIterator* createEqIterator(transaction::Methods*,
                                  arangodb::aql::AstNode const*,
                                  arangodb::aql::AstNode const*) const;

  /// @brief add a single value node to the iterator's keys
  void handleValNode(transaction::Methods* trx, VPackBuilder* keys,
                     arangodb::aql::AstNode const* valNode, bool isId) const;

  MMFilesSimpleIndexElement buildKeyElement(
      LocalDocumentId const& documentId, arangodb::velocypack::Slice const&) const;

 private:
  /// @brief the actual index
  std::unique_ptr<MMFilesPrimaryIndexImpl> _primaryIndex;
};
}

#endif