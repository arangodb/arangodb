////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "Basics/debugging.h"
#include "Pregel/Algos/VertexAccumulators/Evaluator.h"

#include <velocypack/Iterator.h>

using namespace arangodb::velocypack;

void Evaluate(EvalContext& ctx, VPackSlice const slice, VPackBuilder& result) {
  VPackBuilder params;

  if (slice.isArray()) {
    auto paramIterator = ArrayIterator(slice);
    auto funcName = *paramIterator;
    paramIterator++;

    { VPackArrayBuilder builder(&params);
      for(auto e : paramIterator) {
        Evaluate(ctx, e, params);
      }
    }

    TRI_ASSERT(funcName.isString());

    auto func = primitives.find(funcName.copyString());
    if (func != primitives.end()) {
      func->second(ctx, params.slice(), result);
    } else {
      // FIXME! Could not find the function.
      std::abort();
    }

  } else {
    result.add(slice);
  }
}
