////////////////////////////////////////////////////////////////////////////////
/// @brief delete object helper
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
/// @author Dr. Frank Celler
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_DELETE_OBJECT_H
#define TRIAGENS_BASICS_DELETE_OBJECT_H 1

#include "Basics/Common.h"

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

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
      template<typename T, typename S> void operator () (pair<T,S>& ptr) const {
        if (ptr.first != 0) {
          delete ptr.first;
        }
      }
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief delete object helper for maps
////////////////////////////////////////////////////////////////////////////////

    struct DeleteObjectValue {
      template<typename T, typename S> void operator () (pair<T,S>& ptr) const {
        if (ptr.second != 0) {
          delete ptr.second;
        }
      }
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
