////////////////////////////////////////////////////////////////////////////////
/// @brief AQL, data-modification query options
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ModificationOptions.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;
using JsonHelper = triagens::basics::JsonHelper;

ModificationOptions::ModificationOptions (Json const& json) {
  Json array = json.get("modificationFlags");
  ignoreErrors = JsonHelper::getBooleanValue(array.json(), "ignoreErrors", false);
  waitForSync = JsonHelper::getBooleanValue(array.json(), "waitForSync", false);
  nullMeansRemove = JsonHelper::getBooleanValue(array.json(), "nullMeansRemove", false);
  mergeArrays = JsonHelper::getBooleanValue(array.json(), "mergeArrays", false);
}

void ModificationOptions::toJson (triagens::basics::Json& json,
                                  TRI_memory_zone_t* zone) const {
  Json flags;
  flags = Json(Json::Array, 3)
    ("ignoreErrors", Json(ignoreErrors))
    ("waitForSync", Json(waitForSync))
    ("nullMeansRemove", Json(nullMeansRemove))
    ("mergeArrays", Json(mergeArrays));

  json ("modificationFlags", flags);
}


