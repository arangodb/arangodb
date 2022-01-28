////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#include "GraphSpecification.h"
#include "Utils.h"
#include "velocypack/Iterator.h"
#include "Basics/debugging.h"

namespace arangodb::pregel3 {
auto GraphSpecification::fromVelocyPack(VPackSlice slice)
    -> GraphSpecification {
  if (slice.isString()) {
    // slice is a String
    return GraphSpecification(slice.copyString());

  } else if (!slice.isObject()) {
    // todo: trow an appropriate error with Utils::wrongGraphSpecErrMsg
    return GraphSpecification("");
  } else {
    // slice is an object
    if (slice.length() != 2) {
      // todo: trow an appropriate error with Utils::wrongGraphSpecErrMsg
      //  + "(It is an object of length " + slice.length() + ".)"
    }

    GraphSpecificationByCollections gSBC;

    // vertex collections names
    if (!slice.hasKey(Utils::vertexCollNames)) {
      // todo: trow an appropriate error with Utils::wrongGraphSpecErrMsg
      //  + "(It is an object without "
      //    "key \'" + Utils::vertexCollNames + "\'.)"
    }
    {
      VPackSlice vertexCollNamesSlice = slice.get(Utils::vertexCollNames);
      if (!vertexCollNamesSlice.isArray()) {
        // todo: trow an appropriate error with Utils::wrongGraphSpecErrMsg
        //  + "the value of \'" + Utils::vertexCollNames + \' "
        //    "is not an array."
      }
      // fill in vertex collections names
      for (auto const vCollName : VPackArrayIterator(vertexCollNamesSlice)) {
        if (!vCollName.isString()) {
          // todo: trow an appropriate error with Utils::wrongGraphSpecErrMsg
        }
        gSBC.vertexCollectionNames.emplace_back(vCollName.copyString());
      }
    }

    // edge collections names
    if (!slice.hasKey(Utils::edgeCollNames)) {
      // todo: trow an appropriate error with Utils::wrongGraphSpecErrMsg
      //  + "(It is an object without "
      //    "key \'" + Utils::edgeCollNames + "\'.)"
    }
    VPackSlice edgeCollNamesSlice = slice.get(Utils::edgeCollNames);
    if (!edgeCollNamesSlice.isArray()) {
      // todo: trow an appropriate error with Utils::wrongGraphSpecErrMsg
      //  + "the value of \'" + Utils::edgeCollNames + \' "
      //    "is not an array."
    }
    // fill in edge collections names
    for (auto const eCollName : VPackArrayIterator(edgeCollNamesSlice)) {
      if (!eCollName.isString()) {
        // todo: trow an appropriate error with Utils::wrongGraphSpecErrMsg
      }
      gSBC.edgeCollectionNames.emplace_back(eCollName.copyString());
    }

    return GraphSpecification(std::move(gSBC));
  }
}
auto GraphSpecification::toVelocyPack() -> VPackBuilder {
  return VPackBuilder{};  // todo
}
}  // namespace arangodb::pregel3
