////////////////////////////////////////////////////////////////////////////////
/// @brief path-based index
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_INDEXES_PATH_BASED_INDEX_H
#define ARANGODB_INDEXES_PATH_BASED_INDEX_H 1

#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "VocBase/shaped-json.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"
#include "VocBase/document-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              class PathBasedIndex
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {

    class PathBasedIndex : public Index {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        PathBasedIndex () = delete;

        PathBasedIndex (TRI_idx_iid_t,
                        struct TRI_document_collection_t*,
                        std::vector<std::vector<triagens::basics::AttributeName>> const&,
                        bool unique,
                        bool sparse);

        ~PathBasedIndex ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the index should reveal its fields
////////////////////////////////////////////////////////////////////////////////
        
        bool dumpFields () const override final {
          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the attribute paths
////////////////////////////////////////////////////////////////////////////////
        
        std::vector<std::vector<std::pair<TRI_shape_pid_t, bool>>> const& paths () const {
          return _paths;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the index is sparse
////////////////////////////////////////////////////////////////////////////////

        inline bool sparse () const {
          return _sparse;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the index is unique
////////////////////////////////////////////////////////////////////////////////
        
        inline bool unique () const {
          return _unique;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the memory needed for an index key entry
////////////////////////////////////////////////////////////////////////////////

        size_t keyEntrySize () const {
          return _paths.size() * (sizeof(TRI_shaped_sub_t) + sizeof(TRI_doc_mptr_t*));
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the attribute paths
////////////////////////////////////////////////////////////////////////////////
        
        std::vector<std::vector<std::pair<TRI_shape_pid_t, bool>>> const  _paths;

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
