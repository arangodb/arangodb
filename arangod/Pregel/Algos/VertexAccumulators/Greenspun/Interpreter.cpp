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
#include "Interpreter.h"
#include "Primitives.h"

#include <iostream>

#include <velocypack/Iterator.h>

using namespace arangodb::velocypack;

void InitInterpreter() {
    RegisterPrimitives();
}

void Apply(EvalContext& ctx, std::string const& function,
           VPackSlice const params, VPackBuilder& result) {
  auto f = primitives.find(function);
  if (f != primitives.end()) {
    f->second(ctx, params, result);
  } else {
    std::cerr << "primitive not found " << function << std::endl;
    std::abort();
  }
}

void PrepareParameters(EvalContext& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  { VPackArrayBuilder builder(&result);
    for(; paramIterator.valid(); ++paramIterator) {
      std::cerr << " parameter: " << (*paramIterator).toJson() << std::endl;
      Evaluate(ctx, *paramIterator, result);
    }
  }
}

void SpecialIf(EvalContext& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  std::cerr << "Special form `if`" << std::endl;
  auto conditions = *paramIterator;
  std::cerr << "Conditions are " << conditions.toJson() << std::endl;
  if (conditions.isArray()) {

    for (auto&& e : ArrayIterator(conditions)) {
      std::cerr << "pair " << e.toJson() << std::endl;

      auto&& [cond, body] = unpackTuple<VPackSlice, VPackSlice>(e);


      VPackBuilder condResult;
      Evaluate(ctx, cond, condResult);
      std::cerr << "condition evaluated to " << condResult.toJson() << " which is seen as " << !condResult.slice().isFalse() << std::endl;
      if (!condResult.slice().isFalse()) {
        Evaluate(ctx, body, result);
        return;
      }

      result.add(VPackSlice::noneSlice());
    }
  } else {
    std::cerr << "expected conditions to be array, found: " << conditions.toJson();
    std::abort();
  }
}

void SpecialList(EvalContext& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  VPackArrayBuilder array(&result);
  result.add(paramIterator);
}

void Evaluate(EvalContext& ctx, VPackSlice const slice, VPackBuilder& result) {
  if (slice.isArray()) {

      auto paramIterator = ArrayIterator(slice);

      VPackBuilder functionBuilder;
      Evaluate(ctx, *paramIterator, functionBuilder);
      ++paramIterator;
      std::cerr << "function slice: " << functionBuilder.toJson() << std::endl;

      VPackSlice functionSlice = functionBuilder.slice();
      if (functionSlice.isString()) {

        // check for special forms
        if (functionSlice.isEqualString("if")) {
          SpecialIf(ctx, paramIterator, result);
        } else if (functionSlice.isEqualString("quote")) {
          SpecialList(ctx, paramIterator, result);
        } else {
          std::cerr << "function application" << std::endl;
          VPackBuilder paramBuilder;
          PrepareParameters(ctx, paramIterator, paramBuilder);
          Apply(ctx, functionSlice.copyString(), paramBuilder.slice(), result);
        }
      } else {
        std::cerr << "function is not a string, found " << functionSlice.toJson();
        std::abort();
      }
  } else {
      std::cerr << "literal " << std::endl;
      result.add(slice);
  }
}
