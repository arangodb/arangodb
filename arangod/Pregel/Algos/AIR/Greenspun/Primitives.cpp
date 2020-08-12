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
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>

#include "Extractor.h"
#include "Interpreter.h"
#include "Primitives.h"

namespace arangodb {
namespace greenspun {

EvalResult Prim_Banana(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
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

EvalResult Prim_Sub(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
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

EvalResult Prim_Mul(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
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

EvalResult Prim_Div(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
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
EvalResult Prim_CmpHuh(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
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
      if constexpr (!std::is_same_v<T, std::equal_to<>> &&
                    !std::is_same_v<T, std::not_equal_to<>>) {
        return EvalError("There is no order on booleans");
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
      if constexpr (!std::is_same_v<T, std::equal_to<>> &&
                    !std::is_same_v<T, std::not_equal_to<>>) {
        return EvalError("There is no order on strings implemented");
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

EvalResult Prim_VarRef(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (params.length() == 1) {
    auto nameSlice = params.at(0);
    if (nameSlice.isString()) {
      return ctx.getVariable(nameSlice.copyString(), result);
    }
  }
  return EvalError("expecting a single string parameter, found " + params.toJson());
}

EvalResult Prim_VarSet(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (!params.isArray() && params.length() != 2) {
    return EvalError("expected exactly two parameters");
  }

  auto&& [key, slice] = unpackTuple<VPackSlice, VPackSlice>(params);
  if (!slice.isObject()) {
    return EvalError("expect second parameter to be an object");
  }

  if (key.isString()) {
    return ctx.setVariable(key.copyString(), slice);
  } else {
    return EvalError("expect first parameter to be a string");
  }
  return {};
}

EvalResult Prim_AttribRef(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
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

EvalResult Prim_AttribSet(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  // TODO: implement me
  std::abort();
  return {};
}

EvalResult Prim_StringCat(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
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

EvalResult Prim_ListCat(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
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
void print(std::string msg) { std::cout << " >> LOG: " << msg << std::endl; }

EvalResult Prim_IntToStr(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
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

EvalResult Prim_FalseHuh(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (params.length() != 1) {
    return EvalError("expected a single argument");
  }
  result.add(VPackValue(ValueConsideredFalse(params.at(0))));
  return {};
}

EvalResult Prim_TrueHuh(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (params.length() != 1) {
    return EvalError("expected a single argument");
  }
  result.add(VPackValue(ValueConsideredTrue(params.at(0))));
  return {};
}

EvalResult Prim_Not(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (params.length() != 1) {
    return EvalError("expected a single argument");
  }
  result.add(VPackValue(ValueConsideredFalse(params.at(0))));
  return {};
}

// TODO: Ugly
namespace {
std::string paramsToString(VPackSlice const params) {
  std::stringstream ss;

  for (auto&& p : VPackArrayIterator(params)) {
    if (p.isString()) {
      ss << p.stringView();
    } else if (p.isNumber()) {
      ss << p.getNumber<double>();
    } else if (p.isBool()) {
      ss << std::boolalpha << p.getBool();
    } else {
      ss << p.toJson();
    }
    ss << " ";
  }
  return ss.str();
}
}  // namespace

EvalResult Prim_PrintLn(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  std::cerr << paramsToString(params) << std::endl;
  result.add(VPackSlice::noneSlice());
  return {};
}

EvalResult Prim_Error(Machine& ctx, VPackSlice const params,
                      VPackBuilder& result) {
  return EvalError(paramsToString(params));
}

EvalResult Prim_List(Machine& ctx, VPackSlice const params,
                     VPackBuilder& result) {
  VPackArrayBuilder ab(&result);
  result.add(VPackArrayIterator(params));
  return {};
}

EvalResult Prim_Dict(Machine& ctx, VPackSlice const params,
                     VPackBuilder& result) {
  VPackObjectBuilder ob(&result);
  for (auto&& pair : VPackArrayIterator(params)) {
    if (pair.length() == 2) {
      if (pair.at(0).isString()) {
        result.add(pair.at(0).stringRef(), pair.at(1));
        continue;
      }
    }

    return EvalError("expected pairs of string and slice");
  }
  return {};
}

EvalResult Prim_Lambda(Machine& ctx, VPackSlice const paramsList,
                     VPackBuilder& result) {
  VPackArrayIterator paramIterator(paramsList);
  if (!paramIterator.valid()) {
    return EvalError("lambda requires two arguments: a list of argument names and a body");
  }

  auto captures = *paramIterator++;
  if (captures.isArray()) {
    for (auto&& name : VPackArrayIterator(captures)) {
      if (!name.isString()) {
        return EvalError("in capture list: expected name, found: " + name.toJson());
      }
    }
  }

  auto params = *paramIterator++;
  if (params.isArray()) {
    for (auto&& name : VPackArrayIterator(params)) {
      if (!name.isString()) {
        return EvalError("in parameter list: expected name, found: " + name.toJson());
      }
    }
  }

  if (!paramIterator.valid()) {
    return EvalError("missing body");
  }

  auto body = *paramIterator++;
  if (paramIterator.valid()) {
    return EvalError("to many arguments to lambda constructor");
  }

  {
    VPackObjectBuilder ob(&result);
    result.add("_params", params);
    result.add("_call", body);
    {
      VPackObjectBuilder cob(&result, "_captures");
      for (auto&& name : VPackArrayIterator (captures)) {
        result.add(name);
        if (auto res = ctx.getVariable(name.copyString(), result); res.fail()) {
          return res;
        }
      }
    }
  }
  return {};
}

void RegisterFunction(Machine& ctx, std::string_view name, Machine::function_type&& f) {
  ctx.setFunction(name, std::move(f));
}

void RegisterAllPrimitives(Machine& ctx) {
  // Calculation operators
  ctx.setFunction("banana", Prim_Banana);
  ctx.setFunction("+", Prim_Banana);
  ctx.setFunction("-", Prim_Sub);
  ctx.setFunction("*", Prim_Mul);
  ctx.setFunction("/", Prim_Div);

  // Logical operators
  ctx.setFunction("not", Prim_Not);  // unary
  ctx.setFunction("false?", Prim_FalseHuh);
  ctx.setFunction("true?", Prim_TrueHuh);

  // Comparison operators
  ctx.setFunction("eq?", Prim_CmpHuh<std::equal_to<>>);
  ctx.setFunction("gt?", Prim_CmpHuh<std::greater<>>);
  ctx.setFunction("ge?", Prim_CmpHuh<std::greater_equal<>>);
  ctx.setFunction("le?", Prim_CmpHuh<std::less_equal<>>);
  ctx.setFunction("lt?", Prim_CmpHuh<std::less<>>);
  ctx.setFunction("ne?", Prim_CmpHuh<std::not_equal_to<>>);

  // Debug operators
  ctx.setFunction("print", Prim_PrintLn);

  // Lambdas
  ctx.setFunction("lambda", Prim_Lambda);

  // Utilities
  ctx.setFunction("list-cat", Prim_ListCat);
  ctx.setFunction("string-cat", Prim_StringCat);
  ctx.setFunction("int-to-str", Prim_IntToStr);

  // Access operators
  ctx.setFunction("attrib-ref", Prim_AttribRef);
  ctx.setFunction("attrib-set!", Prim_AttribSet);

  ctx.setFunction("var-ref", Prim_VarRef);
  ctx.setFunction("var-set!", Prim_VarSet);

  // TODO: We can just register bind parameters as variables (or a variable)
  ctx.setFunction("bind-ref", Prim_VarRef);
}


}  // namespace greenspun
}  // namespace arangodb
