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

#ifndef ARANGOD_MMFILES_HASH_INDEX_H
#define ARANGOD_MMFILES_HASH_INDEX_H 1

#include "Basics/AssocMulti.h"
#include "Basics/AssocUnique.h"
#include "Basics/Common.h"
#include "Basics/SmallVector.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/fasthash.h"
#include "Indexes/IndexIterator.h"
#include "MMFiles/MMFilesIndexLookupContext.h"
#include "MMFiles/MMFilesIndexElement.h"
#include "MMFiles/MMFilesPathBasedIndex.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

/// @brief hash index query parameter
namespace arangodb {
namespace basics {
class LocalTaskQueue;
}

class MMFilesHashIndex;

struct MMFilesHashIndexHelper {
  MMFilesHashIndexHelper(size_t n, bool allowExpansion)
    : _numFields(n), _allowExpansion(allowExpansion) {}

  static inline uint64_t HashKey(VPackSlice const* key) {
    return MMFilesHashIndexElement::hash(*key);
  }

  static inline uint64_t HashElement(MMFilesHashIndexElement const* element, bool byKey) {
    uint64_t hash = element->hash();

    if (byKey) {
      return hash;
    }

    uint64_t documentId = element->localDocumentIdValue();
    return fasthash64_uint64(documentId, hash);
  }

  /// @brief determines if a key corresponds to an element
  inline bool IsEqualKeyElement(void* userData, VPackSlice const* left,
                                MMFilesHashIndexElement const* right) const {
    TRI_ASSERT(left->isArray());
    TRI_ASSERT(right->isSet());
    MMFilesIndexLookupContext* context = static_cast<MMFilesIndexLookupContext*>(userData);
    TRI_ASSERT(context != nullptr);

    // TODO: is it a performance improvement to compare the hash values first?
    VPackArrayIterator it(*left);

    while (it.valid()) {
      int res = arangodb::basics::VelocyPackHelper::compare(it.value(), right->slice(context, it.index()), false);

      if (res != 0) {
        return false;
      }

      it.next();
    }

    return true;
  }

  inline bool IsEqualElementElementByKey(void* userData,
                                         MMFilesHashIndexElement const* left,
                                         MMFilesHashIndexElement const* right) const {
    TRI_ASSERT(left->isSet());
    TRI_ASSERT(right->isSet());

    if (!_allowExpansion && left->localDocumentId() == right->localDocumentId()) {
      return true;
    }

    MMFilesIndexLookupContext* context = static_cast<MMFilesIndexLookupContext*>(userData);

    for (size_t i = 0; i < _numFields; ++i) {
      VPackSlice leftData = left->slice(context, i);
      VPackSlice rightData = right->slice(context, i);

      int res = arangodb::basics::VelocyPackHelper::compare(leftData,
                                                            rightData, false);

      if (res != 0) {
        return false;
      }
    }

    return true;
  }

  size_t const _numFields;
  bool const _allowExpansion;
};

struct MMFilesUniqueHashIndexHelper : public MMFilesHashIndexHelper {
  MMFilesUniqueHashIndexHelper(size_t n, bool allowExpansion) : MMFilesHashIndexHelper(n, allowExpansion) {}

  /// @brief determines if two elements are equal
  inline bool IsEqualElementElement(void*,
                                    MMFilesHashIndexElement const* left,
                                    MMFilesHashIndexElement const* right) const {
    // this is quite simple
    return left->localDocumentId() == right->localDocumentId();
  }
};

struct MMFilesMultiHashIndexHelper : public MMFilesHashIndexHelper {
  MMFilesMultiHashIndexHelper(size_t n, bool allowExpansion) : MMFilesHashIndexHelper(n, allowExpansion) {}

  /// @brief determines if two elements are equal
  inline bool IsEqualElementElement(void* userData,
                                    MMFilesHashIndexElement const* left,
                                    MMFilesHashIndexElement const* right) const {
    TRI_ASSERT(left != nullptr);
    TRI_ASSERT(right != nullptr);

    if (left->localDocumentId() != right->localDocumentId()) {
      return false;
    }
    if (left->hash() != right->hash()) {
      return false;
    }

    MMFilesIndexLookupContext* context = static_cast<MMFilesIndexLookupContext*>(userData);
    TRI_ASSERT(context != nullptr);

    for (size_t i = 0; i < context->numFields(); ++i) {
      VPackSlice leftData = left->slice(context, i);
      VPackSlice rightData = right->slice(context, i);

      int res =
          arangodb::basics::VelocyPackHelper::compare(leftData, rightData, false);

      if (res != 0) {
        return false;
      }
    }

    return true;
  }
};

/// @brief Class to build Slice lookups out of AST Conditions
class MMFilesHashIndexLookupBuilder {
 private:
  transaction::BuilderLeaser _builder;
  bool _usesIn;
  bool _isEmpty;
  size_t _coveredFields;
  
  SmallVector<arangodb::aql::AstNode const*> _mappingFieldCondition;
  SmallVector<arangodb::aql::AstNode const*>::allocator_type::arena_type _mappingFieldConditionArena;
  
  std::unordered_map<
      size_t, std::pair<size_t, std::vector<arangodb::velocypack::Slice>>>
      _inPosition;
  transaction::BuilderLeaser _inStorage;

 public:
  MMFilesHashIndexLookupBuilder(
      transaction::Methods*, arangodb::aql::AstNode const*,
      arangodb::aql::Variable const*,
      std::vector<std::vector<arangodb::basics::AttributeName>> const&);

  arangodb::velocypack::Slice lookup();

  bool hasAndGetNext();

  void reset();

 private:
  bool incrementInPosition();
  void buildNextSearchValue();
};

class MMFilesHashIndexIterator final : public IndexIterator {
 public:
  /// @brief Construct an MMFilesHashIndexIterator based on Ast Conditions
  MMFilesHashIndexIterator(LogicalCollection* collection,
                           transaction::Methods* trx,
                           MMFilesHashIndex const* index,
                           arangodb::aql::AstNode const*,
                           arangodb::aql::Variable const*);

  ~MMFilesHashIndexIterator() = default;

  char const* typeName() const override { return "hash-index-iterator"; }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override;
  bool nextDocument(DocumentCallback const& cb, size_t limit) override;

  void reset() override;

 private:
  MMFilesHashIndex const* _index;
  MMFilesHashIndexLookupBuilder _lookups;
  std::vector<MMFilesHashIndexElement*> _buffer;
  size_t _posInBuffer;
  std::vector<std::pair<LocalDocumentId, uint8_t const*>> _documentIds;
};

class MMFilesHashIndex final : public MMFilesPathBasedIndex {
  friend class MMFilesHashIndexIterator;

 public:
  MMFilesHashIndex() = delete;

  MMFilesHashIndex(
    TRI_idx_iid_t iid,
    LogicalCollection& collection,
    arangodb::velocypack::Slice const& info
  );

  ~MMFilesHashIndex();

  IndexType type() const override { return Index::TRI_IDX_TYPE_HASH_INDEX; }

  char const* typeName() const override { return "hash"; }

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimate(arangodb::StringRef const& = arangodb::StringRef()) const override;

  size_t memory() const override;

  void toVelocyPackFigures(VPackBuilder&) const override;

  bool matchesDefinition(VPackSlice const& info) const override;

  void batchInsert(
    transaction::Methods& trx,
    std::vector<std::pair<LocalDocumentId, velocypack::Slice>> const& docs,
    std::shared_ptr<basics::LocalTaskQueue> queue
  ) override;

  Result insert(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
  ) override;

  Result remove(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
  ) override;

  void unload() override;

  Result sizeHint(transaction::Methods& trx, size_t size) override;

  bool hasBatchInsert() const override { return true; }

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
  /// @brief locates entries in the hash index given a velocypack slice
  int lookup(transaction::Methods*, arangodb::velocypack::Slice,
             std::vector<MMFilesHashIndexElement*>&) const;

  Result insertUnique(transaction::Methods*, LocalDocumentId const& documentId,
                      arangodb::velocypack::Slice const&, OperationMode mode);

  void batchInsertUnique(
      transaction::Methods*,
      std::vector<std::pair<LocalDocumentId, arangodb::velocypack::Slice>> const&,
      std::shared_ptr<arangodb::basics::LocalTaskQueue> queue);

  Result insertMulti(transaction::Methods*, LocalDocumentId const& documentId,
                     arangodb::velocypack::Slice const&, OperationMode mode);

  void batchInsertMulti(
      transaction::Methods*,
      std::vector<std::pair<LocalDocumentId, arangodb::velocypack::Slice>> const&,
      std::shared_ptr<arangodb::basics::LocalTaskQueue> queue);

  int removeUniqueElement(transaction::Methods*, MMFilesHashIndexElement*,
                          OperationMode mode);

  int removeMultiElement(transaction::Methods*, MMFilesHashIndexElement*,
                         OperationMode mode);

  bool accessFitsIndex(arangodb::aql::AstNode const* access,
                       arangodb::aql::AstNode const* other,
                       arangodb::aql::Variable const* reference,
                       std::unordered_set<size_t>& found) const;

  /// @brief given an element generates a hash integer
 private:
  /// @brief the actual hash index (unique type)
  typedef arangodb::basics::AssocUnique<arangodb::velocypack::Slice,
                                        MMFilesHashIndexElement*, MMFilesUniqueHashIndexHelper>
      TRI_HashArray_t;

  struct UniqueArray {
    UniqueArray() = delete;
    UniqueArray(size_t numPaths, std::unique_ptr<TRI_HashArray_t>);
    std::unique_ptr<TRI_HashArray_t> _hashArray;    // the hash array itself, unique values
    size_t _numPaths;
  };

  /// @brief the actual hash index (multi type)
  typedef arangodb::basics::AssocMulti<
      arangodb::velocypack::Slice, MMFilesHashIndexElement*, uint32_t, false, MMFilesMultiHashIndexHelper>
      TRI_HashArrayMulti_t;

  struct MultiArray {
    MultiArray() = delete;
    MultiArray(size_t numPaths, std::unique_ptr<TRI_HashArrayMulti_t>);

    std::unique_ptr<TRI_HashArrayMulti_t> _hashArray; // the hash array itself, non-unique values
    size_t _numPaths;
  };

  union {
    UniqueArray* _uniqueArray;
    MultiArray* _multiArray;
  };
};
}

#endif