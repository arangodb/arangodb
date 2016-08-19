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

ShortestPathOptions::ShortestPathOptions(VPackSlice const& slice) {
  VPackSlice obj = slice.get("shortestpathFlags");
  
  weightAttribute = "";
  if (obj.hasKey("weightAttribute")) {
    VPackSlice v = obj.get("weightAttribute");
    if (v.isString()) {
      weightAttribute = v.copyString();
    }
  }
  
  defaultWeight = 1;
  if (obj.hasKey("defaultWeight")) {
    VPackSlice v = obj.get("defaultWeight");
    if (v.isNumber()) {
      defaultWeight = v.getNumericValue<double>();
    }
  }
}

void ShortestPathOptions::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);
  builder.add("weightAttribute", VPackValue(weightAttribute));
  builder.add("defaultWeight", VPackValue(defaultWeight));
}
