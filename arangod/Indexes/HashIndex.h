////////////////////////////////////////////////////////////////////////////////
/// @brief hash index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_INDEXES_HASH_INDEX_H
#define ARANGODB_INDEXES_HASH_INDEX_H 1

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

////////////////////////////////////////////////////////////////////////////////
/// @brief hash index query parameter
////////////////////////////////////////////////////////////////////////////////

struct TRI_hash_index_search_value_t {
  TRI_hash_index_search_value_t ();
  ~TRI_hash_index_search_value_t ();

  TRI_hash_index_search_value_t (TRI_hash_index_search_value_t const&) = delete;
  TRI_hash_index_search_value_t& operator= (TRI_hash_index_search_value_t const&) = delete;

  void reserve (size_t);
  void destroy ();

  size_t _length;
  struct TRI_shaped_json_s* _values;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   class HashIndex
// -----------------------------------------------------------------------------

namespace triagens {
  namespace aql {
    class SortCondition;
  }
  namespace arango {

    class HashIndexIterator : public IndexIterator {

      public:

        HashIndexIterator (HashIndex const* index,
                           TRI_hash_index_search_value_t* searchValue) 
        : _index(index),
          _searchValue(searchValue),
          _posInBuffer(0) {
        }

        ~HashIndexIterator() {
          delete _searchValue;
        }

        TRI_doc_mptr_t* next () override;

        void reset () override;

      private:

        HashIndex const*                  _index;
        TRI_hash_index_search_value_t*    _searchValue;
        std::vector<TRI_doc_mptr_t*>      _buffer;
        size_t                            _posInBuffer;

    };

    class HashIndex : public PathBasedIndex {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        HashIndex () = delete;

        HashIndex (TRI_idx_iid_t,
                   struct TRI_document_collection_t*,
                   std::vector<std::vector<triagens::basics::AttributeName>> const&,
                   bool,
                   bool);

        ~HashIndex ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:
        
        IndexType type () const override final {
          return Index::TRI_IDX_TYPE_HASH_INDEX;
        }
        
        bool isSorted () const override final {
          return false;
        }

        bool hasSelectivityEstimate () const override final {
          return true;
        }

        double selectivityEstimate () const override final;

        size_t memory () const override final;

        triagens::basics::Json toJson (TRI_memory_zone_t*, bool) const override final;
        triagens::basics::Json toJsonFigures (TRI_memory_zone_t*) const override final;
  
        int insert (struct TRI_doc_mptr_t const*, bool) override final;
         
        int remove (struct TRI_doc_mptr_t const*, bool) override final;
        
        int batchInsert (std::vector<TRI_doc_mptr_t const*> const*,
                         size_t) override final;

        int sizeHint (size_t) override final;

        bool hasBatchInsert () const override final {
          return true;
        }
        
        std::vector<std::vector<std::pair<TRI_shape_pid_t, bool>>> const& paths () const {
          return _paths;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given shaped json objects
////////////////////////////////////////////////////////////////////////////////

        int lookup (TRI_hash_index_search_value_t*,
                    std::vector<TRI_doc_mptr_t*>&) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given shaped json objects
////////////////////////////////////////////////////////////////////////////////

        int lookup (TRI_hash_index_search_value_t*,
                    std::vector<TRI_doc_mptr_copy_t>&,
                    TRI_index_element_t*&,
                    size_t batchSize) const;

        bool supportsFilterCondition (triagens::aql::AstNode const*,
                                      triagens::aql::Variable const*,
                                      double&) const override;

        IndexIterator* iteratorForCondition (IndexIteratorContext*,
                                             triagens::aql::Ast*,
                                             triagens::aql::AstNode const*,
                                             triagens::aql::Variable const*) const override;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

        int insertUnique (struct TRI_doc_mptr_t const*, bool);

        int batchInsertUnique (std::vector<TRI_doc_mptr_t const*> const*, size_t);

        int insertMulti (struct TRI_doc_mptr_t const*, bool);

        int batchInsertMulti (std::vector<TRI_doc_mptr_t const*> const*, size_t);

        int removeUniqueElement (TRI_index_element_t*, bool);

        int removeUnique (struct TRI_doc_mptr_t const*, bool);

        int removeMultiElement (TRI_index_element_t*, bool);

        int removeMulti (struct TRI_doc_mptr_t const*, bool);

        bool accessFitsIndex (triagens::aql::AstNode const* access,
                              triagens::aql::AstNode const* other,
                              triagens::aql::Variable const* reference,
                              std::unordered_set<size_t>& found) const;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private classes
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief given an element generates a hash integer
////////////////////////////////////////////////////////////////////////////////

      private:

        class HashElementFunc {

            size_t _numFields;

          public:

            HashElementFunc (size_t n) 
              : _numFields(n) {
            }

            uint64_t operator() (TRI_index_element_t const* element,
                                 bool byKey = true) {
              uint64_t hash = 0x0123456789abcdef;

              for (size_t j = 0;  j < _numFields;  j++) {
                char const* data;
                size_t length;
                TRI_InspectShapedSub(&element->subObjects()[j], 
                                     element->document(), data, length);

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

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if a key corresponds to an element
////////////////////////////////////////////////////////////////////////////////

        class IsEqualElementElementByKey {

            size_t _numFields;

          public:

            IsEqualElementElementByKey (size_t n) 
              : _numFields(n) {
            }

            bool operator() (TRI_index_element_t const* left,
                             TRI_index_element_t const* right) {
              TRI_ASSERT_EXPENSIVE(left->document() != nullptr);
              TRI_ASSERT_EXPENSIVE(right->document() != nullptr);

              if (left->document() == right->document()) {
                return true;
              }

              for (size_t j = 0;  j < _numFields;  ++j) {
                TRI_shaped_sub_t* leftSub  = &left->subObjects()[j];
                TRI_shaped_sub_t* rightSub = &right->subObjects()[j];

                if (leftSub->_sid != rightSub->_sid) {
                  return false;
                }

                char const* leftData;
                size_t leftLength;
                TRI_InspectShapedSub(leftSub, left->document(),
                                     leftData, leftLength);

                char const* rightData;
                size_t rightLength;
                TRI_InspectShapedSub(rightSub, right->document(),
                                     rightData, rightLength);

                if (leftLength != rightLength) {
                  return false;
                }

                if (leftLength > 0 && 
                    memcmp(leftData, rightData, leftLength) != 0) {
                  return false;
                }
              }

              return true;
            }
        };

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual hash index (unique type)
////////////////////////////////////////////////////////////////////////////////
  
        typedef triagens::basics::AssocUnique<TRI_hash_index_search_value_t,
                                              TRI_index_element_t>
                TRI_HashArray_t;
          
        struct UniqueArray {
          UniqueArray () = delete;
          UniqueArray (TRI_HashArray_t*, HashElementFunc*, IsEqualElementElementByKey*);

          ~UniqueArray ();

          TRI_HashArray_t*            _hashArray;   // the hash array itself, unique values
          HashElementFunc*            _hashElement; // hash function for elements
          IsEqualElementElementByKey* _isEqualElElByKey;  // comparison func
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual hash index (multi type)
////////////////////////////////////////////////////////////////////////////////
        
        typedef triagens::basics::AssocMulti<TRI_hash_index_search_value_t,
                                             TRI_index_element_t,
                                             uint32_t, true>
                TRI_HashArrayMulti_t;


        struct MultiArray {
          MultiArray () = delete;
          MultiArray (TRI_HashArrayMulti_t*, HashElementFunc*, IsEqualElementElementByKey*);
          ~MultiArray ();

          TRI_HashArrayMulti_t*       _hashArray;   // the hash array itself, non-unique values
          HashElementFunc*            _hashElement; // hash function for elements
          IsEqualElementElementByKey* _isEqualElElByKey;  // comparison func
        };

        union {
          UniqueArray* _uniqueArray;
          MultiArray* _multiArray;
        };

    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
