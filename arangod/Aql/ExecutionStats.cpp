////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, execution statistics
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

#include "Aql/ExecutionStats.h"
#include "Utils/Exception.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;
using JsonHelper = triagens::basics::JsonHelper;

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the statistics to JSON
////////////////////////////////////////////////////////////////////////////////

Json ExecutionStats::toJson () const {
  Json json(Json::Object);
  json.set("writesExecuted", Json(static_cast<double>(writesExecuted)));
  json.set("writesIgnored",  Json(static_cast<double>(writesIgnored)));
  json.set("scannedFull",    Json(static_cast<double>(scannedFull)));
  json.set("scannedIndex",   Json(static_cast<double>(scannedIndex)));

  if (fullCount > -1) {
    // fullCount is exceptional. it has a default value of -1 and is
    // not reported with this value
    json.set("fullCount",      Json(static_cast<double>(fullCount)));
  }

  return json;
}

Json ExecutionStats::toJsonStatic () {
  Json json(Json::Object);
  json.set("writesExecuted", Json(0.0));
  json.set("writesIgnored",  Json(0.0));
  json.set("scannedFull",    Json(0.0));
  json.set("scannedIndex",   Json(0.0));
  json.set("fullCount",      Json(-1.0));
  json.set("static",         Json(0.0));

  return json;
}

ExecutionStats::ExecutionStats()
  :writesExecuted(0),
   writesIgnored(0),
   scannedFull(0),
   scannedIndex(0),
   fullCount(-1) {
}

ExecutionStats::ExecutionStats (triagens::basics::Json const& jsonStats) {
  if (! jsonStats.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "stats is not an array");
  }

  writesExecuted = JsonHelper::checkAndGetNumericValue<int64_t>(jsonStats.json(), "writesExecuted");
  writesIgnored  = JsonHelper::checkAndGetNumericValue<int64_t>(jsonStats.json(), "writesIgnored");
  scannedFull    = JsonHelper::checkAndGetNumericValue<int64_t>(jsonStats.json(), "scannedFull");
  scannedIndex   = JsonHelper::checkAndGetNumericValue<int64_t>(jsonStats.json(), "scannedIndex");

  // note: fullCount is an optional attribute!
  fullCount      = JsonHelper::getNumericValue<int64_t>(jsonStats.json(), "fullCount", -1);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
