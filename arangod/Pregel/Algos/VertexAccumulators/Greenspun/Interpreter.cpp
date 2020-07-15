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

std::ostream& EvalDebugOut(size_t depth) {
  for(size_t i = 0; i<depth; ++i) {
    std::cerr << " ";
  }
  return std::cerr;
}

void Apply(EvalContext& ctx, VPackSlice const function, VPackSlice const params, VPackBuilder& result) {
  EvalDebugOut(ctx.depth) << "APPLY IN: " << function.toJson() << " " << params.toJson() << std::endl;
  if (function.isString() && params.isArray()) {
        auto f = primitives.find(function.copyString());
        if (f != primitives.end()) {
            f->second(ctx, params, result);
        } else {
          EvalDebugOut(ctx.depth) << "primitive not found " << function.toJson() << std::endl;
            std::abort();
        }
    } else {
      EvalDebugOut(ctx.depth)
        << "either function is not a string or params is not an array: " << std::endl;
      EvalDebugOut(ctx.depth) << function.toJson() << std::endl;
      EvalDebugOut(ctx.depth) << params.toJson() << std::endl;
      std::abort();
    }

  if (result.isClosed()) {
    EvalDebugOut(ctx.depth) << "APPLY RESULT: " << result.toJson() << std::endl;
  } else {
    EvalDebugOut(ctx.depth) << "APPLY RESULT: (not printable because not sealed object)" << std::endl;
  }
}

#define EVAL(ctx,slice,result) \
  { ++ctx.depth; Evaluate(ctx,slice,result); --ctx.depth; }

void Evaluate(EvalContext& ctx, VPackSlice const slice, VPackBuilder& result) {
  EvalDebugOut(ctx.depth) << "EVAL IN: " << slice.toJson() << std::endl;
  if (slice.isArray()) {
      auto paramIterator = ArrayIterator(slice);

      VPackBuilder functionBuilder;

      EvalDebugOut(ctx.depth) << "FUNC: " << std::endl;
      EVAL(ctx, *paramIterator, functionBuilder);

      ++paramIterator;

      EvalDebugOut(ctx.depth) << "PARM: " << std::endl;
      VPackBuilder paramBuilder;
      { VPackArrayBuilder builder(&paramBuilder);
          for(; paramIterator.valid(); ++paramIterator) {
              EVAL(ctx, *paramIterator, paramBuilder);
          }
      }

      Apply(ctx, functionBuilder.slice(), paramBuilder.slice(), result);
  } else {
    EvalDebugOut(ctx.depth) << "LIT: " << std::endl;
    result.add(slice);
  }
//   EvalDebugOut(ctx.depth) << "EVAL RESULT: " << result.toJson() << std::endl;
}
