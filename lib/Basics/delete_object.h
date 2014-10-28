////////////////////////////////////////////////////////////////////////////////
/// @brief delete object helper
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
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_DELETE__OBJECT_H
#define ARANGODB_BASICS_DELETE__OBJECT_H 1

#include "Basics/Common.h"

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief delete object helper
////////////////////////////////////////////////////////////////////////////////

    struct DeleteObjectAny {
      template<typename T> void operator () (const T* ptr) const {
        if (ptr != 0) {
          delete ptr;
        }
      }
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief delete object helper for maps
////////////////////////////////////////////////////////////////////////////////

    struct DeleteObjectKey {
      template<typename T, typename S> void operator () (std::pair<T,S>& ptr) const {
        if (ptr.first != 0) {
          delete ptr.first;
        }
      }
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief delete object helper for maps
////////////////////////////////////////////////////////////////////////////////

    struct DeleteObjectValue {
      template<typename T, typename S> void operator () (std::pair<T,S>& ptr) const {
        if (ptr.second != 0) {
          delete ptr.second;
        }
      }
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
