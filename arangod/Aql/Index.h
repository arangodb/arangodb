////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, Index
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
/// @author not James
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_Index_H
#define ARANGODB_AQL_Index_H 1

#include "Basics/Common.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                 struct Index
// -----------------------------------------------------------------------------

    struct Index {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      Index& operator= (Index const&) = delete;
      Index (Index const&) = delete;
      Index () = delete;
      
      Index (TRI_idx_iid_t id, Collection const* collection) 
        : _id(id), _collection(collection){
      }
      
      ~Index() {
      }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get a pointer to the underlying TRI_index_s of the Index
////////////////////////////////////////////////////////////////////////////////
      
      inline TRI_index_s* index () const {
        return TRI_LookupIndex(_collection->documentCollection(), _id);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the index type
////////////////////////////////////////////////////////////////////////////////
      
      inline TRI_idx_type_e type () const {
        return this->index()->_type;
      }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      TRI_idx_iid_t         _id;
      Collection const*     _collection;

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
