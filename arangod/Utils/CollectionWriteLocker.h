////////////////////////////////////////////////////////////////////////////////
/// @brief collection write locker
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

#ifndef TRIAGENS_UTILS_COLLECTION_WRITE_LOCKER_H
#define TRIAGENS_UTILS_COLLECTION_WRITE_LOCKER_H 1

#include "Basics/Common.h"

#include "VocBase/document-collection.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                       class CollectionWriteLocker
// -----------------------------------------------------------------------------

    class CollectionWriteLocker {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:
         
        CollectionWriteLocker (CollectionWriteLocker const&) = delete;
        CollectionWriteLocker& operator= (CollectionWriteLocker const&) = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the locker
////////////////////////////////////////////////////////////////////////////////

        CollectionWriteLocker (TRI_document_collection_t* document, 
                               bool doLock) 
          : _document(document),
            _doLock(false) {

          if (doLock) {
            _document->beginWrite(_document);
            _doLock = true;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the locker
////////////////////////////////////////////////////////////////////////////////

        ~CollectionWriteLocker () {
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
            _document->endWrite(_document);
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

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
