////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, query results
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

#ifndef ARANGODB_AQL_QUERY_RESULTV8_H
#define ARANGODB_AQL_QUERY_RESULTV8_H 1

#include "Basics/Common.h"
#include "Basics/json.h"
#include "Aql/QueryResult.h"
#include <v8.h>

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                struct QueryResult
// -----------------------------------------------------------------------------

    struct QueryResultV8 : public QueryResult {
      QueryResultV8& operator= (QueryResultV8 const& other) = delete;
            
      QueryResultV8 (QueryResultV8&& other) 
        : QueryResult ( (QueryResult&&) other),
          result(other.result) {
      }
      
      QueryResultV8 (QueryResult&& other) 
        : QueryResult((QueryResult&&)other),
          result(nullptr) {
      }
      
      QueryResultV8 (int code,
                     std::string const& details) 
        : QueryResult(code, details),
          result(nullptr) {
      }

      explicit QueryResultV8 (int code)
        : QueryResult(code, ""),
          result(nullptr) {
      }

      v8::Handle<v8::Array>          result;
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
