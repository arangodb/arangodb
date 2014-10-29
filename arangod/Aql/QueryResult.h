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

#ifndef ARANGODB_AQL_QUERY_RESULT_H
#define ARANGODB_AQL_QUERY_RESULT_H 1

#include "Basics/Common.h"
#include "Basics/json.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                struct QueryResult
// -----------------------------------------------------------------------------

    struct QueryResult {
      QueryResult& operator= (QueryResult const& other) = delete;
      
      QueryResult (QueryResult&& other) {
        code              = other.code;
        details           = other.details;
        json              = other.json;
        stats             = other.stats;
        profile           = other.profile;
        zone              = other.zone;
        clusterplan       = other.clusterplan;

        other.json        = nullptr;
        other.stats       = nullptr;
        other.profile     = nullptr;
        other.clusterplan = nullptr;
      }

      QueryResult (int code,
                   std::string const& details) 
        : code(code),
          details(details),
          zone(TRI_UNKNOWN_MEM_ZONE),
          json(nullptr),
          stats(nullptr),
        profile(nullptr),
        clusterplan(nullptr) {
      }
      
      explicit QueryResult (int code)
        : QueryResult(code, "") {
      }

      QueryResult ()
        : QueryResult(TRI_ERROR_NO_ERROR) {
      }

      ~QueryResult () {
        if (json != nullptr) {
          TRI_FreeJson(zone, json);
        }
        if (stats != nullptr) {
          TRI_FreeJson(zone, stats);
        }
        if (profile != nullptr) {
          TRI_FreeJson(zone, profile);
        }
      }

      int                             code;
      std::string                     details;
      std::unordered_set<std::string> bindParameters;
      std::vector<std::string>        collectionNames;
      TRI_memory_zone_t*              zone;
      TRI_json_t*                     json;
      TRI_json_t*                     stats;
      TRI_json_t*                     profile;
      TRI_json_t*                     clusterplan;
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
