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
#include <map>
#include <variant>

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include "template-stuff.h"

#include "EvalResult.h"

namespace arangodb {
namespace greenspun {

struct StackFrame {
  std::unordered_map<std::string, VPackSlice> bindings;
  bool noParentScope = false;
};

struct Machine {
  Machine() noexcept;
  virtual ~Machine() = default;
  // Variables go here.
  void pushStack(bool noParentScope = false);
  void popStack();
  EvalResult setVariable(std::string const& name, VPackSlice value);
  EvalResult getVariable(std::string const& name, VPackBuilder& result);

  using function_type =
      std::function<EvalResult(Machine& ctx, VPackSlice const slice, VPackBuilder& result)>;

  EvalResult setFunction(std::string_view name, function_type&& f);
  EvalResult unsetFunction(std::string_view name);

  EvalResult applyFunction(std::string name, VPackSlice const slice, VPackBuilder& result);

  template<typename T>
  using member_function_type =
    std::function<EvalResult(T*, Machine& ctx, VPackSlice const slice, VPackBuilder& result)>;

  template<typename T, typename F>
  EvalResult setFunctionMember(std::string_view name, F&& f, T* ptr) {
    static_assert(!(std::is_move_constructible_v<T> || std::is_move_assignable_v<T> ||
                    std::is_copy_constructible_v<T> || std::is_copy_assignable_v<T>),
                  "please make sure that `this` is a stable pointer.");
    return setFunction(name, bind_member(f, ptr));
  }

  using print_callback_type = std::function<void(std::string)>;
  template<typename F>
  void setPrintCallback(F&& f) {
    printCallback = std::forward<F>(f);
  }

  void print(std::string const& msg) const;

 private:
  std::vector<StackFrame> variables;
  std::unordered_map<std::string, function_type> functions;

  std::function<void(std::string)> printCallback;
};

template <bool isNewScope, bool noParentScope = false>
struct StackFrameGuard {
  StackFrameGuard(Machine& ctx) : _ctx(ctx) {
    if constexpr (isNewScope) {
      ctx.pushStack(noParentScope);
    }
  }

  ~StackFrameGuard() {
    if constexpr (isNewScope) {
      _ctx.popStack();
    }
  }

  Machine& _ctx;
};

bool ValueConsideredTrue(VPackSlice const value);
bool ValueConsideredFalse(VPackSlice const value);

EvalResult Evaluate(Machine& ctx, VPackSlice slice, VPackBuilder& result);
void InitMachine(Machine& ctx);

std::string paramsToString(const VPackArrayIterator iter);
std::string paramsToString(VPackSlice const params);

}  // namespace greenspun
}  // namespace arangodb

#endif
