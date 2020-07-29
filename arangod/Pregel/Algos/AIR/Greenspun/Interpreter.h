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


struct EvalError {
  struct CallFrame {
    std::string function;
    std::vector<std::string> parameter;
  };

  struct ParamFrame {
    std::string function;
    std::size_t offset;
  };

  struct WrapFrame {
    std::string message;
  };

  using Frame = std::variant<ParamFrame, CallFrame, WrapFrame>;

  explicit EvalError(std::string message) : message(std::move(message)) {}
  EvalError(EvalError const&) = default;
  EvalError(EvalError&&) = default;
  EvalError& operator=(EvalError const&) = default;
  EvalError& operator=(EvalError&&) = default;

  EvalError& wrapParameter(std::string function, std::size_t off) {
    frames.emplace_back(ParamFrame{std::move(function), off});
    return *this;
  }

  EvalError& wrapMessage(std::string wrap) {
    frames.emplace_back(WrapFrame{std::move(wrap)});
    return *this;
  }

  EvalError& wrapCall(std::string function, VPackSlice parameter) {
    std::vector<std::string> parameterVec;
    for (auto&& p : VPackArrayIterator(parameter)) {
      parameterVec.push_back(p.toJson());
    }
    frames.emplace_back(CallFrame{std::move(function), std::move(parameterVec)});
    return *this;
  }

  std::string toString() const;

  std::vector<Frame> frames;
  std::string message;
};

struct EvalResult {

  bool ok() const { return !_error.has_value(); }
  bool fail() const { return _error.has_value(); }
  operator bool() const { return ok(); }

  EvalResult() = default;
  EvalResult(EvalError error) : _error(error) {}
  EvalResult(EvalResult const&) = default;
  EvalResult(EvalResult &&) = default;
  EvalResult& operator=(EvalResult const&) = default;
  EvalResult& operator=(EvalResult &&) = default;

  template<typename F>
  EvalResult& wrapError(F && f) {
    if (_error) {
      std::forward<F>(f)(_error.value());
    }
    return *this;
  }

  EvalError& error() { return _error.value(); }
  EvalError const& error() const { return _error.value(); }

  std::optional<EvalError> _error;
};

struct EvalContext {
  EvalContext() noexcept;
  virtual ~EvalContext() = default;
  // Variables go here.
  void pushStack();
  void popStack();
  EvalResult setVariable(std::string name, VPackSlice value);
  EvalResult getVariable(std::string const& name, VPackBuilder& result);

  size_t depth{0};
 private:
  std::vector<std::map<std::string, VPackSlice>> variables;
};

template <bool isNewScope>
struct StackFrameGuard {
  StackFrameGuard(EvalContext& ctx) : _ctx(ctx) {
    ctx.depth += 1;
    if constexpr (isNewScope) {
      ctx.pushStack();
    }
  }

  ~StackFrameGuard() {
    _ctx.depth -= 1;
    if constexpr (isNewScope) {
      _ctx.popStack();
    }
  }

  EvalContext& _ctx;
};

//
// ["varRef", "name"]
//
// assignment
//

EvalResult Evaluate(EvalContext& ctx, VPackSlice slice, VPackBuilder& result);
void InitInterpreter();

bool ValueConsideredTrue(VPackSlice const value);
bool ValueConsideredFalse(VPackSlice const value);

#endif
