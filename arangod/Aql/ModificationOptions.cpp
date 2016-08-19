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

#include "ModificationOptions.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

ModificationOptions::ModificationOptions(VPackSlice const& slice) {
  VPackSlice obj = slice.get("modificationFlags");

  ignoreErrors = basics::VelocyPackHelper::getBooleanValue(obj, "ignoreErrors", false);
  waitForSync = basics::VelocyPackHelper::getBooleanValue(obj, "waitForSync", false);
  nullMeansRemove =
      basics::VelocyPackHelper::getBooleanValue(obj, "nullMeansRemove", false);
  mergeObjects = basics::VelocyPackHelper::getBooleanValue(obj, "mergeObjects", true);
  ignoreDocumentNotFound =
      basics::VelocyPackHelper::getBooleanValue(obj, "ignoreDocumentNotFound", false);
  readCompleteInput =
      basics::VelocyPackHelper::getBooleanValue(obj, "readCompleteInput", true);
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
