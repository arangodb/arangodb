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
#include "Basics/VelocyPackHelper.h"
#include "Basics/fasthash.h"
#include "Indexes/IndexIterator.h"
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

/// @brief Class to build Slice lookups out of AST Conditions
class MMFilesHashIndexLookupBuilder {
 private:
  transaction::BuilderLeaser _builder;
  bool _usesIn;
  bool _isEmpty;
  size_t _coveredFields;
  std::unordered_map<size_t, arangodb::aql::AstNode const*>
      _mappingFieldCondition;
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
                           ManagedDocumentResult* mmdr,
                           MMFilesHashIndex const* index,
                           arangodb::aql::AstNode const*,
                           arangodb::aql::Variable const*);

  ~MMFilesHashIndexIterator() = default;

  char const* typeName() const override { return "hash-index-iterator"; }

  bool next(TokenCallback const& cb, size_t limit) override;

  void reset() override;

 private:
  MMFilesHashIndex const* _index;
  MMFilesHashIndexLookupBuilder _lookups;
  std::vector<MMFilesHashIndexElement*> _buffer;
  size_t _posInBuffer;
};

class MMFilesHashIndexIteratorVPack final : public IndexIterator {
 public:
  /// @brief Construct an MMFilesHashIndexIterator based on VelocyPack
  MMFilesHashIndexIteratorVPack(
      LogicalCollection* collection, transaction::Methods* trx,
      ManagedDocumentResult* mmdr, MMFilesHashIndex const* index,
      std::unique_ptr<arangodb::velocypack::Builder>& searchValues);

  ~MMFilesHashIndexIteratorVPack();

  char const* typeName() const override { return "hash-index-iterator-vpack"; }

  bool next(TokenCallback const& cb, size_t limit) override;

  void reset() override;

 private:
  MMFilesHashIndex const* _index;
  std::unique_ptr<arangodb::velocypack::Builder> _searchValues;
  arangodb::velocypack::ArrayIterator _iterator;
  std::vector<MMFilesHashIndexElement*> _buffer;
  size_t _posInBuffer;
};

class MMFilesHashIndex final : public MMFilesPathBasedIndex {
  friend class MMFilesHashIndexIterator;
  friend class MMFilesHashIndexIteratorVPack;

 public:
  MMFilesHashIndex() = delete;

  MMFilesHashIndex(TRI_idx_iid_t, LogicalCollection*,
                   arangodb::velocypack::Slice const&);

  ~MMFilesHashIndex();

 public:
  IndexType type() const override { return Index::TRI_IDX_TYPE_HASH_INDEX; }

  char const* typeName() const override { return "hash"; }

  bool allowExpansion() const override { return true; }

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimateLocal(
      arangodb::StringRef const* = nullptr) const override;

  size_t memory() const override;

  void toVelocyPackFigures(VPackBuilder&) const override;

  bool matchesDefinition(VPackSlice const& info) const override;

  Result insert(transaction::Methods*, TRI_voc_rid_t,
                arangodb::velocypack::Slice const&, bool isRollback) override;

  Result remove(transaction::Methods*, TRI_voc_rid_t,
                arangodb::velocypack::Slice const&, bool isRollback) override;

  void batchInsert(
      transaction::Methods*,
      std::vector<std::pair<TRI_voc_rid_t, arangodb::velocypack::Slice>> const&,
      std::shared_ptr<arangodb::basics::LocalTaskQueue> queue) override;

  void unload() override;

  int sizeHint(transaction::Methods*, size_t) override;

  bool hasBatchInsert() const override { return true; }

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
  /// @brief locates entries in the hash index given a velocypack slice
  int lookup(transaction::Methods*, arangodb::velocypack::Slice,
             std::vector<MMFilesHashIndexElement*>&) const;

  int insertUnique(transaction::Methods*, TRI_voc_rid_t,
                   arangodb::velocypack::Slice const&, bool isRollback);

  void batchInsertUnique(
      transaction::Methods*,
      std::vector<std::pair<TRI_voc_rid_t, arangodb::velocypack::Slice>> const&,
      std::shared_ptr<arangodb::basics::LocalTaskQueue> queue);

  int insertMulti(transaction::Methods*, TRI_voc_rid_t,
                  arangodb::velocypack::Slice const&, bool isRollback);

  void batchInsertMulti(
      transaction::Methods*,
      std::vector<std::pair<TRI_voc_rid_t, arangodb::velocypack::Slice>> const&,
      std::shared_ptr<arangodb::basics::LocalTaskQueue> queue);

  int removeUniqueElement(transaction::Methods*, MMFilesHashIndexElement*,
                          bool);

  int removeMultiElement(transaction::Methods*, MMFilesHashIndexElement*, bool);

  bool accessFitsIndex(arangodb::aql::AstNode const* access,
                       arangodb::aql::AstNode const* other,
                       arangodb::aql::Variable const* reference,
                       std::unordered_set<size_t>& found) const;

  /// @brief given an element generates a hash integer
 private:
  class HashElementFunc {
   public:
    HashElementFunc() {}

    uint64_t operator()(void* userData, MMFilesHashIndexElement const* element,
                        bool byKey = true) {
      uint64_t hash = element->hash();

      if (byKey) {
        return hash;
      }

      TRI_voc_rid_t revisionId = element->revisionId();
      return fasthash64_uint64(revisionId, hash);
    }
  };

  /// @brief determines if a key corresponds to an element
  class IsEqualElementElementByKey {
    size_t _numFields;
    bool _allowExpansion;

   public:
    IsEqualElementElementByKey(size_t n, bool allowExpansion)
        : _numFields(n), _allowExpansion(allowExpansion) {}

    bool operator()(void* userData, MMFilesHashIndexElement const* left,
                    MMFilesHashIndexElement const* right) {
      TRI_ASSERT(left->revisionId() != 0);
      TRI_ASSERT(right->revisionId() != 0);

      if (!_allowExpansion && left->revisionId() == right->revisionId()) {
        return true;
      }

      IndexLookupContext* context = static_cast<IndexLookupContext*>(userData);

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
  };

 private:
  /// @brief the actual hash index (unique type)
  typedef arangodb::basics::AssocUnique<arangodb::velocypack::Slice,
                                        MMFilesHashIndexElement*>
      TRI_HashArray_t;

  struct UniqueArray {
    UniqueArray() = delete;
    UniqueArray(size_t numPaths, TRI_HashArray_t*, HashElementFunc*,
                IsEqualElementElementByKey*);

    ~UniqueArray();

    TRI_HashArray_t* _hashArray;    // the hash array itself, unique values
    HashElementFunc* _hashElement;  // hash function for elements
    IsEqualElementElementByKey* _isEqualElElByKey;  // comparison func
    size_t _numPaths;
  };

  /// @brief the actual hash index (multi type)
  typedef arangodb::basics::AssocMulti<
      arangodb::velocypack::Slice, MMFilesHashIndexElement*, uint32_t, false>
      TRI_HashArrayMulti_t;

  struct MultiArray {
    MultiArray() = delete;
    MultiArray(size_t numPaths, TRI_HashArrayMulti_t*, HashElementFunc*,
               IsEqualElementElementByKey*);
    ~MultiArray();

    TRI_HashArrayMulti_t*
        _hashArray;                 // the hash array itself, non-unique values
    HashElementFunc* _hashElement;  // hash function for elements
    IsEqualElementElementByKey* _isEqualElElByKey;  // comparison func
    size_t _numPaths;
  };

  union {
    UniqueArray* _uniqueArray;
    MultiArray* _multiArray;
  };
};
}

#endif
