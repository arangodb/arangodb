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

////////////////////////////////////////////////////////////////////////////////
/// @brief hash index query parameter
////////////////////////////////////////////////////////////////////////////////

namespace arangodb {

class HashIndex;

////////////////////////////////////////////////////////////////////////////////
/// @brief Class to build Slice lookups out of AST Conditions
////////////////////////////////////////////////////////////////////////////////

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
  
////////////////////////////////////////////////////////////////////////////////
/// @brief Construct an HashIndexIterator based on Ast Conditions
////////////////////////////////////////////////////////////////////////////////

  HashIndexIterator(arangodb::Transaction* trx, HashIndex const* index,
                    arangodb::aql::AstNode const*,
                    arangodb::aql::Variable const*);

  ~HashIndexIterator() = default;

  TRI_doc_mptr_t* next() override;

  void nextBabies(std::vector<TRI_doc_mptr_t*>&, size_t) override;

  void reset() override;

 private:
  arangodb::Transaction* _trx;
  HashIndex const* _index;
  LookupBuilder _lookups;
  std::vector<TRI_doc_mptr_t*> _buffer;
  size_t _posInBuffer;
};



class HashIndexIteratorVPack final : public IndexIterator {
 public:
  
////////////////////////////////////////////////////////////////////////////////
/// @brief Construct an HashIndexIterator based on VelocyPack
////////////////////////////////////////////////////////////////////////////////

  HashIndexIteratorVPack(
      arangodb::Transaction* trx, HashIndex const* index,
      std::unique_ptr<arangodb::velocypack::Builder>& searchValues)
      : _trx(trx),
        _index(index),
        _searchValues(searchValues.get()),
        _iterator(_searchValues->slice()),
        _buffer(),
        _posInBuffer(0) {
    searchValues.release(); // now we have ownership for searchValues
  }

  ~HashIndexIteratorVPack();

  TRI_doc_mptr_t* next() override;

  void reset() override;

 private:
  arangodb::Transaction* _trx;
  HashIndex const* _index;
  std::unique_ptr<arangodb::velocypack::Builder> _searchValues;
  arangodb::velocypack::ArrayIterator _iterator;
  std::vector<TRI_doc_mptr_t*> _buffer;
  size_t _posInBuffer;
};

class HashIndex final : public PathBasedIndex {
  friend class HashIndexIterator;
  friend class HashIndexIteratorVPack;

 public:
  HashIndex() = delete;

  HashIndex(TRI_idx_iid_t, arangodb::LogicalCollection*,
            std::vector<std::vector<arangodb::basics::AttributeName>> const&,
            bool, bool);

  HashIndex(TRI_idx_iid_t, LogicalCollection*,
            arangodb::velocypack::Slice const&);

  explicit HashIndex(VPackSlice const&);

  ~HashIndex();

 public:
  IndexType type() const override final {
    return Index::TRI_IDX_TYPE_HASH_INDEX;
  }
  
  bool allowExpansion() const override final { return true; }
  
  bool canBeDropped() const override final { return true; }

  bool isSorted() const override final { return false; }

  bool hasSelectivityEstimate() const override final { return true; }

  double selectivityEstimate() const override final;

  size_t memory() const override final;

  void toVelocyPack(VPackBuilder&, bool) const override final;
  void toVelocyPackFigures(VPackBuilder&) const override final;

  bool matchesDefinition(VPackSlice const& info) const override final;

  int insert(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
             bool) override final;

  int remove(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
             bool) override final;

  int batchInsert(arangodb::Transaction*,
                  std::vector<TRI_doc_mptr_t const*> const*,
                  size_t) override final;
  
  int unload() override final;

  int sizeHint(arangodb::Transaction*, size_t) override final;

  bool hasBatchInsert() const override final { return true; }


  bool supportsFilterCondition(arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  IndexIterator* iteratorForCondition(arangodb::Transaction*,
                                      IndexIteratorContext*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an IndexIterator for the given VelocyPackSlices
///        Each slice represents the field at the same position. (order matters)
///        And each slice has to be an object of one of the following types:
///        1) {"eq": <compareValue>} // The value in index is exactly this
///        2) {"in": <compareValues>} // The value in index os one of them
////////////////////////////////////////////////////////////////////////////////

  IndexIterator* iteratorForSlice(arangodb::Transaction*, IndexIteratorContext*,
                                  arangodb::velocypack::Slice const,
                                  bool) const override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

 private:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief locates entries in the hash index given a velocypack slice
  //////////////////////////////////////////////////////////////////////////////
  int lookup(arangodb::Transaction*, arangodb::velocypack::Slice,
             std::vector<TRI_doc_mptr_t*>&) const;

  int insertUnique(arangodb::Transaction*, struct TRI_doc_mptr_t const*, bool);

  int batchInsertUnique(arangodb::Transaction*,
                        std::vector<TRI_doc_mptr_t const*> const*, size_t);

  int insertMulti(arangodb::Transaction*, struct TRI_doc_mptr_t const*, bool);

  int batchInsertMulti(arangodb::Transaction*,
                       std::vector<TRI_doc_mptr_t const*> const*, size_t);

  int removeUniqueElement(arangodb::Transaction*, TRI_index_element_t*, bool);

  int removeUnique(arangodb::Transaction*, struct TRI_doc_mptr_t const*, bool);

  int removeMultiElement(arangodb::Transaction*, TRI_index_element_t*, bool);

  int removeMulti(arangodb::Transaction*, struct TRI_doc_mptr_t const*, bool);

  bool accessFitsIndex(arangodb::aql::AstNode const* access,
                       arangodb::aql::AstNode const* other,
                       arangodb::aql::Variable const* reference,
                       std::unordered_set<size_t>& found) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief given an element generates a hash integer
  //////////////////////////////////////////////////////////////////////////////

 private:
  class HashElementFunc {
    size_t _numFields;

   public:
    explicit HashElementFunc(size_t n) : _numFields(n) {}

    uint64_t operator()(void*, TRI_index_element_t const* element,
                        bool byKey = true) {
      uint64_t hash = 0x0123456789abcdef;

      for (size_t j = 0; j < _numFields; j++) {
        VPackSlice data = element->subObjects()[j].slice(element->document());
        // must use normalized hash here, to normalize different representations 
        // of arrays/objects/numbers
        hash = data.normalizedHash(hash);
      }

      if (byKey) {
        return hash;
      }

      TRI_doc_mptr_t* ptr = element->document();
      return fasthash64(&ptr, sizeof(TRI_doc_mptr_t*), hash);
    }
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief determines if a key corresponds to an element
  //////////////////////////////////////////////////////////////////////////////

  class IsEqualElementElementByKey {
    size_t _numFields;

   public:
    explicit IsEqualElementElementByKey(size_t n) : _numFields(n) {}

    bool operator()(void*, TRI_index_element_t const* left,
                    TRI_index_element_t const* right) {
      TRI_ASSERT(left->document() != nullptr);
      TRI_ASSERT(right->document() != nullptr);

      if (left->document() == right->document()) {
        return true;
      }

      for (size_t j = 0; j < _numFields; ++j) {
        TRI_vpack_sub_t* leftSub = left->subObjects() + j;
        TRI_vpack_sub_t* rightSub = right->subObjects() + j;
        VPackSlice leftData = leftSub->slice(left->document());
        VPackSlice rightData = rightSub->slice(right->document());

        int res = arangodb::basics::VelocyPackHelper::compare(leftData, rightData, false);
        if (res != 0) {
          return false;
        }
      }

      return true;
    }
  };

 private:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the actual hash index (unique type)
  //////////////////////////////////////////////////////////////////////////////

  typedef arangodb::basics::AssocUnique<arangodb::velocypack::Slice,
                                        TRI_index_element_t> TRI_HashArray_t;

  struct UniqueArray {
    UniqueArray() = delete;
    UniqueArray(TRI_HashArray_t*, HashElementFunc*,
                IsEqualElementElementByKey*);

    ~UniqueArray();

    TRI_HashArray_t* _hashArray;    // the hash array itself, unique values
    HashElementFunc* _hashElement;  // hash function for elements
    IsEqualElementElementByKey* _isEqualElElByKey;  // comparison func
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the actual hash index (multi type)
  //////////////////////////////////////////////////////////////////////////////

  typedef arangodb::basics::AssocMulti<arangodb::velocypack::Slice,
                                       TRI_index_element_t, uint32_t,
                                       true> TRI_HashArrayMulti_t;

  struct MultiArray {
    MultiArray() = delete;
    MultiArray(TRI_HashArrayMulti_t*, HashElementFunc*,
               IsEqualElementElementByKey*);
    ~MultiArray();

    TRI_HashArrayMulti_t*
        _hashArray;                 // the hash array itself, non-unique values
    HashElementFunc* _hashElement;  // hash function for elements
    IsEqualElementElementByKey* _isEqualElElByKey;  // comparison func
  };

  union {
    UniqueArray* _uniqueArray;
    MultiArray* _multiArray;
  };
};
}

#endif
