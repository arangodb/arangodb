////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "VelocypackUtils/VelocyPackStringLiteral.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Slice.h>

#include <string>
#include <vector>

namespace arangodb::tests {

using velocypack::operator""_vpack;

inline VPackString minimalVectorDefinition() {
  return R"({"type":"vector","fields":["embedding"],)"
         R"("params":{"dimension":2,"metric":"l2"}})"_vpack;
}

inline VPackString fullVectorDefinition() {
  return R"({"type":"vector","name":"vec","fields":["embedding"],)"
         R"("sparse":true,"estimates":true,"inBackground":true,)"
         R"("parallelism":3,"storedValues":["a","b.c"],)"
         R"("params":{"dimension":4,"metric":"cosine","nLists":3,)"
         R"("factory":"IVF3,Flat","trainingIterations":10,)"
         R"("defaultNProbe":2,"numberOfDocsPerCentroid":50}})"_vpack;
}

inline VPackBuilder withoutKeys(VPackSlice slice,
                                std::vector<std::string> const& keys) {
  return VPackCollection::remove(slice, keys);
}

}  // namespace arangodb::tests
