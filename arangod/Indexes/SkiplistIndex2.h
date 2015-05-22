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
#include "Indexes/Index.h"
#include "IndexOperators/index-operator.h"
#include "SkipLists/skiplistIndex.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

// -----------------------------------------------------------------------------
// --SECTION--                                               class SkiplistIndex
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {

    class SkiplistIndex2 : public Index {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        SkiplistIndex2 () = delete;

        SkiplistIndex2 (TRI_idx_iid_t,
                        struct TRI_document_collection_t*,
                        std::vector<std::string> const&,
                        std::vector<TRI_shape_pid_t> const&,
                        bool,
                        bool);

        ~SkiplistIndex2 ();

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

        triagens::basics::Json toJson (TRI_memory_zone_t*) const override final;
  
        int insert (struct TRI_doc_mptr_t const*, bool) override final;
         
        int remove (struct TRI_doc_mptr_t const*, bool) override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to locate an entry in the skip list index
///
/// Note: this function will not destroy the passed slOperator before it returns
/// Warning: who ever calls this function is responsible for destroying
/// the TRI_index_operator_t* and the TRI_skiplist_iterator_t* results
////////////////////////////////////////////////////////////////////////////////

        TRI_skiplist_iterator_t* lookup (TRI_index_operator_t*,
                                         bool);
        
        bool sparse () const {
          return _sparse;
        }
        
        bool unique () const {
          return _unique;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

        int fillElement(TRI_skiplist_index_element_t*,
                        struct TRI_doc_mptr_t const*);
        
// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the underlying collection
////////////////////////////////////////////////////////////////////////////////

        struct TRI_document_collection_t* _collection;

////////////////////////////////////////////////////////////////////////////////
/// @brief the attribute paths
////////////////////////////////////////////////////////////////////////////////
        
        std::vector<TRI_shape_pid_t> const  _paths;

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual skiplist index
////////////////////////////////////////////////////////////////////////////////

        SkiplistIndex* _skiplistIndex;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether the index is unique
////////////////////////////////////////////////////////////////////////////////

        bool const _unique;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether the index is sparse
////////////////////////////////////////////////////////////////////////////////

        bool const _sparse;
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
