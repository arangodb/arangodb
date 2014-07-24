////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, error handling
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

#ifndef ARANGODB_AQL_QUERYERROR_H
#define ARANGODB_AQL_QUERYERROR_H 1

#include "Basics/Common.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief query error structure
///
/// This struct is used to hold information about errors that happen during
/// query execution. The data will be passed to the end user.
////////////////////////////////////////////////////////////////////////////////

    struct QueryError {

      QueryError () 
        : _code(TRI_ERROR_NO_ERROR),
          _data(nullptr) {
      }

      ~QueryError () {
        if (_data != nullptr) {
          TRI_Free(TRI_UNKNOWN_MEM_ZONE, _data);
        }
      }

      inline int getCode () const {
        return _code;
      }

      char* getMessage () const;

      char* getContextError (char const*,
                             size_t,
                             size_t,
                             size_t) const;

      int _code;
      char* _message;
      char* _data;
      const char* _file;
      int _line;
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
