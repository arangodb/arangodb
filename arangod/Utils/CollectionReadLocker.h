////////////////////////////////////////////////////////////////////////////////
/// @brief collection read locker
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_UTILS_COLLECTION_READ_LOCKER_H
#define ARANGODB_UTILS_COLLECTION_READ_LOCKER_H 1

#include "Basics/Common.h"

#include "VocBase/document-collection.h"

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

        CollectionReadLocker (TRI_document_collection_t* document,
                              bool doLock)
          : _document(document),
            _doLock(false) {

          if (doLock) {
            _document->beginRead(_document);
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
            _document->endRead(_document);
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

        TRI_document_collection_t* _document;

////////////////////////////////////////////////////////////////////////////////
/// @brief lock flag
////////////////////////////////////////////////////////////////////////////////

        bool _doLock;

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
