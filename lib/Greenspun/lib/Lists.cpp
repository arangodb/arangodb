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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "Lists.h"
#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

#include "Greenspun/Extractor.h"
#include "Greenspun/Interpreter.h"

using namespace arangodb::greenspun;


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

  if (index.getNumber<int>() < 0) {
    return EvalError("number cannot be less than zero");
  }

  if (index.getNumber<unsigned int>() + 1 > arr.length()) {
    return EvalError("array index is out of bounds");
  }

  return {};
}

EvalResult Prim_ListEmptyHuh(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  auto res = extract<VPackSlice>(paramsList);
  if (!res) {
    return std::move(res).asResult();
  }

  auto&& [array] = res.value();
  result.add(VPackValue(array.isEmptyArray()));
  return {};
}

EvalResult Prim_ListLength(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
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

EvalResult Prim_ListAppend(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  VPackArrayBuilder ab(&result);

  VPackArrayIterator iter(paramsList);
  if (iter.valid()) {
    VPackSlice list = *iter;
    iter.next();
    if (!list.isArray()) {
      return EvalError("expected array as first parameter, found: " + list.toJson());
    }

    result.add(VPackArrayIterator(list));
    for (; iter.valid(); iter.next()) {
      result.add(*iter);
    }
  }

  return {};
}

EvalResult Prim_ListJoin(Machine& ctx, VPackSlice const slice, VPackBuilder& result) {
  auto res = extract<VPackArrayIterator>(slice);
  if (res.fail()) {
    return res.error();
  }

  VPackArrayBuilder ab(&result);
  auto&& [iter] = res.value();
  for (auto&& list : iter) {
    if (!list.isArray()) {
      return EvalError("expected array, found: " + list.toJson());
    }
    result.add(VPackArrayIterator(list));
  }
  return {};
}

EvalResult Prim_ListRef(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (!params.isArray() || params.length() != 2) {
    return EvalError("expected exactly two parameters");
  }

  auto&& arr = params.at(0);
  auto&& index = params.at(1);

  auto check = checkArrayParams(arr, index);
  if (check.fail()) {
    return check;
  }

  result.add(arr.at(index.getNumber<unsigned int>()));

  return {};
}

EvalResult Prim_ListRepeat(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (!params.isArray() || params.length() != 2) {
    return EvalError("expected exactly two parameters");
  }

  auto&& value = params.at(0);
  auto&& num = params.at(1);

  if (!num.isNumber<size_t>()) {
    return EvalError("invalid repeat count");
  }

  VPackArrayBuilder ab(&result);
  for (size_t i = 0; i < num.getNumber<size_t>(); i++) {
    result.add(value);
  }

  return {};
}

EvalResult Prim_ListSet(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (!params.isArray() || params.length() != 3) {
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
    if (pos == index.getNumber<unsigned int>()) {
      result.add(value);
    } else {
      result.add(element);
    }

    pos++;
  }
  result.close();

  return {};
}

EvalResult Prim_Sort(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  auto res = extract<VPackSlice, VPackSlice>(paramsList);
  if (!res) {
    return std::move(res).asResult();
  }

  auto&& [func, list] = res.value();
  if (!list.isArray()) {
    return EvalError("expected list as second parameter, found: " + list.toJson());
  }

  VPackArrayIterator iter(list);
  std::vector<VPackSlice> v;
  v.reserve(list.length());
  v.assign(iter.begin(), iter.end());

  struct Compare {
    Machine& ctx;
    VPackSlice func;

    // return true if A is _less_ than B
    bool operator()(VPackSlice A, VPackSlice B) {
      VPackBuilder parameter;
      {
        VPackArrayBuilder pb(&parameter);
        parameter.add(A);
        parameter.add(B);
      }

      VPackBuilder tempBuffer;

      auto res = EvaluateApply(ctx, func, VPackArrayIterator(parameter.slice()),
                               tempBuffer, false);
      if (res.fail()) {
        throw res.error().wrapMessage("when mapping pair " + parameter.toJson());
      }

      return ValueConsideredTrue(tempBuffer.slice());
    }
  };

  try {
    std::sort(v.begin(), v.end(), Compare{ctx, func});
  } catch (EvalError& err) {
    return err.wrapMessage("in compare function");
  }

  VPackArrayBuilder ab(&result);
  for (auto&& slice : v) {
    result.add(slice);
  }
  return {};
}

EvalResult Prim_ListHuh(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  auto res = extract<VPackSlice>(paramsList);
  if (!res) {
    return std::move(res).asResult();
  }
  auto&& [value] = res.value();
  result.add(VPackValue(value.isArray()));
  return {};
}

void arangodb::greenspun::RegisterAllListFunctions(Machine& ctx) {
  // Lists
  ctx.setFunction("list", Prim_List);
  ctx.setFunction("list?", Prim_ListHuh);
  ctx.setFunction("list-cat", Prim_ListCat);
  ctx.setFunction("list-append", Prim_ListAppend);
  ctx.setFunction("list-ref", Prim_ListRef);
  ctx.setFunction("list-set", Prim_ListSet);
  ctx.setFunction("list-empty?", Prim_ListEmptyHuh);
  ctx.setFunction("list-length", Prim_ListLength);
  ctx.setFunction("list-join", Prim_ListJoin);
  ctx.setFunction("list-sort", Prim_Sort);
  ctx.setFunction("list-repeat", Prim_ListRepeat);
  ctx.setFunction("sort", Prim_Sort);

  // deprecated list functions
  ctx.setFunction("array-ref", Prim_ListRef);
  ctx.setFunction("array-set", Prim_ListSet);
  ctx.setFunction("array-empty?", Prim_ListEmptyHuh);
  ctx.setFunction("array-length", Prim_ListLength);
}
