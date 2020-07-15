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

// #include "Basics/debugging.h"
#include "Evaluator.h"
#include <iostream>

#include <velocypack/Iterator.h>

using namespace arangodb::velocypack;

std::unordered_map<std::string, std::function<void(EvalContext& ctx, VPackSlice const slice, VPackBuilder& result)>> primitives;

void Prim_Banana(EvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
    auto tmp = int64_t{0};
    for (auto p : ArrayIterator(params)) {
        tmp += p.getNumericValue<int64_t>();
    }
    result.add(VPackValue(tmp));
}

void RegisterPrimitives() {
    primitives["banana"] = Prim_Banana;
}

void InitInterpreter() {
    RegisterPrimitives();
}

void Apply(EvalContext& ctx, VPackSlice const function, VPackSlice const params, VPackBuilder& result) {
    if (function.isString() && params.isArray()) {
        auto f = primitives.find(function.copyString());
        if (f != primitives.end()) {
            f->second(ctx, params, result);
        } else {
            std::cerr << "primitive not found " << function.toJson() << std::endl;
            std::abort();
        }
    } else {
        std::cerr << "either function is not a string or params is not an array";
        std::abort();
    }
}

void Evaluate(EvalContext& ctx, VPackSlice const slice, VPackBuilder& result) {
  if (slice.isArray()) {
      std::cerr << "function application" << std::endl;

      auto paramIterator = ArrayIterator(slice);

      VPackBuilder functionBuilder;
      Evaluate(ctx, *paramIterator, functionBuilder);
      ++paramIterator;
      std::cerr << "function slice: " << functionBuilder.toJson() << std::endl;

      std::cerr << "it: " << paramIterator;
      VPackBuilder paramBuilder;
      { VPackArrayBuilder builder(&paramBuilder);
          for(; paramIterator.valid(); ++paramIterator) {
              std::cerr << " parameter: " << (*paramIterator).toJson() << "(from: " << paramIterator << ")" << std::endl;
              Evaluate(ctx, *paramIterator, paramBuilder);
          }
      }

      Apply(ctx, functionBuilder.slice(), paramBuilder.slice(), result);
  } else {
      std::cerr << "literal " << std::endl;
      result.add(slice);
  }
}
