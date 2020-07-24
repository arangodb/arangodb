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

#include <Basics/VelocyPackHelper.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>

#include "Interpreter.h"
#include "Primitives.h"

using PrimitiveFunction = std::function<EvalResult(PrimEvalContext& ctx, VPackSlice const slice, VPackBuilder& result)>;
std::unordered_map<std::string, PrimitiveFunction> primitives;

EvalResult Prim_Banana(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto tmp = double{0};
  for (auto p : VPackArrayIterator(params)) {
    if (p.isNumber<double>()) {
      tmp += p.getNumericValue<double>();
    } else {
      return EvalError("expected double, found: " + p.toJson());
    }
  }
  result.add(VPackValue(tmp));
  return {};
}

EvalResult Prim_Sub(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto tmp = double{0};
  auto iter = VPackArrayIterator(params);
  if (iter.valid()) {
    if (!(*iter).isNumber<double>()) {
      return EvalError("expected double, found: " + (*iter).toJson());
    }
    tmp = (*iter).getNumericValue<double>();
    iter++;
    for (; iter.valid(); iter++) {
      if (!(*iter).isNumber<double>()) {
        return EvalError("expected double, found: " + (*iter).toJson());
      }
      tmp -= (*iter).getNumericValue<double>();
    }
  }
  result.add(VPackValue(tmp));
  return {};
}

EvalResult Prim_Mul(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto tmp = double{1};
  for (auto p : VPackArrayIterator(params)) {
    if (!p.isNumber<double>()) {
      return EvalError("expected double, found: " + p.toJson());
    }
    tmp *= p.getNumericValue<double>();
  }
  result.add(VPackValue(tmp));
  return {};
}

EvalResult Prim_Div(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto tmp = double{1};
  auto iter = VPackArrayIterator(params);
  if (iter.valid()) {
    if (!(*iter).isNumber<double>()) {
      return EvalError("expected double, found: " + (*iter).toJson());
    }
    tmp = (*iter).getNumericValue<double>();
    iter++;
    for (; iter.valid(); iter++) {
      if (!(*iter).isNumber<double>()) {
        return EvalError("expected double, found: " + (*iter).toJson());
      }
      auto v = (*iter).getNumericValue<double>();
      if (v == 0) {
        return EvalError("division by zero");
      }
      tmp /= v;
    }
  }
  result.add(VPackValue(tmp));
  return {};
}

template <typename T>
EvalResult Prim_CmpHuh(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto iter = VPackArrayIterator(params);
  if (iter.valid()) {
    auto proto = *iter;
    iter++;
    if (proto.isNumber()) {
      auto value = proto.getNumber<double>();
      for (; iter.valid(); iter++) {
        auto other = *iter;
        if (!other.isNumber()) {
          return EvalError("Expected numerical value at parameter " +
                           std::to_string(iter.index()) + ", found: " + other.toJson());
        }

        if (!T{}(value, other.getNumber<double>())) {
          result.add(VPackValue(false));
          return {};
        }
      }
    } else if (proto.isBool()) {
      if constexpr (!std::is_same_v<T, std::equal_to<>> && !std::is_same_v<T, std::not_equal_to<>>) {
        return EvalError("There is no relation on booleans");
      }
      auto value = proto.getBool();
      for (; iter.valid(); iter++) {
        auto other = *iter;
        if (!T{}(value, ValueConsideredTrue(other))) {
          result.add(VPackValue(false));
          return {};
        }
      }
    } else if (proto.isString()) {
      if constexpr (!std::is_same_v<T, std::equal_to<>> && !std::is_same_v<T, std::not_equal_to<>>) {
        return EvalError("There is no relation on strings implemented");
      }
      auto value = proto.stringView();
      for (; iter.valid(); iter++) {
        auto other = *iter;
        if (!other.isString()) {
          return EvalError("Expected string value at parameter " +
                           std::to_string(iter.index()) + ", found: " + other.toJson());
        }
        if (!T{}(value, other.stringView())) {
          result.add(VPackValue(false));
          return {};
        }
      }
    } else {
      return EvalError("Cannot compare values of given type, found: " + proto.toJson());
    }
  }
  result.add(VPackValue(true));
  return {};
}

EvalResult Prim_VarRef(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  if (params.length() == 1) {
    auto nameSlice = params.at(0);
    if (nameSlice.isString()) {
      return ctx.getVariable(nameSlice.copyString(), result);
    }
  }
  return EvalError("expecting a single string parameter, found " + params.toJson());
}

EvalResult Prim_Attrib(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  if (!params.isArray() && params.length() != 2) {
    return EvalError("expected exactly two parameters");
  }

  auto&& [key, slice] = unpackTuple<VPackSlice, VPackSlice>(params);
  if (!slice.isObject()) {
    return EvalError("expect second parameter to be an object");
  }

  if (key.isString()) {
    result.add(slice.get(key.stringRef()));
  } else if (key.isArray()) {
    std::vector<VPackStringRef> path;
    for (auto&& pathStep : VPackArrayIterator(key)) {
      if (!pathStep.isString()) {
        return EvalError("expected string in key arrays");
      }
      path.emplace_back(pathStep.stringRef());
    }
    result.add(slice.get(path));
  } else {
    return EvalError("key is neither array nor string");
  }
  return {};
}

EvalResult Prim_This(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  result.add(VPackValue(ctx.getThisId()));
  return {};
}

EvalResult Prim_AccumRef(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto&& [accumId] = unpackTuple<std::string_view>(params);
  ctx.getAccumulatorValue(accumId, result);
  return {};
}

EvalResult Prim_Update(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto&& [accumId, toId, value] =
      unpackTuple<std::string_view, std::string_view, VPackSlice>(params);

  ctx.updateAccumulator(accumId, toId, value);
  return {};
}

EvalResult Prim_Set(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto&& [accumId, value] = unpackTuple<std::string_view, VPackSlice>(params);
  ctx.setAccumulator(accumId, value);
  return {};
}

EvalResult Prim_For(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto&& [dir, vars, body] = unpackTuple<std::string_view, VPackSlice, VPackSlice>(params);
  auto&& [edgeVar] = unpackTuple<std::string>(vars);

  // TODO translate direction and pass to enumerateEdges
  return ctx.enumerateEdges([&, edgeVar = edgeVar, body = body](VPackSlice edge) {
    StackFrameGuard<true> guard(ctx);
    ctx.setVariable(edgeVar, edge);
    VPackBuilder localResult;
    return Evaluate(ctx, body, localResult);
  });
}

EvalResult Prim_StringCat(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  std::string str;

  for (auto iter = VPackArrayIterator(params); iter.valid(); iter++) {
    VPackSlice p = *iter;
    if (p.isString()) {
      str += p.stringView();
    } else {
      return EvalError("expected string, found " + p.toJson());
    }
  }

  result.add(VPackValue(str));
  return {};
}

EvalResult Prim_ListCat(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  VPackArrayBuilder array(&result);
  for (auto iter = VPackArrayIterator(params); iter.valid(); iter++) {
    VPackSlice p = *iter;
    if (p.isArray()) {
      result.add(VPackArrayIterator(p));
    } else {
      return EvalError("expected array, found " + p.toJson());
    }
  }

  return {};
}

// TODO: Only for debugging purpose. Can be removed later again.
void print(std::string msg) {
  std::cout << " >> LOG: " <<  msg << std::endl;
}

EvalResult Prim_IntToStr(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  if (params.length() != 1) {
    return EvalError("expected a single argument");
  }
  auto value = params.at(0);
  if (!value.isNumber<int64_t>()) {
    return EvalError("expected int, found: " + value.toJson());
  }

  result.add(VPackValue(std::to_string(value.getNumericValue<int64_t>())));
  return {};
}

EvalResult Prim_FalseHuh(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  if (params.length() != 1) {
    return EvalError("expected a single argument");
  }
  result.add(VPackValue(ValueConsideredFalse(params.at(0))));
  return {};
}

EvalResult Prim_TrueHuh(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  if (params.length() != 1) {
    return EvalError("expected a single argument");
  }
  result.add(VPackValue(ValueConsideredTrue(params.at(0))));
  return {};
}

EvalResult Prim_Not(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  if (params.length() != 1) {
    return EvalError("expected a single argument");
  }
  result.add(VPackValue(ValueConsideredTrue(params.at(0))));
  return {};
}

EvalResult Prim_PrintLn(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  for (auto&& p : VPackArrayIterator(params)) {
    if (p.isString()) {
      std::cout << p.stringView();
    } else if (p.isNumber()) {
      std::cout << p.getNumber<double>();
    } else {
      std::cout << p.toJson();
    }
    std::cout << " ";
  }

  std::cout << std::endl;

  result.add(VPackSlice::noneSlice());
  return {};
}

EvalResult Prim_BindRef(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  if (params.length() == 1) {
    VPackSlice name = params.at(0);
    if (name.isString()) {
      return ctx.getBindingValue(name.stringView(), result);
    }
  }

  return EvalError("expected a single string argument");
}

EvalResult Prim_GlobalSuperstep(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  if (params.isEmptyArray()) {
    return ctx.getGlobalSuperstep(result);
  }

  return EvalError("expected no arguments");
}

EvalResult Prim_GoToPhase(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
    if (params.length() == 1) {
        VPackSlice v = params.at(0);
        if (v.isString()) {
            return ctx.gotoPhase(v.stringView());
        }
    }

    return EvalError("expect single string argument");
}

EvalResult Prim_Finish(PrimEvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
    if (params.isEmptyArray()) {
      return ctx.finishAlgorithm();
    }

    return EvalError("expect no arguments");
}


void RegisterPrimitives() {
  // Calculation operators
  primitives["banana"] = Prim_Banana;
  primitives["+"] = Prim_Banana;
  primitives["-"] = Prim_Sub;
  primitives["*"] = Prim_Mul;
  primitives["/"] = Prim_Div;

  // Logical operators
  primitives["not"] = Prim_Not;  // unary
  primitives["false?"] = Prim_FalseHuh;
  primitives["true?"] = Prim_TrueHuh;

  // Comparison operators
  primitives["eq?"] = Prim_CmpHuh<std::equal_to<>>;
  primitives["gt?"] = Prim_CmpHuh<std::greater<>>;
  primitives["ge?"] = Prim_CmpHuh<std::greater_equal<>>;
  primitives["le?"] = Prim_CmpHuh<std::less_equal<>>;
  primitives["lt?"] = Prim_CmpHuh<std::less<>>;
  primitives["ne?"] = Prim_CmpHuh<std::not_equal_to<>>;

  // Debug operators
  primitives["print"] = Prim_PrintLn;

  primitives["list-cat"] = Prim_ListCat;
  primitives["string-cat"] = Prim_StringCat;
  primitives["int-to-str"] = Prim_IntToStr;

  // Access operators
  primitives["attrib"] = Prim_Attrib;
  primitives["var-ref"] = Prim_VarRef;
  primitives["bind-ref"] = Prim_BindRef;
  primitives["accum-ref"] = Prim_AccumRef;

  primitives["this"] = Prim_This;
  //  primitives["doc"] = Prim_Doc;
  primitives["update"] = Prim_Update;
  primitives["set"] = Prim_Set;
  primitives["for"] = Prim_For;
  primitives["global-superstep"] = Prim_GlobalSuperstep;

  primitives["goto"] = Prim_GoToPhase;
  primitives["finish"] = Prim_Finish;
}
