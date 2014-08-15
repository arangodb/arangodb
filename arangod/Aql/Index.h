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

    struct Index{

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      Index& operator= (Index const&) = delete;
      Index (Index const&) = delete;
      Index () = delete;
      
      Index (std::string const& name,
                  struct TRI_vocbase_s* vocbase,
                  TRI_transaction_type_e accessType, 
                  TRI_index_t* index) 
        : name(name),
          vocbase(vocbase),
          collection(nullptr),
          accessType(accessType),
          _index(index){
      }
      
      ~Index() {
      }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the index id
////////////////////////////////////////////////////////////////////////////////

      inline TRI_idx_iid_t id () const {
        TRI_ASSERT(_index != nullptr);
        return _index->_iid;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the index id
////////////////////////////////////////////////////////////////////////////////

      inline TRI_idx_type_e type () const {
        TRI_ASSERT(_index != nullptr);
        return _index->_type;
      }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      std::string const       name;
      TRI_vocbase_t*          vocbase;
      TRI_vocbase_col_t*      collection;
      TRI_transaction_type_e  accessType;

// -----------------------------------------------------------------------------
// --SECTION--                                                  private variables
// -----------------------------------------------------------------------------

      TRI_index_t*         _index;
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
