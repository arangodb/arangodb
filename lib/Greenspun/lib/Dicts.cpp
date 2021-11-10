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

#include <list>

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>
#include "Dicts.h"

#include "Greenspun/Extractor.h"
#include "Greenspun/Interpreter.h"

using namespace arangodb::greenspun;

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
  ++iter;

  {
    VPackObjectBuilder ob(&result);
    for (; iter.valid(); ++iter) {
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

void createPaths(std::vector<std::vector<std::string>>& finalPaths,
                 VPackSlice object, std::vector<std::string>& currentPath) {
  if (!object.isObject()) {
    return;
  }
  for (auto iter : VPackObjectIterator(object)) {
    currentPath.emplace_back(iter.key.toString());
    finalPaths.emplace_back(currentPath);
    createPaths(finalPaths, iter.value, currentPath);
    currentPath.pop_back();
  }
}

void pathToBuilder(std::vector<std::vector<std::string>>& finalPaths, VPackBuilder& result) {
  result.openArray();
  for (auto const& path : finalPaths) {
    if (path.size() > 1) {
      result.openArray();
    }
    for (auto const& pathElement : path) {
      result.add(VPackValue(pathElement));
    }
    if (path.size() > 1) {
      result.close();
    }
  }
  result.close();
}

EvalResult Prim_Dict(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  VPackObjectBuilder ob(&result);
  for (auto&& pair : VPackArrayIterator(params)) {
    if (pair.isArray() && pair.length() == 2 && pair.at(0).isString()) {
      result.add(pair.at(0).stringRef(), pair.at(1));
    } else {
      return EvalError("expected pairs of string and slice");
    }
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

EvalResult Prim_DictDirectory(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (params.length() != 1) {
    return EvalError("expected exactly one parameter");
  }

  auto obj = params.at(0);
  if (!obj.isObject()) {
    return EvalError("expected object, found: " + obj.toJson());
  }

  std::vector<std::vector<std::string>> finalPaths;
  std::vector<std::string> currentPath;
  createPaths(finalPaths, obj, currentPath);
  pathToBuilder(finalPaths, result);

  return {};
}

namespace {
void MergeObjectSlice(VPackBuilder& result, VPackSlice const& sliceA,
                            VPackSlice const& sliceB) {
  VPackCollection::merge(result, sliceA, sliceB, true, false);
}
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

namespace {
EvalResultT<VPackSlice> ReadAttribute(VPackSlice slice, VPackSlice key) {
  if (!slice.isObject()) {
    return EvalError("expect first parameter to be an object");
  }

  if (key.isString()) {
    return slice.get(key.stringRef());
  } else if (key.isArray()) {
    struct Iter : VPackArrayIterator {
      using VPackArrayIterator::difference_type;
      using value_type = VPackStringRef;
      using VPackArrayIterator::iterator_category;
      using VPackArrayIterator::pointer;
      using VPackArrayIterator::reference;

      value_type operator*() const {
        return VPackArrayIterator::operator*().stringRef();
      }
      Iter begin() const { return Iter{VPackArrayIterator::begin()}; }
      Iter end() const { return Iter{VPackArrayIterator::end()}; }
    };

    Iter i{VPackArrayIterator(key)};
    return slice.get(i);
  } else {
    return EvalError("key is neither array nor string");
  }
}
}  // namespace

EvalResult Prim_AttribRef(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (!params.isArray() || params.length() != 2) {
    return EvalError("expected exactly two parameters");
  }

  auto const& [slice, key] = arangodb::basics::VelocyPackHelper::unpackTuple<VPackSlice, VPackSlice>(params);
  auto res = ReadAttribute(slice, key);
  if (res.fail()) {
    return res.error();
  }

  result.add(res.value());
  return {};
}

EvalResult Prim_AttribRefOr(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (!params.isArray() || params.length() != 3) {
    return EvalError("expected exactly three parameters");
  }

  auto const& [slice, key, defaultValue] =
      arangodb::basics::VelocyPackHelper::unpackTuple<VPackSlice, VPackSlice, VPackSlice>(params);
  if (!slice.isObject()) {
    return EvalError("expect second parameter to be an object");
  }

  auto res = ReadAttribute(slice, key);
  if (res.fail()) {
    return std::move(res).error();
  }

  VPackSlice resultValue = res.value();
  if (resultValue.isNone()) {
    resultValue = defaultValue;
  }

  result.add(resultValue);
  return {};
}

EvalResult Prim_AttribRefOrFail(Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (!params.isArray() || params.length() != 2) {
    return EvalError("expected exactly two parameters");
  }

  auto const& [slice, key] = arangodb::basics::VelocyPackHelper::unpackTuple<VPackSlice, VPackSlice>(params);
  if (!slice.isObject()) {
    return EvalError("expected first parameter to be an object");
  }

  auto res = ReadAttribute(slice, key);
  if (res.fail()) {
    return res.error();
  }

  VPackSlice resultValue = res.value();
  if (resultValue.isNone()) {
    return EvalError("key " + key.toJson() + " not present");
  }

  result.add(resultValue);
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
      tmp.add(key.stringRef(), val);
    }
    MergeObjectSlice(result, obj, tmp.slice());
  } else if (key.isArray()) {
    VPackBuilder tmp;
    uint64_t length = key.length();
    uint64_t iterationStep = 0;

    tmp.openObject();
    for (auto&& pathStep : VPackArrayIterator(key)) {
      if (!pathStep.isString()) {
        return EvalError("expected string in key arrays");
      }
      if (iterationStep < (length - 1)) {
        tmp.add(pathStep.stringRef(), VPackValue(VPackValueType::Object));
      } else {
        tmp.add(pathStep.stringRef(), val);
      }
      iterationStep++;
    }

    for (uint64_t step = 0; step < length; step++) {
      tmp.close();
    }

    MergeObjectSlice(result, obj, tmp.slice());
  } else {
    return EvalError("key is neither array nor string");
  }

  return {};
}

EvalResult Prim_DictHuh(Machine& ctx, VPackSlice const paramsList, VPackBuilder& result) {
  auto res = extract<VPackSlice>(paramsList);
  if (!res) {
    return std::move(res).asResult();
  }
  auto&& [value] = res.value();
  result.add(VPackValue(value.isObject()));
  return {};
}

void arangodb::greenspun::RegisterAllDictFunctions(Machine& ctx) {
  // Constructors
  ctx.setFunction("dict", Prim_Dict);
  ctx.setFunction("dict?", Prim_DictHuh);
  ctx.setFunction("dict-merge", Prim_MergeDict);
  ctx.setFunction("dict-keys", Prim_DictKeys);
  ctx.setFunction("dict-directory", Prim_DictDirectory);

  // Access operators
  ctx.setFunction("attrib-ref", Prim_AttribRef);
  ctx.setFunction("attrib-ref-or", Prim_AttribRefOr);
  ctx.setFunction("attrib-ref-or-fail", Prim_AttribRefOrFail);
  ctx.setFunction("attrib-get", Prim_AttribRef);
  ctx.setFunction("attrib-set", Prim_AttribSet);

  ctx.setFunction("dict-x-tract", Prim_DictExtract<false>);
  ctx.setFunction("dict-x-tract-x", Prim_DictExtract<true>);
}
