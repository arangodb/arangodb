////////////////////////////////////////////////////////////////////////////////
/// @brief collection meta-data read locker
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

#ifndef TRIAGENS_UTILS_COLLECTION_METADATA_READ_LOCKER_H
#define TRIAGENS_UTILS_COLLECTION_METADATA_READ_LOCKER_H 1

#include "Basics/Common.h"

#include "VocBase/collection.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                class CollectionMetadataReadLocker
// -----------------------------------------------------------------------------

    class CollectionMetadataReadLocker {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:
         
        CollectionMetadataReadLocker (CollectionMetadataReadLocker const&) = delete;
        CollectionMetadataReadLocker& operator= (CollectionMetadataReadLocker const&) = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the locker
////////////////////////////////////////////////////////////////////////////////

        CollectionMetadataReadLocker (TRI_collection_t* collection) 
          : _collection(collection) {

          TRI_ReadLockReadWriteLock(&_collection->_metadataLock);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the locker
////////////////////////////////////////////////////////////////////////////////

        ~CollectionMetadataReadLocker () {
          TRI_ReadUnlockReadWriteLock(&_collection->_metadataLock);
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief collection pointer
////////////////////////////////////////////////////////////////////////////////

        TRI_collection_t* _collection;
    };
  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
