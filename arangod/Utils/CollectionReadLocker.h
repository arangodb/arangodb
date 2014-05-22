////////////////////////////////////////////////////////////////////////////////
/// @brief collection read locker
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

#ifndef TRIAGENS_UTILS_COLLECTION_READ_LOCKER_H
#define TRIAGENS_UTILS_COLLECTION_READ_LOCKER_H 1

#include "Basics/Common.h"

#include "VocBase/primary-collection.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                        class CollectionReadLocker
// -----------------------------------------------------------------------------

    class CollectionReadLocker {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:
         
        CollectionReadLocker (CollectionReadLocker const&) = delete;
        CollectionReadLocker& operator= (CollectionReadLocker const&) = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the locker
////////////////////////////////////////////////////////////////////////////////

        CollectionReadLocker (TRI_primary_collection_t* primary, 
                              bool doLock) 
          : _primary(primary),
            _doLock(false) {

          if (doLock) {
            _primary->beginRead(_primary);          
            _doLock = true;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the locker
////////////////////////////////////////////////////////////////////////////////

        ~CollectionReadLocker () {
          unlock();
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief release the lock
////////////////////////////////////////////////////////////////////////////////

        inline void unlock () {
          if (_doLock) {
            _primary->endRead(_primary);
            _doLock = false;
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief collection pointer
////////////////////////////////////////////////////////////////////////////////

        TRI_primary_collection_t* _primary;

////////////////////////////////////////////////////////////////////////////////
/// @brief lock flag
////////////////////////////////////////////////////////////////////////////////

        bool _doLock;

    };
  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
