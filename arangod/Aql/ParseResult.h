////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, parse results
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

#ifndef ARANGODB_AQL_PARSE_RESULT_H
#define ARANGODB_AQL_PARSE_RESULT_H 1

#include "Basics/Common.h"
#include "BasicsC/json.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                struct ParseResult
// -----------------------------------------------------------------------------

    struct ParseResult {
      ParseResult (ParseResult&& other) {
        json = other.json;
        zone = other.zone;
        other.json = nullptr;
      }

      ParseResult& operator= (ParseResult const& other) = delete;

      ParseResult (int code,
                   std::string const& explanation) 
        : code(code),
          explanation(explanation),
          zone(TRI_UNKNOWN_MEM_ZONE),
          json(nullptr) {
      }

      ParseResult (TRI_memory_zone_t* zone) 
        : zone(zone),
          json(nullptr) {
      }

      ~ParseResult () {
        if (json != nullptr) {
          TRI_FreeJson(zone, json);
        }
      }

      int                             code;
      std::string                     explanation;
      std::unordered_set<std::string> bindParameters;
      std::unordered_set<std::string> collectionNames;
      TRI_memory_zone_t*              zone;
      TRI_json_t*                     json;
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
