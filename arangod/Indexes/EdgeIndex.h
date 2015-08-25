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
#include "VocBase/edge-collection.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                   class EdgeIndex
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {

    class EdgeIndex : public Index {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        EdgeIndex () = delete;

        EdgeIndex (TRI_idx_iid_t,
                   struct TRI_document_collection_t*);

        ~EdgeIndex ();

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for hash tables
////////////////////////////////////////////////////////////////////////////////

        typedef triagens::basics::AssocMulti<void, void, uint32_t> TRI_EdgeIndexHash_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:
        
        IndexType type () const override final {
          return Index::TRI_IDX_TYPE_EDGE_INDEX;
        }

        bool hasSelectivityEstimate () const override final {
          return true;
        }

        double selectivityEstimate () const override final;
        
        bool dumpFields () const override final {
          return true;
        }
        
        size_t memory () const override final;

        triagens::basics::Json toJson (TRI_memory_zone_t*) const override final;
  
        int insert (struct TRI_doc_mptr_t const*, bool) override final;
         
        int remove (struct TRI_doc_mptr_t const*, bool) override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges using the index, restarting at the edge pointed at
/// by next
////////////////////////////////////////////////////////////////////////////////
      
        void lookup (TRI_edge_index_iterator_t const*,
                     std::vector<TRI_doc_mptr_copy_t>&,
                     void*&,
                     size_t);

        int sizeHint (size_t) override final;
        int sizeHint () override final;

        TRI_EdgeIndexHash_t* from () {
          return _edgesFrom;
        }

        TRI_EdgeIndexHash_t* to () {
          return _edgesTo;
        }

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
