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

#ifndef ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_EVALUATOR_H
#define ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_EVALUATOR_H 1

#include <functional>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <stack>

#ifndef TRI_ASSERT
#define TRI_ASSERT(x) if (!(x)) { std::abort(); }
#endif

struct EvalContext {
  virtual ~EvalContext() = default;
  // Variables go here.
  void pushStack() { variables.emplace(); }
  void popStack() {
    TRI_ASSERT(variables.size() > 1);
    variables.pop();
  }

  void setVariable(std::string name, VPackSlice value) {
    TRI_ASSERT(!variables.empty());
    variables.top().emplace(std::move(name), value);
  }
  void getVariable(std::string const& name, VPackBuilder& result) {
    TRI_ASSERT(!variables.empty());
    if (auto iter = variables.top().find(name); iter != std::end(variables.top())) {
      result.add(iter->second);
    }
    result.add(VPackSlice::noneSlice());
  }

  size_t depth{0};

  virtual std::string const& getThisId() const = 0;

  virtual VPackSlice getDocumentById(std::string_view id) const = 0;
  virtual VPackSlice getAccumulatorValue(std::string_view id) const = 0;
  virtual void updateAccumulator(std::string_view accumId, std::string_view edgeId, VPackSlice value) = 0;
  virtual void setAccumulator(std::string_view accumId, VPackSlice value) = 0;
  virtual void enumerateEdges(std::function<void(VPackSlice edge, VPackSlice vertex)> cb) const = 0;

 private:
  std::stack<std::unordered_map<std::string, VPackSlice>> variables;
};

//
// ["varRef", "name"]
//
// assignment
//


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


void Evaluate(EvalContext& ctx, VPackSlice const slice, VPackBuilder& result);
void InitInterpreter();


#endif
