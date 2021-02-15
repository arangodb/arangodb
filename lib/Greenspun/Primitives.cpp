////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include <boost/range/adaptor/transformed.hpp>

#include <Basics/VelocyPackHelper.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>
#include <list>

#include "Extractor.h"
#include "Interpreter.h"
#include "Primitives.h"

namespace arangodb::greenspun {

EvalResult Prim_Min(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  bool set = false;
  auto tmp = double{0};
  for (auto p : VPackArrayIterator(params)) {
    if (p.isNumber<double>()) {
      if (!set) {
        tmp = p.getNumericValue<double>();
        set = true;
      } else {
        tmp = std::min(tmp, p.getNumericValue<double>());
      }
    } else {
      return EvalError("expected double, found: " + p.toJson());
    }
  }
  if (set) {
    result.add(VPackValue(tmp));
  } else {
    result.add(VPackSlice::nullSlice());
  }
  return {};
}

EvalResult Prim_Max(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  bool set = false;
  auto tmp = double{0};
  for (auto p : VPackArrayIterator(params)) {
    if (p.isNumber<double>()) {
      if (!set) {
        tmp = p.getNumericValue<double>();
        set = true;
      } else {
        tmp = std::max(tmp, p.getNumericValue<double>());
      }
    } else {
      return EvalError("expected double, found: " + p.toJson());
    }
  }
  if (set) {
    result.add(VPackValue(tmp));
  } else {
    result.add(VPackSlice::nullSlice());
  }
  return {};
}

EvalResult Prim_Avg(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  auto tmp = double{0};
  for (auto p : VPackArrayIterator(params)) {
    if (p.isNumber<double>()) {
      tmp += p.getNumericValue<double>();
    } else {
      return EvalError("expected double, found: " + p.toJson());
    }
  }
  auto length = params.length();
  result.add(VPackValue(length == 0 ? tmp : tmp / length));
  return {};
}

EvalResult Prim_Add(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
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

  auto&& [key, slice] = arangodb::basics::VelocyPackHelper::unpackTuple<VPackSlice, VPackSlice>(params);
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

std::list<std::string> createObjectPaths(velocypack::Slice object,
                                         std::list<std::string> currentPath) {
  for (VPackObjectIterator iter(object); iter.valid(); iter++) {
    currentPath.emplace_back(iter.key().toString());
    if (iter.value().isObject()) {
      // recursive through all available keys
      return createObjectPaths(iter.value(), currentPath);
    }
  }
  return currentPath;
}

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

EvalResult Prim_ToJsonString(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (params.length() != 1) {
    return EvalError("expected a single argument");
  }
  auto value = params.at(0);

  result.add(VPackValue(value.toJson()));

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

EvalResult Prim_Report(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  result.add(VPackSlice::nullSlice());
  return ctx.print(paramsToString(params));;
}

EvalResult Machine::print(const std::string& msg) const {
  if (printCallback) {
    printCallback(msg);
    return {};
  } else {
    return EvalError("reporting not supported in this context (message was `" + msg + "`)");
  }
}

EvalResult Prim_Error(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  return EvalError(paramsToString(params));
}

EvalResult Prim_Lambda(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  VPackArrayIterator paramIterator(paramsList);
  if (!paramIterator.valid()) {
    return EvalError(
        "lambda requires two arguments: a list of argument names and a body");
  }

  auto captures = *paramIterator++;
  if (captures.isArray()) {
    for (auto&& name : VPackArrayIterator(captures)) {
      if (!name.isString()) {
        return EvalError("in capture list: expected name, found: " + name.toJson());
      }
    }
  } else {
    return EvalError("capture list: expected array, found: " + captures.toJson());
  }

  auto params = *paramIterator++;
  if (params.isArray()) {
    for (auto&& name : VPackArrayIterator(params)) {
      if (!name.isString()) {
        return EvalError("in parameter list: expected name, found: " + name.toJson());
      }
    }
  } else {
    return EvalError("parameter list: expected array, found: " + captures.toJson());
  }

  if (!paramIterator.valid()) {
    return EvalError("missing body");
  }

  auto body = *paramIterator++;
  if (paramIterator.valid()) {
    return EvalError("too many arguments to lambda constructor");
  }

  {
    VPackObjectBuilder ob(&result);
    result.add("_params", params);
    result.add("_call", body);
    {
      VPackObjectBuilder cob(&result, "_captures");
      for (auto&& name : VPackArrayIterator(captures)) {
        result.add(name);
        if (auto res = ctx.getVariable(name.copyString(), result); res.fail()) {
          return res;
        }
      }
    }
  }
  return {};
}

EvalResult Prim_Apply(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  if (!paramsList.isArray() || paramsList.length() != 2) {
    return EvalError(
        "expected one function argument on one list of parameters");
  }

  auto functionSlice = paramsList.at(0);
  auto parameters = paramsList.at(1);
  if (!parameters.isArray()) {
    return EvalError("expected list of parameters, found: " + parameters.toJson());
  }

  return EvaluateApply(ctx, functionSlice, VPackArrayIterator(parameters), result, false);
}

EvalResult Prim_Identity(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  if (!paramsList.isArray() || paramsList.length() != 1) {
    return EvalError("expecting a single argument");
  }

  result.add(paramsList.at(0));
  return {};
}

EvalResult Prim_Map(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  if (!paramsList.isArray() || paramsList.length() != 2) {
    return EvalError("expecting two arguments, a function and a list");
  }

  auto functionSlice = paramsList.at(0);
  auto list = paramsList.at(1);

  if (list.isArray()) {
    VPackArrayBuilder ab(&result);
    for (VPackArrayIterator iter(list); iter.valid(); ++iter) {
      VPackBuilder parameter;
      {
        VPackArrayBuilder pb(&parameter);
        parameter.add(VPackValue(iter.index()));
        parameter.add(*iter);
      }

      auto res = EvaluateApply(ctx, functionSlice,
                               VPackArrayIterator(parameter.slice()), result, false);
      if (res.fail()) {
        return res.error().wrapMessage("when mapping pair " + parameter.toJson());
      }
    }
  } else if (list.isObject()) {
    VPackObjectBuilder ob(&result);
    for (VPackObjectIterator iter(list); iter.valid(); ++iter) {
      VPackBuilder parameter;
      {
        VPackArrayBuilder pb(&parameter);
        parameter.add(iter.key());
        parameter.add(iter.value());
      }

      VPackBuilder tempBuffer;

      auto res = EvaluateApply(ctx, functionSlice,
                               VPackArrayIterator(parameter.slice()), tempBuffer, false);
      if (res.fail()) {
        return res.error().wrapMessage("when mapping pair " + parameter.toJson());
      }

      result.add(iter.key());
      result.add(tempBuffer.slice());
    }
  } else {
    return EvalError("expected list or object, found: " + list.toJson());
  }

  return {};
}

EvalResult Prim_Reduce(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  if (!paramsList.isArray() || paramsList.length() < 2) {
    return EvalError(
        "expecting at least two arguments, a function and two dicts");
  }

  auto inputValue = paramsList.at(0);
  auto functionSlice = paramsList.at(1);
  auto inputAccumulator = paramsList.at(2);

  if (inputAccumulator.isNone()) {
    return EvalError("input accumulator is required but not set!");
  }

  // reduce(key, value, accumulator)
  // iter can be either VPackArrayIterator or VPackObjectIterator
  auto buildLambdaParameters = [&](VPackBuilder& parameter, auto& iter) {
    {
      VPackArrayBuilder pb(&parameter);
      if constexpr (std::is_same_v<VPackObjectIterator&, decltype(iter)>) {
        parameter.add(iter.key());  // object key
      } else {
        parameter.add(VPackValue(iter.index()));  // array index
      }

      if (iter.isFirst()) {
        parameter.add(iter.value());      // value
        parameter.add(inputAccumulator);  // accum
      } else {
        parameter.add(iter.value());    // value
        parameter.add(result.slice());  // accumulator / previous result
      }
    }
  };

  auto reduceArray = [&](auto& inputValue) -> EvalResult {
    for (VPackArrayIterator iter(inputValue); iter.valid(); ++iter) {
      VPackBuilder parameter;
      buildLambdaParameters(parameter, iter);

      result.clear();
      EvalResult res = EvaluateApply(ctx, functionSlice,
                                     VPackArrayIterator(parameter.slice()), result, false);
      if (res.fail()) {
        return res.error().wrapMessage("when reducing array parameters " +
                                       parameter.toJson());
      }
    }
    // no error
    return {};
  };

  auto reduceObject = [&](auto& inputValue) -> EvalResult {
    for (VPackObjectIterator iter(inputValue); iter.valid(); iter++) {
      VPackBuilder parameter;
      buildLambdaParameters(parameter, iter);

      result.clear();
      auto res = EvaluateApply(ctx, functionSlice,
                               VPackArrayIterator(parameter.slice()), result, false);
      if (res.fail()) {
        return res.error().wrapMessage("when reducing object parameters " +
                                       parameter.toJson());
      }
    }
    // no error
    return {};
  };

  if (inputValue.isArray()) {
    auto res = reduceArray(inputValue);
    if (res.fail()) {
      return res;
    }
  } else if (inputValue.isObject()) {
    auto res = reduceObject(inputValue);
    if (res.fail()) {
      return res;
    }
  } else {
    return EvalError("expected either object or array as input value, found: " +
                     inputValue.toJson() +
                     ". Accumulator can be any type: " + inputAccumulator.toJson() +
                     " (depends on lambda definition");
  }

  return {};
}

EvalResult Prim_Filter(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  if (!paramsList.isArray() || paramsList.length() != 2) {
    return EvalError(
        "expecting two arguments, a function and a list or object");
  }

  auto functionSlice = paramsList.at(0);
  auto list = paramsList.at(1);

  if (list.isArray()) {
    VPackArrayBuilder ab(&result);
    for (VPackArrayIterator iter(list); iter.valid(); ++iter) {
      VPackBuilder parameter;
      {
        VPackArrayBuilder pb(&parameter);
        parameter.add(VPackValue(iter.index()));
        parameter.add(*iter);
      }

      VPackBuilder filterResult;
      auto res = EvaluateApply(ctx, functionSlice,
                               VPackArrayIterator(parameter.slice()), filterResult, false);
      if (res.fail()) {
        return res.error().wrapMessage("when filtering pair " + parameter.toJson());
      }

      if (ValueConsideredTrue(filterResult.slice())) {
        result.add(*iter);
      }
    }
  } else if (list.isObject()) {
    VPackObjectBuilder ob(&result);
    for (VPackObjectIterator iter(list); iter.valid(); ++iter) {
      VPackBuilder parameter;
      {
        VPackArrayBuilder pb(&parameter);
        parameter.add(iter.key());
        parameter.add(iter.value());
      }

      VPackBuilder filterResult;
      auto res = EvaluateApply(ctx, functionSlice,
                               VPackArrayIterator(parameter.slice()), filterResult, false);
      if (res.fail()) {
        return res.error().wrapMessage("when mapping pair " + parameter.toJson());
      }
      if (ValueConsideredTrue(filterResult.slice())) {
        result.add(iter.key());
        result.add(iter.value());
      }
    }
  } else {
    return EvalError("expected list or object, found: " + list.toJson());
  }

  return {};
}

EvalResult Prim_Foldl(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  return EvalError("Prim_Foldl not implemented");
}

EvalResult Prim_Foldl1(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  return EvalError("Prim_Foldl1 not implemented");
}

EvalResult Prim_NumberHuh(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  auto res = extract<VPackSlice>(paramsList);
  if (!res) {
    return std::move(res).asResult();
  }
  auto&& [testee] = res.value();

  result.add(VPackValue(testee.isNumber()));
  return {};
}

EvalResult Prim_NullHuh(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  auto res = extract<VPackSlice>(paramsList);
  if (!res) {
    return std::move(res).asResult();
  }
  auto&& [testee] = res.value();

  result.add(VPackValue(testee.isNull()));
  return {};
}

EvalResult Prim_BoolHuh(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  auto res = extract<VPackSlice>(paramsList);
  if (!res) {
    return std::move(res).asResult();
  }
  auto&& [testee] = res.value();

  result.add(VPackValue(testee.isBool()));
  return {};
}

EvalResult Prim_Assert(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  VPackArrayIterator iter(paramsList);
  if (!iter.valid()) {
    return EvalError("expected at least one argument");
  }

  VPackSlice value = *iter;
  if (ValueConsideredFalse(value)) {
    iter++;
    std::string errorMessage = "assertion failed";
    if (iter.valid()) {
      errorMessage = paramsToString(iter);
    }
    return EvalError(errorMessage);
  }

  result.add(VPackSlice::nullSlice());
  return {};
}

double rand_source_query() {
  return static_cast<double>(std::rand()) / RAND_MAX;
}

EvalResult Prim_Rand(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  result.add(VPackValue(rand_source_query()));
  return {};
}

EvalResult Prim_RandRange(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  auto res = extract<double, double>(paramsList);
  if (!res) {
    return res.error();
  }

  auto&& [min, max] = res.value();

  auto v = min + rand_source_query() * (max - min);
  result.add(VPackValue(v));
  return {};
}

EvalResult Prim_ToJson(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  auto res = extract<VPackSlice>(paramsList);
  if (res.fail()) {
    return res.error();
  }

  auto&& [slice] = res.value();
  result.add(VPackValue(slice.toJson()));
  return {};
}

EvalResult Prim_FromJson(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  auto res = extract<std::string>(paramsList);
  if (res.fail()) {
    return res.error();
  }

  auto&& [json] = res.value();
  try {
    VPackParser parser(result);
    parser.parse(json);
    return {};
  } catch(VPackException const& e) {
    return EvalError(std::string{"failed to parse json: "} + e.what());
  }
}

void RegisterFunction(Machine& ctx, std::string_view name, Machine::function_type&& f) {
  ctx.setFunction(name, std::move(f));
}

void RegisterAllPrimitives(Machine& ctx) {
  // Calculation operators
  ctx.setFunction("banana", Prim_Add);
  ctx.setFunction("+", Prim_Add);
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

  // Misc
  ctx.setFunction("min", Prim_Min);
  ctx.setFunction("max", Prim_Max);
  ctx.setFunction("avg", Prim_Avg);

  // Debug operators
  ctx.setFunction("report", Prim_Report);
  ctx.setFunction("error", Prim_Error);
  ctx.setFunction("assert", Prim_Assert);

  // Lambdas
  ctx.setFunction("lambda", Prim_Lambda);

  // Utilities
  ctx.setFunction("int-to-str", Prim_IntToStr);
  ctx.setFunction("to-json-string", Prim_ToJsonString);

  // Functional stuff
  ctx.setFunction("id", Prim_Identity);
  ctx.setFunction("apply", Prim_Apply);
  ctx.setFunction("map", Prim_Map);  // ["map", <func(index, value) -> value>, <list>] or ["map", <func(key, value) -> value>, <dict>]
  ctx.setFunction("reduce", Prim_Reduce);  // ["reduce", value, <func(index, value, accumulator), accumulator]
  ctx.setFunction("filter", Prim_Filter);  // ["filter", <func(index, value) -> bool>, <list>] or ["filter", <func(key, value) -> bool>, <dict>]
  ctx.setFunction("foldl", Prim_Foldl);
  ctx.setFunction("foldl1", Prim_Foldl1);

  // Type-related functions
  ctx.setFunction("number?", Prim_NumberHuh);
  ctx.setFunction("null?", Prim_NullHuh);
  ctx.setFunction("bool?", Prim_BoolHuh);

  ctx.setFunction("var-ref", Prim_VarRef);
  ctx.setFunction("bind-ref", Prim_VarRef);

  ctx.setFunction("rand", Prim_Rand);
  ctx.setFunction("rand-range", Prim_RandRange);

  ctx.setFunction("to-json", Prim_ToJson);
  ctx.setFunction("from-json", Prim_FromJson);
}

}  // namespace arangodb::greenspun
