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

std::unordered_map<std::string, std::function<void(EvalContext& ctx, VPackSlice const slice, VPackBuilder& result)>> primitives;

// FIXME: This needs to go into support code somehwere.
namespace detail {
template <class>
inline constexpr bool always_false_v = false;
template <typename... Ts, std::size_t... Is>
auto unpackTuple(VPackArrayIterator& iter, std::index_sequence<Is...>) {
  std::tuple<Ts...> result;
  (
      [&result](VPackSlice slice) {
        TRI_ASSERT(!slice.isNone());
        auto& value = std::get<Is>(result);
        if constexpr (std::is_same_v<Ts, bool>) {
          TRI_ASSERT(slice.isBool());
          value = slice.getBool();
        } else if constexpr (std::is_integral_v<Ts>) {
          TRI_ASSERT(slice.template isNumber<Ts>());
          value = slice.template getNumericValue<Ts>();
        } else if constexpr (std::is_same_v<Ts, double>) {
          TRI_ASSERT(slice.isDouble());
          value = slice.getDouble();
        } else if constexpr (std::is_same_v<Ts, std::string>) {
          TRI_ASSERT(slice.isString());
          value = slice.copyString();
        } else if constexpr (std::is_same_v<Ts, std::string_view>) {
          TRI_ASSERT(slice.isString());
          value = slice.stringView();
        } else if constexpr (std::is_same_v<Ts, VPackStringRef>) {
          TRI_ASSERT(slice.isString());
          value = slice.stringRef();
        } else if constexpr (std::is_same_v<Ts, VPackSlice>) {
          value = slice;
        } else {
          static_assert(always_false_v<Ts>, "Unhandled value type requested");
        }
      }(*(iter++)),
      ...);
  return result;
}
}  // namespace detail
/// @brief unpacks an array as tuple. Use like this: auto&& [a, b, c] = unpack<size_t, std::string, double>(slice);
template <typename... Ts>
static std::tuple<Ts...> unpackTuple(VPackSlice slice) {
  VPackArrayIterator iter(slice);
  return detail::unpackTuple<Ts...>(iter, std::index_sequence_for<Ts...>{});
}
template <typename... Ts>
static std::tuple<Ts...> unpackTuple(VPackArrayIterator& iter) {
  return detail::unpackTuple<Ts...>(iter, std::index_sequence_for<Ts...>{});
}

void Prim_Banana(EvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto tmp = int64_t{0};
  for (auto p : VPackArrayIterator(params)) {
    tmp += p.getNumericValue<int64_t>();
  }
  result.add(VPackValue(tmp));
}

void Prim_Sub(EvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto tmp = int64_t{0};
  auto iter = VPackArrayIterator(params);
  if (iter.valid()) {
    tmp = (*iter).getNumericValue<int64_t>();
    for (; iter.valid(); iter++) {
      tmp -= (*iter).getNumericValue<int64_t>();
    }
  }
  result.add(VPackValue(tmp));
}

void Prim_Mul(EvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto tmp = int64_t{1};
  for (auto p : VPackArrayIterator(params)) {
    tmp *= p.getNumericValue<int64_t>();
  }
  result.add(VPackValue(tmp));
}

void Prim_Div(EvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto tmp = int64_t{1};
  auto iter = VPackArrayIterator(params);
  if (iter.valid()) {
    tmp = (*iter).getNumericValue<int64_t>();
    for (; iter.valid(); iter++) {
      tmp /= (*iter).getNumericValue<int64_t>();
    }
  }
  result.add(VPackValue(tmp));
}

void Prim_List(EvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  VPackArrayBuilder array(&result);
  result.add(VPackArrayIterator(params));
}

void Prim_EqHuh(EvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto iter = VPackArrayIterator(params);
  if (iter.valid()) {
    auto proto = *iter;
    for (; iter.valid(); iter++) {
      if (!arangodb::basics::VelocyPackHelper::equal(proto, *iter, true)) {
        result.add(VPackValue(false));
        return;
      }
    }
  }
  result.add(VPackValue(true));
}

void Prim_If(EvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  for (auto&& pair : VPackArrayIterator(params)) {
    VPackBuilder condResult;
    auto&& [cond, slice] = unpackTuple<VPackSlice, VPackSlice>(pair);
    Evaluate(ctx, cond, condResult);
    if (!condResult.slice().isFalse()) {
      Evaluate(ctx, slice, result);
      return;
    }
  }

  result.add(VPackSlice::noneSlice());
}

void Prim_VarRef(EvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto&& [name] = unpackTuple<std::string>(params);

  auto value = ctx.variables.find(name);
  if (value != ctx.variables.end()) {
    result.add(value->second);
  } else {
    std::cerr << "Variable " << name << " not found.";
    std::abort();
  }
}

void Prim_Attrib(EvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto&& [key, slice] = unpackTuple<VPackSlice, VPackSlice>(params);
  if (key.isString()) {
    result.add(slice.get(key.stringRef()));
  } else {
    std::vector<VPackStringRef> path;
    for (auto&& pathStep : VPackArrayIterator(key)) {
      path.emplace_back(pathStep.stringRef());
    }
    result.add(slice.get(path));
  }
}

void Prim_This(EvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  result.add(VPackValue(ctx.getThisId()));
}

void Prim_Doc(EvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto&& [docId] = unpackTuple<std::string_view>(params);
  result.add(ctx.getDocumentById(docId));
}

void Prim_AccumRef(EvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto&& [accumId] = unpackTuple<std::string_view>(params);
  result.add(ctx.getAccumulatorValue(accumId));
}

void Prim_Update(EvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto&& [accumId, vertexId, value] =
      unpackTuple<std::string_view, std::string_view, VPackSlice>(params);
  ctx.updateAccumulator(accumId, vertexId, value);
}

void Prim_Set(EvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto&& [accumId, vertexId, value] =
      unpackTuple<std::string_view, std::string_view, VPackSlice>(params);
  ctx.setAccumulator(accumId, vertexId, value);
}

void Prim_For(EvalContext& ctx, VPackSlice const params, VPackBuilder& result) {
  auto&& [dir, vars, body] = unpackTuple<std::string_view, VPackSlice, VPackSlice>(params);
  auto&& [edgeVar, otherVertexVar] = unpackTuple<std::string, std::string>(vars);

  TRI_ASSERT(ctx.variables.find(edgeVar) == std::end(ctx.variables));
  TRI_ASSERT(ctx.variables.find(otherVertexVar) == std::end(ctx.variables));

  // TODO translate direction and pass to enumerateEdges
  ctx.enumerateEdges([&, edgeVar = edgeVar,
                      otherVertexVar = otherVertexVar, body = body](VPackSlice edge, VPackSlice vertex) {
    ctx.variables[edgeVar] = edge;
    ctx.variables[otherVertexVar] = vertex;

    VPackBuilder localResult;
    Evaluate(ctx, body, localResult);
  });

  ctx.variables.erase(edgeVar);
  ctx.variables.erase(otherVertexVar);
}

void RegisterPrimitives() {
  primitives["banana"] = Prim_Banana;
  primitives["+"] = Prim_Banana;
  primitives["-"] = Prim_Sub;
  primitives["*"] = Prim_Mul;
  primitives["/"] = Prim_Div;
  primitives["list"] = Prim_List;
  primitives["eq?"] = Prim_EqHuh;
  primitives["if"] = Prim_If;
  primitives["varref"] = Prim_VarRef;
  primitives["attrib"] = Prim_Attrib;
  primitives["this"] = Prim_This;
  primitives["doc"] = Prim_Doc;
  primitives["accumref"] = Prim_AccumRef;
  primitives["update"] = Prim_Update;
  primitives["set"] = Prim_Set;
  primitives["for"] = Prim_For;
}
