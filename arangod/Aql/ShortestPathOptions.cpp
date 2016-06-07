////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ShortestPathOptions.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;
using Json = arangodb::basics::Json;
using JsonHelper = arangodb::basics::JsonHelper;

ShortestPathOptions::ShortestPathOptions(Json const& json) {
  Json obj = json.get("shortestpathFlags");
  weightAttribute = JsonHelper::getStringValue(obj.json(), "weightAttribute", "");
  defaultWeight = JsonHelper::getNumericValue<double>(obj.json(), "defaultWeight", 1);
}

void ShortestPathOptions::toJson(arangodb::basics::Json& json,
                              TRI_memory_zone_t* zone) const {
  Json flags;
  flags = Json(Json::Object, 2);
  flags("weightAttribute", Json(weightAttribute));
  flags("defaultWeight", Json(defaultWeight));
  json("shortestPathFlags", flags);
}

void ShortestPathOptions::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);
  builder.add("weightAttribute", VPackValue(weightAttribute));
  builder.add("defaultWeight", VPackValue(defaultWeight));
}
