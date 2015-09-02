////////////////////////////////////////////////////////////////////////////////
/// @brief skiplist index
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

#ifndef ARANGODB_INDEXES_SKIPLIST_INDEX_H
#define ARANGODB_INDEXES_SKIPLIST_INDEX_H 1

#include "Basics/Common.h"
#include "Basics/SkipList.h"
#include "Indexes/PathBasedIndex.h"
#include "IndexOperators/index-operator.h"
#include "VocBase/shaped-json.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

typedef struct {
  TRI_shaped_json_t* _fields;   // list of shaped json objects which the
                                // collection should know about
  size_t _numFields;   // Note that the number of fields coming from
                       // a query can be smaller than the number of
                       // fields indexed
}
TRI_skiplist_index_key_t;

namespace triagens {
  namespace arango {

////////////////////////////////////////////////////////////////////////////////
/// @brief Iterator structure for skip list. We require a start and stop node
///
/// Intervals are open in the sense that both end points are not members
/// of the interval. This means that one has to use SkipList::nextNode
/// on the start node to get the first element and that the stop node
/// can be NULL. Note that it is ensured that all intervals in an iterator
/// are non-empty.
////////////////////////////////////////////////////////////////////////////////

    class SkiplistIterator {

      private:

        friend class SkiplistIndex;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private structs
// -----------------------------------------------------------------------------
      
      private:

        // Shorthand for the skiplist node
        typedef triagens::basics::SkipListNode<TRI_skiplist_index_key_t, TRI_index_element_t> Node;

        struct SkiplistIteratorInterval {
          Node* _leftEndPoint;
          Node* _rightEndPoint;

          SkiplistIteratorInterval ()
            : _leftEndPoint(nullptr),
              _rightEndPoint(nullptr) { 
          }
        };

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------
      
      private:

        SkiplistIndex const* _index;
        size_t _currentInterval; // starts with 0, current interval used
        bool _reverse;
        Node* _cursor;
        std::vector<SkiplistIteratorInterval> _intervals;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------
      
      public:

        SkiplistIterator (SkiplistIndex const* idx,
                          bool reverse) 
          : _index(idx) ,
            _currentInterval(0),
            _reverse(reverse),
            _cursor(nullptr) {
        }

        ~SkiplistIterator () {
        }

        // always holds the last node returned, initially equal to
        // the _leftEndPoint of the first interval (or the 
        // _rightEndPoint of the last interval in the reverse
        // case), can be nullptr if there are no intervals
        // (yet), or, in the reverse case, if the cursor is
        // at the end of the last interval. Additionally
        // in the non-reverse case _cursor is set to nullptr
        // if the cursor is exhausted.
        // See SkiplistNextIterationCallback and
        // SkiplistPrevIterationCallback for the exact
        // condition for the iterator to be exhausted.
        
// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------
      
      public:
        
        size_t size () const;

        bool hasNext () const;

        TRI_index_element_t* next ();

        void initCursor ();

        void findHelper (
          TRI_index_operator_t const* indexOperator,
          std::vector<SkiplistIteratorInterval>& interval
        );

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

        bool hasPrevIteration () const;
        TRI_index_element_t* prevIteration ();

        bool hasNextIteration () const;
        TRI_index_element_t* nextIteration ();

        bool findHelperIntervalIntersectionValid (
          SkiplistIteratorInterval const& lInterval,
          SkiplistIteratorInterval const& rInterval,
          SkiplistIteratorInterval& interval
        );

        bool findHelperIntervalValid (SkiplistIteratorInterval const& interval);
    };

// -----------------------------------------------------------------------------
// --SECTION--                                               class SkiplistIndex
// -----------------------------------------------------------------------------

    class SkiplistIndex : public PathBasedIndex {

      struct KeyElementComparator {
        int operator() (TRI_skiplist_index_key_t const* leftKey,
                        TRI_index_element_t const* rightElement) const;

        KeyElementComparator (SkiplistIndex* idx) {
          _idx = idx;
        }

        private:
          SkiplistIndex* _idx;

      };

      struct ElementElementComparator {
        int operator() (TRI_index_element_t const* leftElement,
                        TRI_index_element_t const* rightElement,
                        triagens::basics::SkipListCmpType cmptype) const;

        ElementElementComparator (SkiplistIndex* idx) {
          _idx = idx;
        }

        private:
          SkiplistIndex* _idx;

      };

      friend class SkiplistIterator;
      friend struct KeyElementComparator;
      friend struct ElementElementComparator;

      typedef triagens::basics::SkipList<TRI_skiplist_index_key_t, TRI_index_element_t> TRI_Skiplist;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        SkiplistIndex () = delete;

        SkiplistIndex (TRI_idx_iid_t,
                        struct TRI_document_collection_t*,
                        std::vector<std::vector<triagens::basics::AttributeName>> const&,
                        bool,
                        bool);

        ~SkiplistIndex ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:
        
        IndexType type () const override final {
          return Index::TRI_IDX_TYPE_SKIPLIST_INDEX;
        }

        bool hasSelectivityEstimate () const override final {
          return false;
        }
        
        size_t memory () const override final;

        triagens::basics::Json toJson (TRI_memory_zone_t*, bool) const override final;
        triagens::basics::Json toJsonFigures (TRI_memory_zone_t*) const override final;
  
        int insert (struct TRI_doc_mptr_t const*, bool) override final;
         
        int remove (struct TRI_doc_mptr_t const*, bool) override final;

        size_t numFields () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to locate an entry in the skip list index
///
/// Note: this function will not destroy the passed slOperator before it returns
/// Warning: who ever calls this function is responsible for destroying
/// the TRI_index_operator_t* and the TRI_skiplist_iterator_t* results
////////////////////////////////////////////////////////////////////////////////

        SkiplistIterator* lookup (TRI_index_operator_t*, bool);

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

        size_t elementSize () const;

        int _CmpElmElm (TRI_index_element_t const* leftElement,
                       TRI_index_element_t const* rightElement,
                       triagens::basics::SkipListCmpType cmptype);

        int _CmpKeyElm (TRI_skiplist_index_key_t const* leftKey,
                       TRI_index_element_t const* rightElement);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

        ElementElementComparator CmpElmElm;

        KeyElementComparator CmpKeyElm;

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual skiplist index
////////////////////////////////////////////////////////////////////////////////

        TRI_Skiplist* _skiplistIndex;

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
