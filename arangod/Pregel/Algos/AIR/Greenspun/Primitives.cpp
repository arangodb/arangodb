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
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
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
    result.add(VPackSlice::noneSlice());
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
    result.add(VPackSlice::noneSlice());
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

EvalResult Prim_Dict(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
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

EvalResult Prim_DictKeys(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (params.length() != 1) {
    return EvalError("expected exactly one parameter");
  }

  auto obj = params.at(0);
  if (!obj.isObject()) {
    return EvalError("expected object, found: " + obj.toJson());
  }

  result.openArray();
  for (VPackObjectIterator iter(params.at(0)); iter.valid(); iter++) {
    result.add(iter.key());
  }
  result.close();

  return {};
}

std::list<std::string> createObjectPaths(velocypack::Slice object,
                                         std::list<std::string> currentPath) {
  for (VPackObjectIterator iter(object); iter.valid(); iter++) {
    if (iter.value().isObject()) {
      // recursive through all available keys
      currentPath.emplace_back(iter.key().toString());
      return createObjectPaths(iter.value(), currentPath);
    } else {
      // reached end
      currentPath.emplace_back(iter.key().toString());
    }
  }
  return currentPath;
}

void printPath(std::list<std::string>& path) {

  std::cout << "Printing current path: " << std::endl;
    std::cout << " [ ";
    for (auto const& pathElement : path) {
      std::cout << " " << pathElement << " ";
    }
    std::cout << " ] " << std::endl;
}

void createPaths(
    std::list<std::list<std::string>>& finalPaths,
    velocypack::Slice object,
    std::list<std::string>& currentPath) {
  for (VPackObjectIterator iter(object); iter.valid(); iter++) {
    std::string currentKey = iter.key().toString();  // <= a
    VPackSlice currentValue = iter.value();          // <= {b:2 , c:2}
    std::cout << "Current Key is: " << currentKey << std::endl;
    std::cout << "Current Value is: " << currentValue.toString() << std::endl;

    if (currentValue.isObject()) {
      std::cout << "We've found an object!" << std::endl;
      std::list<std::string> currentTmpPath(currentPath); //= {currentKey}; // <-- muss schon gespeichert werden
      currentTmpPath.emplace_back(currentKey);
      finalPaths.emplace_back(currentTmpPath); // ["c"] <-- das hier gespeichert in finalPath result

      // ABER -> Pfad ist nicht abgeschlossen

      currentPath.emplace_back(currentKey);

      std::cout << "We want to continue with path generation!" << std::endl;
      createPaths(finalPaths, currentValue, currentPath);
    } else {
      std::cout << "Path finalised!" << std::endl;
      // -> Pfad ist abgeschlossen!
      std::list<std::string> currentTmpPath(currentPath);
      currentTmpPath.emplace_back(currentKey);
      finalPaths.emplace_back(currentTmpPath);
    }

    if (iter.isLast()) {
      if (currentPath.size() > 0) {
        currentPath.pop_back();
      }
    }
  }
}

EvalResult Prim_DictDirectory(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (params.length() != 1) {
    return EvalError("expected exactly one parameter");
  }

  auto obj = params.at(0);
  if (!obj.isObject()) {
    return EvalError("expected object, found: " + obj.toJson());
  }
  std::cout << "TEST: " << obj.toString() << std::endl;

  std::list<std::list<std::string>> finalList;
  std::list<std::string> currentPath;
  createPaths(finalList, obj, currentPath);
  // finalList.emplace_back(createObjectPaths(obj, {}));

  std::cout << "printing incoming object " << obj.toString() << std::endl
            << std::endl;

  std::cout << "Printing final vector row by row: " << std::endl;
  for (auto const& path : finalList) {
    std::cout << " [ ";
    for (auto const& pathElement : path) {
      std::cout << " " << pathElement << " ";
    }
    std::cout << " ] " << std::endl;
  }

  return {Prim_DictKeys(ctx, params, result)};
}

EvalResult MergeObjectSlice(VPackBuilder& result, VPackSlice const& sliceA,
                            VPackSlice const& sliceB) {
  VPackCollection::merge(result, sliceA, sliceB, true, false);
  return {};
}

EvalResult Prim_MergeDict(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (!params.isArray() && params.length() != 2) {
    return EvalError("expected exactly two parameters");
  }

  if (!params.at(0).isObject()) {
    return EvalError("expected object, found: " + params.at(0).toJson());
  }
  if (!params.at(1).isObject()) {
    return EvalError("expected object, found: " + params.at(1).toJson());
  }

  MergeObjectSlice(result, params.at(0), params.at(1));
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

EvalResult Prim_PrintLn(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  std::cerr << paramsToString(params) << std::endl;
  result.add(VPackSlice::noneSlice());
  return {};
}

EvalResult Prim_Error(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  return EvalError(paramsToString(params));
}

EvalResult Prim_List(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  VPackArrayBuilder ab(&result);
  result.add(VPackArrayIterator(params));
  return {};
}

EvalResult checkArrayParams(VPackSlice const& arr, VPackSlice const& index) {
  if (!arr.isArray()) {
    return EvalError("expect first parameter to be an array");
  }

  if (!index.isNumber()) {
    return EvalError("expect second parameter to be a number");
  }

  if (index.getInt() < 0) {
    return EvalError("number cannot be less than zero");
  }

  if (index.getUInt() > (arr.length() - 1)) {
    return EvalError("array index is out of bounds");
  }

  return {};
}

EvalResult Prim_ArrayRef(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (!params.isArray() && params.length() != 2) {
    return EvalError("expected exactly two parameters");
  }

  auto&& arr = params.at(0);
  auto&& index = params.at(1);

  auto check = checkArrayParams(arr, index);
  if (check.fail()) {
    return check;
  }

  result.add(arr.at(index.getUInt()));

  return {};
}

EvalResult Prim_ArraySet(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (!params.isArray() && params.length() != 3) {
    return EvalError("expected exactly two parameters");
  }

  auto&& arr = params.at(0);
  auto&& index = params.at(1);
  auto&& value = params.at(2);

  auto check = checkArrayParams(arr, index);
  if (check.fail()) {
    return check;
  }

  uint64_t pos = 0;
  result.openArray();
  for (auto&& element : VPackArrayIterator(arr)) {
    if (pos == index.getUInt()) {
      result.add(value);
    } else {
      result.add(element);
    }

    pos++;
  }
  result.close();

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
  if (!params.isArray() && params.length() != 3) {
    return EvalError("expected exactly three parameters");
  }

  auto&& obj = params.at(0);
  auto&& key = params.at(1);
  auto&& val = params.at(2);

  if (!obj.isObject()) {
    return EvalError("expect first parameter to be an object");
  }

  if (!key.isString() && !key.isArray()) {
    return EvalError("expect second parameter to be an array or string");
  }

  if (key.isString()) {
    VPackBuilder tmp;
    {
      VPackObjectBuilder ob(&tmp);
      tmp.add(key.copyString(), val);
    }
    MergeObjectSlice(result, obj, tmp.slice());
  } else if (key.isArray()) {
    VPackBuilder tmp;
    std::vector<VPackStringRef> path;
    uint64_t length = key.length();
    uint64_t iterationStep = 0;

    tmp.openObject();
    for (auto&& pathStep : VPackArrayIterator(key)) {
      if (!pathStep.isString()) {
        return EvalError("expected string in key arrays");
      }
      if (iterationStep < (length - 1)) {
        tmp.add(pathStep.copyString(), VPackValue(VPackValueType::Object));
      } else {
        tmp.add(pathStep.copyString(), val);
      }
      iterationStep++;
    }

    for (uint64_t step = 0; step < (length - 1); step++) {
      tmp.close();
    }
    tmp.close();

    MergeObjectSlice(result, obj, tmp.slice());
  } else {
    return EvalError("key is neither array nor string");
  }

  return {};
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

EvalResult EvaluateApply(Machine& ctx, VPackSlice const functionSlice,
                         VPackArrayIterator paramIterator, VPackBuilder& result,
                         bool isEvaluateParameter);

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
    return EvalError("expecting to arguments, a function and a list");
  }

  auto functionSlice = paramsList.at(0);
  auto list = paramsList.at(1);

  if (list.isArray()) {
    VPackArrayBuilder ab(&result);
    for (VPackArrayIterator iter(list); iter.valid(); iter++) {
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
    for (VPackObjectIterator iter(list); iter.valid(); iter++) {
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
    return EvalError("expected list, found: " + list.toJson());
  }

  return {};
}

EvalResult Prim_Filter(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  return EvalError("not implemented");
}

EvalResult Prim_Foldl(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  return EvalError("not implemented");
}

EvalResult Prim_Foldl1(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  return EvalError("not implemented");
}

EvalResult Prim_EmptyArrayHuh(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  auto res = extract<VPackSlice>(paramsList);
  if (!res) {
    return std::move(res).asResult();
  }

  auto&& [array] = res.value();
  result.add(VPackValue(array.isEmptyArray()));
  return {};
}

EvalResult Prim_ArrayLength(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  auto res = extract<VPackSlice>(paramsList);
  if (!res) {
    return std::move(res).asResult();
  }

  auto&& [array] = res.value();
  if (!array.isArray()) {
    return EvalError("expected array, found " + array.toJson());
  }

  result.add(VPackValue(array.length()));
  return {};
}

template <bool ignoreMissing>
EvalResult Prim_DictExtract(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  if (paramsList.length() < 1) {
    return EvalError("expected at least on parameter");
  }
  VPackArrayIterator iter(paramsList);

  VPackSlice obj = *iter;
  if (!obj.isObject()) {
    return EvalError("expected first parameter to be a dict, found: " + obj.toJson());
  }
  iter++;

  {
    VPackObjectBuilder ob(&result);
    for (; iter.valid(); iter++) {
      VPackSlice key = *iter;
      if (!key.isString()) {
        return EvalError("expected string, found: " + key.toJson());
      }

      VPackSlice value = obj.get(key.stringRef());
      if (value.isNone()) {
        if constexpr (ignoreMissing) {
          continue;
        } else {
          return EvalError("key `" + key.copyString() + "` not found");
        }
      }

      result.add(key.stringRef(), value);
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

  // Misc
  ctx.setFunction("min", Prim_Min);
  ctx.setFunction("max", Prim_Max);
  ctx.setFunction("avg", Prim_Avg);

  // Debug operators
  ctx.setFunction("print", Prim_PrintLn);
  ctx.setFunction("error", Prim_Error);

  // Constructors
  ctx.setFunction("dict", Prim_Dict);
  ctx.setFunction("dict-merge", Prim_MergeDict);
  ctx.setFunction("dict-keys", Prim_DictKeys);
  ctx.setFunction("dict-directory", Prim_DictDirectory);

  ctx.setFunction("list", Prim_List);

  // Lambdas
  ctx.setFunction("lambda", Prim_Lambda);

  // Utilities
  ctx.setFunction("list-cat", Prim_ListCat);
  ctx.setFunction("string-cat", Prim_StringCat);
  ctx.setFunction("int-to-str", Prim_IntToStr);

  // Functional stuff
  ctx.setFunction("id", Prim_Identity);
  ctx.setFunction("apply", Prim_Apply);
  ctx.setFunction("map", Prim_Map);  // ["map", <func(index, value) -> value>, <list>] or ["map", <func(key, value) -> value>, <dict>]
  ctx.setFunction("filter", Prim_Filter);  // ["filter", <func(index, value) -> bool>, <list>] or ["map", <func(key, value) -> bool>, <dict>]
  ctx.setFunction("foldl", Prim_Foldl);
  ctx.setFunction("foldl1", Prim_Foldl1);

  // Access operators
  ctx.setFunction("attrib-ref", Prim_AttribRef);
  ctx.setFunction("attrib-get", Prim_AttribRef);
  ctx.setFunction("attrib-set", Prim_AttribSet);
  ctx.setFunction("array-ref", Prim_ArrayRef);
  ctx.setFunction("array-set", Prim_ArraySet);

  ctx.setFunction("array-empty?", Prim_EmptyArrayHuh);
  ctx.setFunction("array-length", Prim_ArrayLength);
  ctx.setFunction("dict-x-tract", Prim_DictExtract<false>);
  ctx.setFunction("dict-x-tract-x", Prim_DictExtract<true>);

  ctx.setFunction("var-ref", Prim_VarRef);

  // TODO: We can just register bind parameters as variables (or a variable)
  ctx.setFunction("bind-ref", Prim_VarRef);
}

}  // namespace arangodb::greenspun
