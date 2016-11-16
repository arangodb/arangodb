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

#ifndef ARANGOD_INDEXES_HASH_INDEX_H
#define ARANGOD_INDEXES_HASH_INDEX_H 1

#include "Basics/Common.h"
#include "Basics/AssocMulti.h"
#include "Basics/AssocUnique.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/fasthash.h"
#include "Indexes/PathBasedIndex.h"
#include "Indexes/IndexIterator.h"
#include "Utils/Transaction.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

/// @brief hash index query parameter
namespace arangodb {

class HashIndex;

/// @brief Class to build Slice lookups out of AST Conditions
class LookupBuilder {
  private:
    TransactionBuilderLeaser _builder;
    bool _usesIn;
    bool _isEmpty;
    size_t _coveredFields;
    std::unordered_map<size_t, arangodb::aql::AstNode const*> _mappingFieldCondition;
    std::unordered_map<
        size_t, std::pair<size_t, std::vector<arangodb::velocypack::Slice>>>
        _inPosition;
    TransactionBuilderLeaser _inStorage;

  public:
   LookupBuilder(
       arangodb::Transaction*, arangodb::aql::AstNode const*,
       arangodb::aql::Variable const*,
       std::vector<std::vector<arangodb::basics::AttributeName>> const&);

   arangodb::velocypack::Slice lookup();

   bool hasAndGetNext();

   void reset();

  private:

   bool incrementInPosition();
   void buildNextSearchValue();
};

class HashIndexIterator final : public IndexIterator {
 public:
  
/// @brief Construct an HashIndexIterator based on Ast Conditions
  HashIndexIterator(LogicalCollection* collection, arangodb::Transaction* trx, 
                    ManagedDocumentResult* mmdr,
                    HashIndex const* index,
                    arangodb::aql::AstNode const*,
                    arangodb::aql::Variable const*);

  ~HashIndexIterator() = default;
  
  char const* typeName() const override { return "hash-index-iterator"; }

  IndexLookupResult next() override;

  void nextBabies(std::vector<IndexLookupResult>&, size_t) override;

  void reset() override;

 private:
  HashIndex const* _index;
  LookupBuilder _lookups;
  std::vector<HashIndexElement*> _buffer;
  size_t _posInBuffer;
};

class HashIndexIteratorVPack final : public IndexIterator {
 public:
  
/// @brief Construct an HashIndexIterator based on VelocyPack
  HashIndexIteratorVPack(LogicalCollection* collection,
      arangodb::Transaction* trx, 
      ManagedDocumentResult* mmdr,
      HashIndex const* index,
      std::unique_ptr<arangodb::velocypack::Builder>& searchValues);

  ~HashIndexIteratorVPack();
  
  char const* typeName() const override { return "hash-index-iterator-vpack"; }

  IndexLookupResult next() override;

  void reset() override;

 private:
  HashIndex const* _index;
  std::unique_ptr<arangodb::velocypack::Builder> _searchValues;
  arangodb::velocypack::ArrayIterator _iterator;
  std::vector<HashIndexElement*> _buffer;
  size_t _posInBuffer;
};

class HashIndex final : public PathBasedIndex {
  friend class HashIndexIterator;
  friend class HashIndexIteratorVPack;

 public:
  HashIndex() = delete;

  HashIndex(TRI_idx_iid_t, LogicalCollection*,
            arangodb::velocypack::Slice const&);

  ~HashIndex();

 public:
  IndexType type() const override {
    return Index::TRI_IDX_TYPE_HASH_INDEX;
  }
  
  bool allowExpansion() const override { return true; }
  
  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return true; }

  double selectivityEstimate(arangodb::StringRef const* = nullptr) const override;

  size_t memory() const override;

  void toVelocyPack(VPackBuilder&, bool) const override;
  void toVelocyPackFigures(VPackBuilder&) const override;

  bool matchesDefinition(VPackSlice const& info) const override;

  int insert(arangodb::Transaction*, TRI_voc_rid_t, arangodb::velocypack::Slice const&, bool isRollback) override;

  int remove(arangodb::Transaction*, TRI_voc_rid_t, arangodb::velocypack::Slice const&, bool isRollback) override;

  int batchInsert(arangodb::Transaction*, std::vector<std::pair<TRI_voc_rid_t, arangodb::velocypack::Slice>> const&, size_t) override;
  
  int unload() override;

  int sizeHint(arangodb::Transaction*, size_t) override;

  bool hasBatchInsert() const override { return true; }

  bool supportsFilterCondition(arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  IndexIterator* iteratorForCondition(arangodb::Transaction*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      bool) const override;

/// @brief creates an IndexIterator for the given VelocyPackSlices
///        Each slice represents the field at the same position. (order matters)
///        And each slice has to be an object of one of the following types:
///        1) {"eq": <compareValue>} // The value in index is exactly this
///        2) {"in": <compareValues>} // The value in index os one of them
  IndexIterator* iteratorForSlice(arangodb::Transaction*, 
                                  ManagedDocumentResult*,
                                  arangodb::velocypack::Slice const,
                                  bool) const override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

 private:

  /// @brief locates entries in the hash index given a velocypack slice
  int lookup(arangodb::Transaction*, arangodb::velocypack::Slice,
             std::vector<HashIndexElement*>&) const;

  int insertUnique(arangodb::Transaction*, TRI_voc_rid_t, arangodb::velocypack::Slice const&, bool isRollback);

  int batchInsertUnique(arangodb::Transaction*,
                        std::vector<std::pair<TRI_voc_rid_t, arangodb::velocypack::Slice>> const&, size_t);

  int insertMulti(arangodb::Transaction*, TRI_voc_rid_t, arangodb::velocypack::Slice const&, bool isRollback);

  int batchInsertMulti(arangodb::Transaction*,
                       std::vector<std::pair<TRI_voc_rid_t, arangodb::velocypack::Slice>> const&, size_t);

  int removeUniqueElement(arangodb::Transaction*, HashIndexElement*, bool);

  int removeMultiElement(arangodb::Transaction*, HashIndexElement*, bool);

  bool accessFitsIndex(arangodb::aql::AstNode const* access,
                       arangodb::aql::AstNode const* other,
                       arangodb::aql::Variable const* reference,
                       std::unordered_set<size_t>& found) const;

  /// @brief given an element generates a hash integer
 private:
  class HashElementFunc {
   public:
    HashElementFunc() {}

    uint64_t operator()(void* userData, HashIndexElement const* element,
                        bool byKey = true) {
      uint64_t hash = element->hash();
      
      if (byKey) {
        return hash;
      }

      TRI_voc_rid_t revisionId = element->revisionId();
      return fasthash64(&revisionId, sizeof(revisionId), hash);
    }
  };

  /// @brief determines if a key corresponds to an element
  class IsEqualElementElementByKey {
    size_t _numFields;

   public:
    explicit IsEqualElementElementByKey(size_t n) : _numFields(n) {}

    bool operator()(void* userData, HashIndexElement const* left,
                    HashIndexElement const* right) {
      TRI_ASSERT(left->revisionId() != 0);
      TRI_ASSERT(right->revisionId() != 0);

      if (left->revisionId() == right->revisionId()) {
        return true;
      }

      IndexLookupContext* context = static_cast<IndexLookupContext*>(userData);
  
      for (size_t i = 0; i < _numFields; ++i) {
        VPackSlice leftData = left->slice(context, i);
        VPackSlice rightData = right->slice(context, i);

        int res = arangodb::basics::VelocyPackHelper::compare(leftData, rightData, false);
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
                                        HashIndexElement*> TRI_HashArray_t;

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
  typedef arangodb::basics::AssocMulti<arangodb::velocypack::Slice,
                                       HashIndexElement*, uint32_t,
                                       false> TRI_HashArrayMulti_t;

  struct MultiArray {
    MultiArray() = delete;
    MultiArray(size_t numPaths,
               TRI_HashArrayMulti_t*, HashElementFunc*,
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
