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
                         TRI_voc_cid_t id,
                         bool restoreOriginalStatus = false)
          : _vocbase(vocbase),
            _collection(nullptr),
            _originalStatus(TRI_VOC_COL_STATUS_CORRUPTED),
            _restoreOriginalStatus(restoreOriginalStatus) {
  
          _collection = TRI_UseCollectionByIdVocBase(_vocbase, id, _originalStatus);

          if (_collection == nullptr) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create the guard, using a collection name
////////////////////////////////////////////////////////////////////////////////

        CollectionGuard (TRI_vocbase_t* vocbase,
                         char const* name,
                         bool restoreOriginalStatus = false)
          : _vocbase(vocbase),
            _collection(nullptr),
            _originalStatus(TRI_VOC_COL_STATUS_CORRUPTED),
            _restoreOriginalStatus(restoreOriginalStatus) {

          _collection = TRI_UseCollectionByNameVocBase(_vocbase, name, _originalStatus);
          
          if (_collection == nullptr) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the guard
////////////////////////////////////////////////////////////////////////////////

        ~CollectionGuard () {
          if (_collection != nullptr) {
            TRI_ReleaseCollectionVocBase(_vocbase, _collection);

            if (_restoreOriginalStatus && 
                (_originalStatus == TRI_VOC_COL_STATUS_UNLOADING || 
                 _originalStatus == TRI_VOC_COL_STATUS_UNLOADED)) {
              TRI_UnloadCollectionVocBase(_vocbase, _collection, false);
            }
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

////////////////////////////////////////////////////////////////////////////////
/// @brief return the status of the collection at the time of using the guard
////////////////////////////////////////////////////////////////////////////////

        inline TRI_vocbase_col_status_e originalStatus () const {
          return _originalStatus;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief status of collection when invoking the guard
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_col_status_e _originalStatus;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not to restore the original collection status
////////////////////////////////////////////////////////////////////////////////

        bool _restoreOriginalStatus;

    };
  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
