////////////////////////////////////////////////////////////////////////////////
/// @brief edge index
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

#ifndef ARANGODB_INDEXES_EDGE_INDEX_H
#define ARANGODB_INDEXES_EDGE_INDEX_H 1

#include "Basics/Common.h"
#include "Basics/AssocMulti.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/edge-collection.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

struct TRI_json_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                   class EdgeIndex
// -----------------------------------------------------------------------------

namespace triagens {
  namespace aql {
    class SortCondition;
  }

  namespace arango {

    class EdgeIndexIterator final : public IndexIterator {

      public:

        typedef triagens::basics::AssocMulti<TRI_edge_header_t, TRI_doc_mptr_t, uint32_t, true> TRI_EdgeIndexHash_t;

        TRI_doc_mptr_t* next () override;

        void reset () override;

        EdgeIndexIterator (triagens::arango::Transaction* trx,
                           TRI_EdgeIndexHash_t const* index,
                           std::vector<TRI_edge_header_t>& searchValues) 
        : _trx(trx),
          _index(index),
          _keys(std::move(searchValues)),
          _position(0),
          _last(nullptr),
          _buffer(nullptr),
          _posInBuffer(0),
          _batchSize(50) { // This might be adjusted
        }

        ~EdgeIndexIterator () {
          // Free the vector space, not the content
          delete _buffer;
        }

      private:

        triagens::arango::Transaction*     _trx;
        TRI_EdgeIndexHash_t const*         _index;
        std::vector<TRI_edge_header_t>     _keys;
        size_t                             _position;
        TRI_doc_mptr_t*                    _last;
        std::vector<TRI_doc_mptr_t*>*      _buffer;
        size_t                             _posInBuffer;
        size_t                             _batchSize;

    };

    class EdgeIndex final : public Index {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        EdgeIndex () = delete;

        EdgeIndex (TRI_idx_iid_t,
                   struct TRI_document_collection_t*);
        
        explicit EdgeIndex (struct TRI_json_t const*);

        ~EdgeIndex ();

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for hash tables
////////////////////////////////////////////////////////////////////////////////

        typedef triagens::basics::AssocMulti<TRI_edge_header_t, TRI_doc_mptr_t, uint32_t, true> TRI_EdgeIndexHash_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:
        
        IndexType type () const override final {
          return Index::TRI_IDX_TYPE_EDGE_INDEX;
        }
        
        bool isSorted () const override final {
          return false;
        }

        bool hasSelectivityEstimate () const override final {
          return true;
        }

        double selectivityEstimate () const override final;
        
        bool dumpFields () const override final {
          return true;
        }
        
        size_t memory () const override final;

        triagens::basics::Json toJson (TRI_memory_zone_t*, bool) const override final;
        triagens::basics::Json toJsonFigures (TRI_memory_zone_t*) const override final;
  
        int insert (triagens::arango::Transaction*, struct TRI_doc_mptr_t const*, bool) override final;
         
        int remove (triagens::arango::Transaction*, struct TRI_doc_mptr_t const*, bool) override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges using the index, restarting at the edge pointed at
/// by next
////////////////////////////////////////////////////////////////////////////////
      
        void lookup (triagens::arango::Transaction*,
                     TRI_edge_index_iterator_t const*,
                     std::vector<TRI_doc_mptr_copy_t>&,
                     TRI_doc_mptr_copy_t*&,
                     size_t);
        
        int batchInsert (triagens::arango::Transaction*,
                         std::vector<TRI_doc_mptr_t const*> const*,
                         size_t) override final;

        int sizeHint (triagens::arango::Transaction*, size_t) override final;

        bool hasBatchInsert () const override final {
          return true;
        }

        TRI_EdgeIndexHash_t* from () {
          return _edgesFrom;
        }

        TRI_EdgeIndexHash_t* to () {
          return _edgesTo;
        }

        bool supportsFilterCondition (triagens::aql::AstNode const*,
                                      triagens::aql::Variable const*,
                                      size_t,
                                      size_t&,
                                      double&) const override;

        IndexIterator* iteratorForCondition (triagens::arango::Transaction*,
                                             IndexIteratorContext*,
                                             triagens::aql::Ast*,
                                             triagens::aql::AstNode const*,
                                             triagens::aql::Variable const*,
                                             bool) const override;

        triagens::aql::AstNode* specializeCondition (triagens::aql::AstNode*,
                                                     triagens::aql::Variable const*) const override;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the iterator
////////////////////////////////////////////////////////////////////////////////
    
        IndexIterator* createIterator (triagens::arango::Transaction*,
                                       IndexIteratorContext*,
                                       triagens::aql::AstNode const*,
                                       std::vector<triagens::aql::AstNode const*> const&) const;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the hash table for _from 
////////////////////////////////////////////////////////////////////////////////
  
        TRI_EdgeIndexHash_t* _edgesFrom;

////////////////////////////////////////////////////////////////////////////////
/// @brief the hash table for _to
////////////////////////////////////////////////////////////////////////////////

        TRI_EdgeIndexHash_t* _edgesTo;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of buckets effectively used by the index
////////////////////////////////////////////////////////////////////////////////

        size_t _numBuckets;

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
