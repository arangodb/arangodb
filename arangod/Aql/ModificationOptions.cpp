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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ModificationOptions.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;
using Json = arangodb::basics::Json;
using JsonHelper = arangodb::basics::JsonHelper;

ModificationOptions::ModificationOptions(Json const& json) {
  Json obj = json.get("modificationFlags");

  ignoreErrors = JsonHelper::getBooleanValue(obj.json(), "ignoreErrors", false);
  waitForSync = JsonHelper::getBooleanValue(obj.json(), "waitForSync", false);
  nullMeansRemove =
      JsonHelper::getBooleanValue(obj.json(), "nullMeansRemove", false);
  mergeObjects = JsonHelper::getBooleanValue(obj.json(), "mergeObjects", true);
  ignoreDocumentNotFound =
      JsonHelper::getBooleanValue(obj.json(), "ignoreDocumentNotFound", false);
  readCompleteInput =
      JsonHelper::getBooleanValue(obj.json(), "readCompleteInput", true);
}

void ModificationOptions::toJson(arangodb::basics::Json& json,
                                 TRI_memory_zone_t* zone) const {
  Json flags;

  flags = Json(Json::Object, 6)("ignoreErrors", Json(ignoreErrors))(
      "waitForSync", Json(waitForSync))("nullMeansRemove",
                                        Json(nullMeansRemove))(
      "mergeObjects", Json(mergeObjects))("ignoreDocumentNotFound",
                                          Json(ignoreDocumentNotFound))(
      "readCompleteInput", Json(readCompleteInput));

  json("modificationFlags", flags);
}

void ModificationOptions::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);
  builder.add("ignoreErrors", VPackValue(ignoreErrors));
  builder.add("waitForSync", VPackValue(waitForSync));
  builder.add("nullMeansRemove", VPackValue(nullMeansRemove));
  builder.add("mergeObjects", VPackValue(mergeObjects));
  builder.add("ignoreDocumentNotFound", VPackValue(ignoreDocumentNotFound));
  builder.add("readCompleteInput", VPackValue(readCompleteInput));
}
