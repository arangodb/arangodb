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
#include "Indexes/PathBasedIndex.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/shaped-json.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"
#include "VocBase/document-collection.h"
#include "VocBase/VocShaper.h"

struct TRI_json_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief hash index query parameter
////////////////////////////////////////////////////////////////////////////////

struct TRI_hash_index_search_value_t {
  TRI_hash_index_search_value_t();
  ~TRI_hash_index_search_value_t();

  TRI_hash_index_search_value_t(TRI_hash_index_search_value_t const&) = delete;
  TRI_hash_index_search_value_t& operator=(
      TRI_hash_index_search_value_t const&) = delete;

  void reserve(size_t);
  void destroy();

  size_t _length;
  struct TRI_shaped_json_s* _values;
};


namespace arangodb {
namespace aql {
class SortCondition;
}

class Transaction;

class HashIndexIterator final : public IndexIterator {
 public:
  HashIndexIterator(arangodb::Transaction* trx, HashIndex const* index,
                    std::vector<TRI_hash_index_search_value_t*>& keys)
      : _trx(trx),
        _index(index),
        _keys(keys),
        _position(0),
        _buffer(),
        _posInBuffer(0) {}

  ~HashIndexIterator() {
    for (auto& it : _keys) {
      delete it;
    }
  }

  TRI_doc_mptr_t* next() override;

  void reset() override;

 private:
  arangodb::Transaction* _trx;
  HashIndex const* _index;
  std::vector<TRI_hash_index_search_value_t*> _keys;
  size_t _position;
  std::vector<TRI_doc_mptr_t*> _buffer;
  size_t _posInBuffer;
};

class HashIndex final : public PathBasedIndex {
  
 public:
  HashIndex() = delete;

  HashIndex(TRI_idx_iid_t, struct TRI_document_collection_t*,
            std::vector<std::vector<arangodb::basics::AttributeName>> const&,
            bool, bool);

  explicit HashIndex(struct TRI_json_t const*);

  ~HashIndex();

  
 public:
  IndexType type() const override final {
    return Index::TRI_IDX_TYPE_HASH_INDEX;
  }

  bool isSorted() const override final { return false; }

  bool hasSelectivityEstimate() const override final { return true; }

  double selectivityEstimate() const override final;

  size_t memory() const override final;

  arangodb::basics::Json toJson(TRI_memory_zone_t*, bool) const override final;
  arangodb::basics::Json toJsonFigures(TRI_memory_zone_t*) const override final;

  int insert(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
             bool) override final;

  int remove(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
             bool) override final;

  int batchInsert(arangodb::Transaction*,
                  std::vector<TRI_doc_mptr_t const*> const*,
                  size_t) override final;

  int sizeHint(arangodb::Transaction*, size_t) override final;

  bool hasBatchInsert() const override final { return true; }

  std::vector<std::vector<std::pair<TRI_shape_pid_t, bool>>> const& paths()
      const {
    return _paths;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief locates entries in the hash index given shaped json objects
  //////////////////////////////////////////////////////////////////////////////

  int lookup(arangodb::Transaction*, TRI_hash_index_search_value_t*,
             std::vector<TRI_doc_mptr_t*>&) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief locates entries in the hash index given shaped json objects
  //////////////////////////////////////////////////////////////////////////////

  int lookup(arangodb::Transaction*, TRI_hash_index_search_value_t*,
             std::vector<TRI_doc_mptr_copy_t>&, TRI_index_element_t*&,
             size_t batchSize) const;

  bool supportsFilterCondition(arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  IndexIterator* iteratorForCondition(arangodb::Transaction*,
                                      IndexIteratorContext*,
                                      arangodb::aql::Ast*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      bool) const override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

  
 private:
  int insertUnique(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
                   bool);

  int batchInsertUnique(arangodb::Transaction*,
                        std::vector<TRI_doc_mptr_t const*> const*, size_t);

  int insertMulti(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
                  bool);

  int batchInsertMulti(arangodb::Transaction*,
                       std::vector<TRI_doc_mptr_t const*> const*, size_t);

  int removeUniqueElement(arangodb::Transaction*, TRI_index_element_t*,
                          bool);

  int removeUnique(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
                   bool);

  int removeMultiElement(arangodb::Transaction*, TRI_index_element_t*,
                         bool);

  int removeMulti(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
                  bool);

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

    uint64_t operator()(void* userData, TRI_index_element_t const* element,
                        bool byKey = true) {
      uint64_t hash = 0x0123456789abcdef;

      for (size_t j = 0; j < _numFields; j++) {
        char const* data;
        size_t length;
        TRI_InspectShapedSub(&element->subObjects()[j], element->document(),
                             data, length);

        // ignore the sid for hashing
        // only hash the data block
        hash = fasthash64(data, length, hash);
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

    bool operator()(void* userData, TRI_index_element_t const* left,
                    TRI_index_element_t const* right) {
      TRI_ASSERT_EXPENSIVE(left->document() != nullptr);
      TRI_ASSERT_EXPENSIVE(right->document() != nullptr);

      if (left->document() == right->document()) {
        return true;
      }

      for (size_t j = 0; j < _numFields; ++j) {
        TRI_shaped_sub_t* leftSub = &left->subObjects()[j];
        TRI_shaped_sub_t* rightSub = &right->subObjects()[j];

        if (leftSub->_sid != rightSub->_sid) {
          return false;
        }

        char const* leftData;
        size_t leftLength;
        TRI_InspectShapedSub(leftSub, left->document(), leftData, leftLength);

        char const* rightData;
        size_t rightLength;
        TRI_InspectShapedSub(rightSub, right->document(), rightData,
                             rightLength);

        if (leftLength != rightLength) {
          return false;
        }

        if (leftLength > 0 && memcmp(leftData, rightData, leftLength) != 0) {
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

  typedef arangodb::basics::AssocUnique<TRI_hash_index_search_value_t,
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

  typedef arangodb::basics::AssocMulti<TRI_hash_index_search_value_t,
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


