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

// #include "Basics/debugging.h"
#include "Interpreter.h"
#include "PrimEvalContext.h"
#include "Primitives.h"

#include <iostream>
#include <sstream>

#include <Basics/VelocyPackHelper.h>
#include <velocypack/Iterator.h>

using namespace arangodb::velocypack;

namespace arangodb::greenspun {

void InitInterpreter() { RegisterPrimitives(); }

EvalResult Apply(EvalContext& ctx, std::string const& function,
                 VPackSlice const params, VPackBuilder& result) {
  TRI_ASSERT(params.isArray())
  auto f = primitives.find(function);
  if (f != primitives.end()) {
    return f->second(reinterpret_cast<PrimEvalContext&>(ctx), params, result)
        .wrapError([&](EvalError& err) { err.wrapCall(function, params); });
  } else {
    return EvalError("primitive not found `" + function + "`");
  }
}

EvalResult SpecialIf(EvalContext& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  for (auto iter = paramIterator; iter.valid(); iter++) {
    auto pair = *iter;
    if (!pair.isArray() || pair.length() != 2) {
      return EvalError("in case " + std::to_string(iter.index()) +
                       ", expected pair, found: " + pair.toJson());
    }

    auto&& [cond, body] = unpackTuple<VPackSlice, VPackSlice>(*iter);
    VPackBuilder condResult;
    {
      StackFrameGuard<false> guard(ctx);
      auto res = Evaluate(ctx, cond, condResult);
      if (!res) {
        return res.wrapError([&](EvalError& err) {
          err.wrapMessage("in condition " + std::to_string(iter.index()));
        });
      }
    }
    if (!condResult.slice().isFalse()) {
      StackFrameGuard<false> guard(ctx);
      return Evaluate(ctx, body, result).wrapError([&](EvalError& err) {
        err.wrapMessage("in case " + std::to_string(iter.index()));
      });
    }
  }

  result.add(VPackSlice::noneSlice());
  return {};
}

EvalResult SpecialQuote(EvalContext& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  VPackArrayBuilder array(&result);
  result.add(paramIterator);
  return {};
}

EvalResult SpecialCons(EvalContext& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  auto&& [head, list] = unpackTuple<VPackSlice, VPackSlice>(paramIterator);
  if (paramIterator.valid()) {
    return EvalError("Excess elements in cons call");
  }

  if (!list.isArray()) {
    return EvalError("Expected array as second parameter");
  }

  VPackArrayBuilder array(&result);
  result.add(head);
  result.add(ArrayIterator(list));
  return {};
}

EvalResult SpecialAnd(EvalContext& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  for (; paramIterator.valid(); paramIterator++) {
    VPackBuilder value;
    if (auto res = Evaluate(ctx, *paramIterator, value); res.fail()) {
      return res.wrapError([&](EvalError& err) {
        err.wrapMessage("in case " + std::to_string(paramIterator.index()));
      });
    }

    if (ValueConsideredFalse(value.slice())) {
      result.add(VPackSlice::falseSlice());
      return {};
    }
  }

  result.add(VPackValue(true));
  return {};
}

EvalResult SpecialOr(EvalContext& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  for (; paramIterator.valid(); paramIterator++) {
    VPackBuilder value;
    if (auto res = Evaluate(ctx, *paramIterator, value); res.fail()) {
      return res.wrapError([&](EvalError& err) {
        err.wrapMessage("in case " + std::to_string(paramIterator.index()));
      });
    }

    if (ValueConsideredTrue(value.slice())) {
      result.add(VPackSlice::trueSlice());
      return {};
    }
  }

  result.add(VPackValue(false));
  return {};
}

EvalResult SpecialSeq(EvalContext& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  VPackBuilder store;
  for (; paramIterator.valid(); paramIterator++) {
    auto& usedBuilder = std::invoke([&]() -> VPackBuilder& {
      if (paramIterator.isLast()) {
        return result;
      }

      store.clear();
      return store;
    });
    if (auto res = Evaluate(ctx, *paramIterator, usedBuilder); res.fail()) {
      return res.wrapError([&](EvalError& err) {
        err.wrapMessage("at position " + std::to_string(paramIterator.index()));
      });
    }
  }

  return {};
}

EvalResult SpecialMatch(EvalContext& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  if (!paramIterator.valid()) {
    return EvalError("expected at least one argument");
  }

  VPackBuilder proto;
  if (auto res = Evaluate(ctx, *paramIterator, proto); !res) {
    return res;
  }
  if (!proto.slice().isNumber()) {
    return EvalError("expected numeric expression in pattern");
  }
  auto pattern = proto.slice().getNumber<double>();
  paramIterator++;
  for (; paramIterator.valid(); paramIterator++) {
    auto pair = *paramIterator;
    if (!pair.isArray() || pair.length() != 2) {
      return EvalError("in case " + std::to_string(paramIterator.index()) +
                       ", expected pair, found: " + pair.toJson());
    }
    auto&& [cmp, body] = unpackTuple<VPackSlice, VPackSlice>(pair);
    VPackBuilder cmpValue;
    if (auto res = Evaluate(ctx, cmp, cmpValue); !res) {
      return res.wrapError([&](EvalError& err) {
        err.wrapMessage("in condition " + std::to_string(paramIterator.index() - 1));
      });
    }

    if (!cmpValue.slice().isNumber()) {
      return EvalError("in condition " + std::to_string(paramIterator.index() - 1) +
                       " expected numeric value, found: " + cmpValue.toJson());
    }

    if (pattern == cmpValue.slice().getNumber<double>()) {
      return Evaluate(ctx, body, result).wrapError([&](EvalError& err) {
        err.wrapMessage("in case " + std::to_string(paramIterator.index() - 1));
      });
    }
  }

  result.add(VPackSlice::noneSlice());
  return {};
}

EvalResult SpecialForEach(EvalContext& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  // ["for-each", ["a", ["A", "B", "C"]], ["d", ["1", "2", "3"]], ["print", ["var-ref", "a"], ["var-ref", "d"]]]

  if (!paramIterator.valid()) {
    return EvalError("Expected at least one argument");
  }

  struct IteratorTriple {
    std::string_view varName;
    VPackBuilder value;
    ArrayIterator iterator;

    IteratorTriple(std::string_view name, VPackBuilder v)
        : varName(name), value(std::move(v)), iterator(value.slice()) {}
  };

  std::vector<IteratorTriple> iterators;

  auto const readIteratorPair = [&](VPackSlice pair) -> EvalResult {
    if (pair.length() != 2) {
      return EvalError("Expected pair, found: " + pair.toJson());
    }
    auto&& [var, array] = unpackTuple<VPackSlice, VPackSlice>(pair);
    if (!var.isString()) {
      return EvalError("Expected string as first entry, found: " + var.toJson());
    }
    if (!array.isArray()) {
      return EvalError("Expected array os second entry, found: " + array.toJson());
    }
    VPackBuilder listResult;
    if (auto res = Evaluate(ctx, array, listResult); res.fail()) {
      return res;
    }

    iterators.emplace_back(var.stringView(), std::move(listResult));
    return {};
  };

  while (!paramIterator.isLast()) {
    VPackSlice pair = *paramIterator;
    paramIterator++;
    if (auto res = readIteratorPair(pair); res.fail()) {
      return res.wrapError([&](EvalError& err) {
        err.wrapMessage("at position " + std::to_string(paramIterator.index() - 1));
      });
    }
  }

  VPackSlice body = paramIterator.value();

  auto const runIterators = [&](EvalContext& ctx, std::size_t index, auto next) -> EvalResult {
    if (index == iterators.size()) {
      VPackBuilder sink;
      return Evaluate(ctx, body, sink);
    } else {
      auto pair = iterators.at(index);
      for (auto&& x : pair.iterator) {
        StackFrameGuard<true> guard(ctx);
        ctx.setVariable(std::string{pair.varName}, x);
        if (auto res = next(ctx, index + 1, next); res.fail()) {
          return res;
        }
      }
      return {};
    }
  };
  result.add(VPackSlice::noneSlice());
  return runIterators(ctx, 0, runIterators);
}

EvalResult Call(EvalContext& ctx, VPackSlice const functionSlice,
                ArrayIterator paramIterator, VPackBuilder& result) {
  VPackBuilder paramBuilder;
  {
    VPackArrayBuilder builder(&paramBuilder);
    for (; paramIterator.valid(); ++paramIterator) {
      StackFrameGuard<false> guard(ctx);
      if (auto res = Evaluate(ctx, *paramIterator, paramBuilder); !res) {
        return res.wrapError([&](EvalError& err) {
          err.wrapParameter(functionSlice.copyString(), paramIterator.index());
        });
      }
    }
  }
  return Apply(ctx, functionSlice.copyString(), paramBuilder.slice(), result);
}

EvalResult LambdaCall(EvalContext& ctx, VPackSlice paramNames, VPackSlice captures,
                      ArrayIterator paramIterator, VPackSlice body, VPackBuilder& result) {
  VPackBuilder paramBuilder;
  {
    VPackArrayBuilder builder(&paramBuilder);
    for (; paramIterator.valid(); ++paramIterator) {
      StackFrameGuard<false> guard(ctx);
      if (auto res = Evaluate(ctx, *paramIterator, paramBuilder); !res) {
        return res.wrapError([&](EvalError& err) {
          err.wrapParameter("<lambda>" + captures.toJson() + paramNames.toJson(),
                            paramIterator.index());
        });
      }
    }
  }

  StackFrameGuard<true, true> captureFrameGuard(ctx);
  for (auto&& pair : ObjectIterator(captures)) {
    ctx.setVariable(pair.key.copyString(), pair.value);
  }

  StackFrameGuard<true, false> parameterFrameGuard(ctx);
  VPackArrayIterator builderIter(paramBuilder.slice());
  for (auto&& paramName : ArrayIterator(paramNames)) {
    if (!paramName.isString()) {
      return EvalError(
          "bad lambda format: expected parameter name (string), found: " +
          paramName.toJson());
    }

    if (!builderIter.valid()) {
      return EvalError("lambda expects " + std::to_string(paramNames.length()) +
                       " parameters " + paramNames.toJson() + ", found " +
                       std::to_string(builderIter.index() - 1));
    }

    ctx.setVariable(paramName.copyString(), *builderIter);
    builderIter++;
  }
  return Evaluate(ctx, body, result).wrapError([&](EvalError& err) {
    err.wrapCall("<lambda>" + captures.toJson() + paramNames.toJson(),
                 paramBuilder.slice());
  });
}

EvalResult SpecialLet(EvalContext& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  std::vector<VPackBuilder> store;

  if (!paramIterator.valid()) {
    return EvalError("Expected at least one argument");
  }

  auto bindings = *paramIterator++;
  if (!bindings.isArray()) {
    return EvalError("Expected list of bindings, found: " + bindings.toJson());
  }

  StackFrameGuard<true> guard(ctx);

  for (VPackArrayIterator iter(bindings); iter.valid(); iter++) {
    auto&& pair = *iter;
    if (pair.isArray() && pair.length() == 2) {
      auto nameSlice = pair.at(0);
      auto valueSlice = pair.at(1);
      if (!nameSlice.isString()) {
        return EvalError("expected string as bind name at position " +
                         std::to_string(iter.index()) + ", found: " + nameSlice.toJson());
      }

      auto& builder = store.emplace_back();
      if (auto res = Evaluate(ctx, valueSlice, builder); res.fail()) {
        return res.wrapError([&](EvalError& err) {
          err.wrapMessage("when evaluating value for binding `" + nameSlice.copyString() +
                          "` at position " + std::to_string(iter.index()));
        });
      }

      if (auto res = ctx.setVariable(nameSlice.copyString(), builder.slice()); res.fail()) {
        return res;
      }
    } else {
      return EvalError("expected pair at position " + std::to_string(iter.index()) +
                       " at list of bindings, found: " + pair.toJson());
    }
  }

  // Now do a seq evaluation of the remaining parameter
  return SpecialSeq(ctx, paramIterator, result).wrapError([](EvalError& err) {
    err.wrapMessage("in evaluation of let-statement");
  });
}

EvalResult Evaluate(EvalContext& ctx, VPackSlice const slice, VPackBuilder& result) {
  if (slice.isArray()) {
    if (slice.isEmptyArray()) {
      return EvalError("empty application");
    }

    auto paramIterator = ArrayIterator(slice);
    VPackBuilder functionBuilder;
    {
      StackFrameGuard<false> guard(ctx);
      auto err = Evaluate(ctx, *paramIterator, functionBuilder);
      if (err.fail()) {
        return err.wrapError(
            [&](EvalError& err) { err.wrapMessage("in function expression"); });
      }
    }
    ++paramIterator;
    VPackSlice functionSlice = functionBuilder.slice();
    if (functionSlice.isString()) {
      // check for special forms
      if (functionSlice.isEqualString("if")) {
        return SpecialIf(ctx, paramIterator, result);
      } else if (functionSlice.isEqualString("quote")) {
        return SpecialQuote(ctx, paramIterator, result);
      } else if (functionSlice.isEqualString("cons")) {
        return SpecialCons(ctx, paramIterator, result);
      } else if (functionSlice.isEqualString("and")) {
        return SpecialAnd(ctx, paramIterator, result);
      } else if (functionSlice.isEqualString("or")) {
        return SpecialOr(ctx, paramIterator, result);
      } else if (functionSlice.isEqualString("seq")) {
        return SpecialSeq(ctx, paramIterator, result);
      } else if (functionSlice.isEqualString("match")) {
        return SpecialMatch(ctx, paramIterator, result);
      } else if (functionSlice.isEqualString("for-each")) {
        return SpecialForEach(ctx, paramIterator, result);
      } else if (functionSlice.isEqualString("let")) {
        return SpecialLet(ctx, paramIterator, result);
      } else {
        return Call(ctx, functionSlice, paramIterator, result);
      }
    } else if (functionSlice.isObject()) {
      auto body = functionSlice.get("_call");
      if (!body.isNone()) {
        auto params = functionSlice.get("_params");
        if (!params.isArray()) {
          return EvalError("lambda params have to be an array, found: " + params.toJson());
        }
        auto captures = functionSlice.get("_captures");
        if (!captures.isObject()) {
          return EvalError("lambda captures have to be an object, found: " +
                           params.toJson());
        }
        return LambdaCall(ctx, params, captures, paramIterator, body, result);
      }
    }
    return EvalError("function is not a string, found " + functionSlice.toJson());
  }

  result.add(slice);
  return {};
}

EvalContext::EvalContext() noexcept {
  // Top level variables
  pushStack();
}

EvalResult EvalContext::getVariable(const std::string& name, VPackBuilder& result) {
  for (auto scope = variables.rbegin(); scope != variables.rend(); ++scope) {
    auto iter = scope->bindings.find(name);
    if (iter != std::end(scope->bindings)) {
      result.add(iter->second);
      return {};
    }
    if (scope->noParentScope) {
      break;
    }
  }
  // TODO: variable not found error.
  result.add(VPackSlice::noneSlice());
  return EvalError("variable `" + name + "` not found");
}

EvalResult EvalContext::setVariable(std::string const& name, VPackSlice value) {
  TRI_ASSERT(!variables.empty());
  variables.back().bindings.operator[](name) = value;  // insert or create
  return {};
}

void EvalContext::pushStack(bool noParentScope) {
  variables.emplace_back().noParentScope = noParentScope;
}

void EvalContext::popStack() {
  // Top level variables must not be popped
  TRI_ASSERT(variables.size() > 1);

  variables.pop_back();
}

std::string EvalError::toString() const {
  std::stringstream ss;

  struct PrintErrorVisistor {
    std::stringstream& ss;

    void operator()(EvalError::CallFrame const& f) {
      ss << "in function `" << f.function << "` called with (";
      for (auto&& s : f.parameter) {
        ss << " `" << s << "`";
      }
      ss << " )" << std::endl;
    }

    void operator()(EvalError::WrapFrame const& f) {
      ss << f.message << std::endl;
    }
    void operator()(EvalError::ParamFrame const& f) {
      ss << "in function `" << f.function << "` at parameter " << f.offset << std::endl;
    }
  };

  ss << message << std::endl;
  for (auto&& f : frames) {
    std::visit(PrintErrorVisistor{ss}, f);
  }

  return ss.str();
}

bool ValueConsideredFalse(VPackSlice const value) {
  return value.isFalse() || value.isNone();
}

bool ValueConsideredTrue(VPackSlice const value) {
  return !ValueConsideredFalse(value);
}

}  // namespace arangodb::greenspun
