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


#define EVAL(ctx,slice,result) \
  { ++ctx.depth; Evaluate(ctx,slice,result); --ctx.depth; }

std::ostream& EvalDebugOut(size_t depth) {
  for(size_t i = 0; i<depth; ++i) {
    std::cerr << " ";
  }
  return std::cerr;
}


void Apply(EvalContext& ctx, std::string const& function,
           VPackSlice const params, VPackBuilder& result) {
  EvalDebugOut(ctx.depth) << "APPLY IN: " << function << " " << params.toJson()
                          << std::endl;
  if (params.isArray()) {
    auto f = primitives.find(function);
    if (f != primitives.end()) {
      f->second(ctx, params, result);
    } else {
      std::cerr << "primitive not found " << function << std::endl;
      std::abort();
    }

  } else {
    EvalDebugOut(ctx.depth) << "either function params is not an array: " << std::endl;
    EvalDebugOut(ctx.depth) << params.toJson() << std::endl;
    std::abort();
  }

  if (result.isClosed()) {
    EvalDebugOut(ctx.depth) << "APPLY RESULT: " << result.toJson() << std::endl;
  } else {
    EvalDebugOut(ctx.depth)
        << "APPLY RESULT: (not printable because not sealed object)" << std::endl;
  }
}

void PrepareParameters(EvalContext& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  { VPackArrayBuilder builder(&result);
    for(; paramIterator.valid(); ++paramIterator) {
      EvalDebugOut(ctx.depth) << "PARAM: " << (*paramIterator).toJson() << std::endl;
      EVAL(ctx, *paramIterator, result);
    }
  }
}

void SpecialIf(EvalContext& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  auto conditions = *paramIterator;
  if (conditions.isArray()) {

    for (auto&& e : ArrayIterator(conditions)) {
      auto&& [cond, body] = unpackTuple<VPackSlice, VPackSlice>(e);

      VPackBuilder condResult;
      EVAL(ctx, cond, condResult);
      EvalDebugOut(ctx.depth) << "IF condition evaluated to `" << condResult.toJson() << "` which is interpreted as " << std::boolalpha << !condResult.slice().isFalse() << std::endl;
      if (!condResult.slice().isFalse()) {
        EVAL(ctx, body, result);
        return;
      }

      result.add(VPackSlice::noneSlice());
    }
  } else {
    EvalDebugOut(ctx.depth) << "IF expected conditions to be array, found: " << conditions.toJson();
    std::abort();
  }
}

void SpecialList(EvalContext& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  VPackArrayBuilder array(&result);
  result.add(paramIterator);
}

void Evaluate(EvalContext& ctx, VPackSlice const slice, VPackBuilder& result) {
  EvalDebugOut(ctx.depth) << "EVAL IN: " << slice.toJson() << std::endl;
  if (slice.isArray()) {
      auto paramIterator = ArrayIterator(slice);

      VPackBuilder functionBuilder;
      EvalDebugOut(ctx.depth) << "FUNC: " << std::endl;
      EVAL(ctx, *paramIterator, functionBuilder);
      ++paramIterator;
      VPackSlice functionSlice = functionBuilder.slice();
      if (functionSlice.isString()) {
        // check for special forms
        if (functionSlice.isEqualString("if")) {
          SpecialIf(ctx, paramIterator, result);
        } else if (functionSlice.isEqualString("quote")) {
          SpecialList(ctx, paramIterator, result);
        } else {
          VPackBuilder paramBuilder;
          PrepareParameters(ctx, paramIterator, paramBuilder);
          Apply(ctx, functionSlice.copyString(), paramBuilder.slice(), result);
        }
      } else {
        EvalDebugOut(ctx.depth) << "function is not a string, found " << functionSlice.toJson();
        std::abort();
      }
  } else {
    EvalDebugOut(ctx.depth) << "LIT: " << std::endl;
    result.add(slice);
  }
//   EvalDebugOut(ctx.depth) << "EVAL RESULT: " << result.toJson() << std::endl;
}
