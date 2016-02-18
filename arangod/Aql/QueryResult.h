////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_QUERY_RESULT_H
#define ARANGOD_AQL_QUERY_RESULT_H 1

#include "Basics/Common.h"
#include "Basics/json.h"

namespace arangodb {
namespace velocypack {
class Builder;
}
namespace aql {

struct QueryResult {
  QueryResult& operator=(QueryResult const& other) = delete;

  QueryResult(QueryResult&& other) {
    code = other.code;
    cached = other.cached;
    details = other.details;
    warnings = other.warnings;
    json = other.json;
    stats = other.stats;
    profile = other.profile;
    zone = other.zone;
    clusterplan = other.clusterplan;
    bindParameters = other.bindParameters;
    collectionNames = other.collectionNames;

    other.warnings = nullptr;
    other.json = nullptr;
    other.stats = nullptr;
    other.profile = nullptr;
    other.clusterplan = nullptr;
  }

  QueryResult(int code, std::string const& details)
      : code(code),
        cached(false),
        details(details),
        zone(TRI_UNKNOWN_MEM_ZONE),
        warnings(nullptr),
        json(nullptr),
        profile(nullptr),
        clusterplan(nullptr) {}

  explicit QueryResult(int code) : QueryResult(code, "") {}

  QueryResult() : QueryResult(TRI_ERROR_NO_ERROR) {}

  virtual ~QueryResult() {
    if (warnings != nullptr) {
      TRI_FreeJson(zone, warnings);
    }
    if (json != nullptr) {
      TRI_FreeJson(zone, json);
    }
    if (profile != nullptr) {
      TRI_FreeJson(zone, profile);
    }
  }

  int code;
  bool cached;
  std::string details;
  std::unordered_set<std::string> bindParameters;
  std::vector<std::string> collectionNames;
  TRI_memory_zone_t* zone;
  TRI_json_t* warnings;
  TRI_json_t* json;
  std::shared_ptr<arangodb::velocypack::Builder> stats;
  TRI_json_t* profile;
  TRI_json_t* clusterplan;
};
}
}

#endif
