////////////////////////////////////////////////////////////////////////////////
/// @brief collection usage guard
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_UTILS_COLLECTION_GUARD_H
#define TRIAGENS_UTILS_COLLECTION_GUARD_H 1

#include "Basics/Common.h"
#include "Utils/Exception.h"
#include "VocBase/vocbase.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                             class CollectionGuard
// -----------------------------------------------------------------------------

    class CollectionGuard {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:
         
        CollectionGuard (CollectionGuard const&) = delete;
        CollectionGuard& operator= (CollectionGuard const&) = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the guard, using a collection id
////////////////////////////////////////////////////////////////////////////////

        CollectionGuard (TRI_vocbase_t* vocbase,
                         TRI_voc_cid_t id)
          : _vocbase(vocbase),
            _collection(nullptr) {
  
          _collection = TRI_UseCollectionByIdVocBase(_vocbase, id);

          if (_collection == nullptr) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create the guard, using a collection name
////////////////////////////////////////////////////////////////////////////////

        CollectionGuard (TRI_vocbase_t* vocbase,
                         char const* name)
          : _vocbase(vocbase),
            _collection(nullptr) {

          _collection = TRI_UseCollectionByNameVocBase(_vocbase, name);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the guard
////////////////////////////////////////////////////////////////////////////////

        ~CollectionGuard () {
          if (_collection != nullptr) {
            TRI_ReleaseCollectionVocBase(_vocbase, _collection);
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection pointer
////////////////////////////////////////////////////////////////////////////////

        inline TRI_vocbase_col_t* collection () const {
          return _collection;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief pointer to vocbase
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief pointer to collection
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_col_t* _collection;

    };
  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
