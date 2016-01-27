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

#include "Aql/ExecutionStats.h"
#include "Basics/Exceptions.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;
using Json = arangodb::basics::Json;
using JsonHelper = arangodb::basics::JsonHelper;

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the statistics to VelocyPack
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> ExecutionStats::toVelocyPack() const {
  auto result = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder b(result.get());
    result->add("writesExecuted", VPackValue(writesExecuted));
    result->add("writesIgnored", VPackValue(writesIgnored));
    result->add("scannedFull", VPackValue(scannedFull));
    result->add("scannedIndex", VPackValue(scannedIndex));
    result->add("filtered", VPackValue(filtered));

    if (fullCount > -1) {
      // fullCount is exceptional. it has a default value of -1 and is
      // not reported with this value
      result->add("fullCount", VPackValue(fullCount));
    }
  }
  return result;
}

std::shared_ptr<VPackBuilder> ExecutionStats::toVelocyPackStatic() {
  auto result = std::make_shared<VPackBuilder>();
  VPackObjectBuilder b(result.get());
  result->add("writesExecuted", VPackValue(0));
  result->add("writesIgnored", VPackValue(0));
  result->add("scannedFull", VPackValue(0));
  result->add("scannedIndex", VPackValue(0));
  result->add("filtered", VPackValue(0));
  result->add("fullCount", VPackValue(-1));
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the statistics to JSON
////////////////////////////////////////////////////////////////////////////////

Json ExecutionStats::toJson() const {
  Json json(Json::Object, 5);
  json.set("writesExecuted", Json(static_cast<double>(writesExecuted)));
  json.set("writesIgnored", Json(static_cast<double>(writesIgnored)));
  json.set("scannedFull", Json(static_cast<double>(scannedFull)));
  json.set("scannedIndex", Json(static_cast<double>(scannedIndex)));
  json.set("filtered", Json(static_cast<double>(filtered)));

  if (fullCount > -1) {
    // fullCount is exceptional. it has a default value of -1 and is
    // not reported with this value
    json.set("fullCount", Json(static_cast<double>(fullCount)));
  }

  return json;
}

Json ExecutionStats::toJsonStatic() {
  Json json(Json::Object, 7);
  json.set("writesExecuted", Json(0.0));
  json.set("writesIgnored", Json(0.0));
  json.set("scannedFull", Json(0.0));
  json.set("scannedIndex", Json(0.0));
  json.set("filtered", Json(0.0));
  json.set("fullCount", Json(-1.0));
  json.set("static", Json(0.0));

  return json;
}

ExecutionStats::ExecutionStats()
    : writesExecuted(0),
      writesIgnored(0),
      scannedFull(0),
      scannedIndex(0),
      filtered(0),
      fullCount(-1) {}

ExecutionStats::ExecutionStats(arangodb::basics::Json const& jsonStats) {
  if (!jsonStats.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "stats is not an object");
  }

  writesExecuted = JsonHelper::checkAndGetNumericValue<int64_t>(
      jsonStats.json(), "writesExecuted");
  writesIgnored = JsonHelper::checkAndGetNumericValue<int64_t>(jsonStats.json(),
                                                               "writesIgnored");
  scannedFull = JsonHelper::checkAndGetNumericValue<int64_t>(jsonStats.json(),
                                                             "scannedFull");
  scannedIndex = JsonHelper::checkAndGetNumericValue<int64_t>(jsonStats.json(),
                                                              "scannedIndex");
  filtered = JsonHelper::checkAndGetNumericValue<int64_t>(jsonStats.json(),
                                                          "filtered");

  // note: fullCount is an optional attribute!
  fullCount =
      JsonHelper::getNumericValue<int64_t>(jsonStats.json(), "fullCount", -1);
}
