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

#ifndef ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_EVALUATOR_H
#define ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_EVALUATOR_H 1

#include <functional>
#include <map>
#include <variant>

#include <immer/map.hpp>

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "EvalResult.h"

namespace arangodb::greenspun {

using VariableBindings = immer::map<std::string, VPackSlice>;

struct StackFrame {
  VariableBindings bindings;

  StackFrame() = default;
  explicit StackFrame(VariableBindings bindings) : bindings(std::move(bindings)) {}

  EvalResult getVariable(std::string const& name, VPackBuilder& result) const;
  EvalResult setVariable(std::string const& name, VPackSlice value);
};

struct Machine {
  Machine();
  virtual ~Machine() = default;
  void pushStack(bool noParentScope = false);
  void emplaceStack(StackFrame sf);
  void popStack();
  EvalResult setVariable(std::string const& name, VPackSlice value);
  EvalResult getVariable(std::string const& name, VPackBuilder& result);

  using function_type =
      std::function<EvalResult(Machine& ctx, VPackSlice slice, VPackBuilder& result)>;

  EvalResult setFunction(std::string_view name, function_type&& f);

  EvalResult applyFunction(std::string name, VPackSlice slice, VPackBuilder& result);

  template <typename T>
  using member_function_type =
      std::function<EvalResult(T*, Machine& ctx, VPackSlice slice, VPackBuilder& result)>;

  template <typename T, typename F>
  EvalResult setFunctionMember(std::string_view name, F&& f, T* ptr) {
    static_assert(!(std::is_move_constructible_v<T> || std::is_move_assignable_v<T> ||
                    std::is_copy_constructible_v<T> || std::is_copy_assignable_v<T>),
                  "please make sure that `this` is a stable pointer.");
    return setFunction(name, [f, ptr](Machine& ctx, VPackSlice slice, VPackBuilder& result) -> EvalResult {
      return (ptr->*f)(ctx, slice, result);
    });
  }

  using print_callback_type = std::function<void(std::string)>;
  template <typename F>
  void setPrintCallback(F&& f) {
    printCallback = std::forward<F>(f);
  }

  EvalResult print(std::string const& msg) const;
  VariableBindings getAllVariables();

 private:
  std::vector<StackFrame> frames;
  std::unordered_map<std::string, function_type> functions;

  print_callback_type printCallback;
};

enum class StackFrameGuardMode { KEEP_SCOPE, NEW_SCOPE, NEW_SCOPE_HIDE_PARENT };

template <StackFrameGuardMode mode>
struct StackFrameGuard {
  template <StackFrameGuardMode U>
  static constexpr bool isNewScope =
      U == StackFrameGuardMode::NEW_SCOPE || U == StackFrameGuardMode::NEW_SCOPE_HIDE_PARENT;
  template <StackFrameGuardMode U>
  static constexpr bool noParentScope = U == StackFrameGuardMode::NEW_SCOPE_HIDE_PARENT;

  template <StackFrameGuardMode U = mode, std::enable_if_t<isNewScope<U>, int> = 0>
  StackFrameGuard(Machine& ctx, StackFrame sf) : _ctx(ctx) {
    ctx.emplaceStack(std::move(sf));
  }

  explicit StackFrameGuard(Machine& ctx) : _ctx(ctx) {
    if constexpr (isNewScope<mode>) {
      ctx.pushStack(noParentScope<mode>);
    }
  }

  ~StackFrameGuard() {
    if constexpr (isNewScope<mode>) {
      _ctx.popStack();
    }
  }

  Machine& _ctx;
};

bool ValueConsideredTrue(VPackSlice value);
bool ValueConsideredFalse(VPackSlice value);

EvalResult Evaluate(Machine& ctx, VPackSlice slice, VPackBuilder& result);
void InitMachine(Machine& ctx);
EvalResult EvaluateApply(Machine& ctx, VPackSlice functionSlice, VPackArrayIterator paramIterator,
                         VPackBuilder& result, bool isEvaluateParameter);

std::string paramsToString(VPackArrayIterator iter);
std::string paramsToString(VPackSlice params);

}  // namespace arangodb::greenspun

#endif
