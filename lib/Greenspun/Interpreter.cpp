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

#include "Interpreter.h"
#include "Greenspun/lib/DateTime.h"
#include "Greenspun/lib/Dicts.h"
#include "Greenspun/lib/Lists.h"
#include "Greenspun/lib/Math.h"
#include "Greenspun/lib/Strings.h"
#include "Primitives.h"

#include <iostream>
#include <numeric>
#include <sstream>

#include <Basics/VelocyPackHelper.h>
#include <velocypack/Iterator.h>

namespace {
int calc_levenshtein(std::string const& lhs, std::string const& rhs) {
  int const lhsLength = static_cast<int>(lhs.size());
  int const rhsLength = static_cast<int>(rhs.size());

  int* col = new int[lhsLength + 1];
  int start = 1;
  // fill with initial values
  std::iota(col + start, col + lhsLength + 1, start);

  for (int x = start; x <= rhsLength; ++x) {
    col[0] = x;
    int last = x - start;
    for (int y = start; y <= lhsLength; ++y) {
      int const save = col[y];
      col[y] = (std::min)({
          col[y] + 1,                                // deletion
          col[y - 1] + 1,                            // insertion
          last + (lhs[y - 1] == rhs[x - 1] ? 0 : 1)  // substitution
      });
      last = save;
    }
  }

  // fetch final value
  int result = col[lhsLength];
  // free memory
  delete[] col;

  return result;
}
}  // namespace

using namespace arangodb::velocypack;

namespace arangodb::greenspun {

void InitMachine(Machine& m) {
  RegisterAllPrimitives(m);
  RegisterAllDateTimeFunctions(m);
  RegisterAllMathFunctions(m);
  RegisterAllStringFunctions(m);
  RegisterAllListFunctions(m);
  RegisterAllDictFunctions(m);
}

EvalResult Apply(Machine& ctx, std::string const& function,
                 VPackSlice const params, VPackBuilder& result) {
  return ctx.applyFunction(function, params, result);
}

EvalResult SpecialIf(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  for (auto iter = paramIterator; iter.valid(); ++iter) {
    auto pair = *iter;
    if (!pair.isArray() || pair.length() != 2) {
      return EvalError("in case " + std::to_string(iter.index()) +
                       ", expected pair, found: " + pair.toJson());
    }

    auto&& [cond, body] = arangodb::basics::VelocyPackHelper::unpackTuple<VPackSlice, VPackSlice>(pair);
    VPackBuilder condResult;
    {
      StackFrameGuard<StackFrameGuardMode::KEEP_SCOPE> guard(ctx);
      auto res = Evaluate(ctx, cond, condResult);
      if (!res) {
        return res.mapError([&](EvalError& err) {
          err.wrapMessage("in condition " + std::to_string(iter.index()));
        });
      }
    }
    if (!condResult.slice().isFalse()) {
      StackFrameGuard<StackFrameGuardMode::KEEP_SCOPE> guard(ctx);
      return Evaluate(ctx, body, result).mapError([&](EvalError& err) {
        err.wrapMessage("in case " + std::to_string(iter.index()));
      });
    }
  }

  result.add(VPackSlice::nullSlice());
  return {};
}

EvalResult SpecialQuote(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  if (!paramIterator.valid()) {
    return EvalError("quote expects one parameter");
  }

  auto value = *paramIterator++;
  if (paramIterator.valid()) {
    return EvalError("Excess elements in quote call");
  }

  result.add(value);
  return {};
}

EvalResult SpecialQuoteSplice(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  if (!result.isOpenArray()) {
    return EvalError("quote-splice nothing to splice into");
  }
  if (!paramIterator.valid()) {
    return EvalError("quote expects one parameter");
  }
  auto value = *paramIterator++;
  if (paramIterator.valid()) {
    return EvalError("Excess elements in quote call");
  }
  if (!value.isArray()) {
    return EvalError("Can only splice lists, found: " + value.toJson());
  }

  result.add(VPackArrayIterator(value));
  return {};
}

EvalResult SpecialCons(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  auto&& [head, list] = arangodb::basics::VelocyPackHelper::unpackTuple<VPackSlice, VPackSlice>(paramIterator);
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

EvalResult SpecialAnd(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  for (; paramIterator.valid(); ++paramIterator) {
    VPackBuilder value;
    if (auto res = Evaluate(ctx, *paramIterator, value); res.fail()) {
      return res.mapError([&](EvalError& err) {
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

EvalResult SpecialOr(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  for (; paramIterator.valid(); ++paramIterator) {
    VPackBuilder value;
    if (auto res = Evaluate(ctx, *paramIterator, value); res.fail()) {
      return res.mapError([&](EvalError& err) {
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

EvalResult SpecialSeq(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  VPackBuilder store;
  if (!paramIterator.valid()) {
    result.add(VPackSlice::nullSlice());
    return {};
  }

  for (; paramIterator.valid(); ++paramIterator) {
    auto& usedBuilder = std::invoke([&]() -> VPackBuilder& {
      if (paramIterator.isLast()) {
        return result;
      }

      store.clear();
      return store;
    });
    if (auto res = Evaluate(ctx, *paramIterator, usedBuilder); res.fail()) {
      return res.mapError([&](EvalError& err) {
        err.wrapMessage("at position " + std::to_string(paramIterator.index()));
      });
    }
  }

  return {};
}

EvalResult SpecialMatch(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
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
  ++paramIterator;
  for (; paramIterator.valid(); ++paramIterator) {
    auto pair = *paramIterator;
    if (!pair.isArray() || pair.length() != 2) {
      return EvalError("in case " + std::to_string(paramIterator.index()) +
                       ", expected pair, found: " + pair.toJson());
    }
    auto&& [cmp, body] = arangodb::basics::VelocyPackHelper::unpackTuple<VPackSlice, VPackSlice>(pair);
    VPackBuilder cmpValue;
    if (auto res = Evaluate(ctx, cmp, cmpValue); !res) {
      return res.mapError([&](EvalError& err) {
        err.wrapMessage("in condition " + std::to_string(paramIterator.index() - 1));
      });
    }

    if (!cmpValue.slice().isNumber()) {
      return EvalError("in condition " + std::to_string(paramIterator.index() - 1) +
                       " expected numeric value, found: " + cmpValue.toJson());
    }

    if (pattern == cmpValue.slice().getNumber<double>()) {
      return Evaluate(ctx, body, result).mapError([&](EvalError& err) {
        err.wrapMessage("in case " + std::to_string(paramIterator.index() - 1));
      });
    }
  }

  result.add(VPackSlice::nullSlice());
  return {};
}

EvalResult SpecialForEach(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  if (!paramIterator.valid()) {
    return EvalError("Expected at least one argument");
  }

  auto lists = *paramIterator++;

  struct IteratorTriple {
    std::string_view varName;
    VPackBuilder value;
    ArrayIterator iterator;

    IteratorTriple(std::string_view name, VPackBuilder v)
        : varName(name), value(std::move(v)), iterator(value.slice()) {}
  };

  std::vector<IteratorTriple> iterators;

  auto const readIteratorPair = [&](VPackSlice pair) -> EvalResult {
    if (!pair.isArray() || pair.length() != 2) {
      return EvalError("Expected pair, found: " + pair.toJson());
    }
    auto&& [var, array] = arangodb::basics::VelocyPackHelper::unpackTuple<VPackSlice, VPackSlice>(pair);
    if (!var.isString()) {
      return EvalError("Expected string as first entry, found: " + var.toJson());
    }
    if (!array.isArray()) {
      return EvalError("Expected array as second entry, found: " + array.toJson());
    }
    VPackBuilder listResult;
    if (auto res = Evaluate(ctx, array, listResult); res.fail()) {
      return res;
    }

    iterators.emplace_back(var.stringView(), std::move(listResult));
    return {};
  };

  if (!lists.isArray()) {
    return EvalError("first parameter expected to be list, found: " + lists.toJson());
  }

  for (auto&& pair : VPackArrayIterator(lists)) {
    if (auto res = readIteratorPair(pair); res.fail()) {
      return res.mapError([&](EvalError& err) {
        err.wrapMessage("at position " + std::to_string(paramIterator.index() - 1));
      });
    }
  }

  auto const runIterators = [&](Machine& ctx, std::size_t index, auto next) -> EvalResult {
    if (index == iterators.size()) {
      VPackBuilder sink;
      return SpecialSeq(ctx, paramIterator, sink).mapError([](EvalError& err) {
        err.wrapMessage("in evaluation of for-statement");
      });
    } else {
      auto pair = iterators.at(index);
      for (auto&& x : pair.iterator) {
        StackFrameGuard<StackFrameGuardMode::NEW_SCOPE> guard(ctx);
        ctx.setVariable(std::string{pair.varName}, x);
        if (auto res = next(ctx, index + 1, next); res.fail()) {
          return res.mapError([&](EvalError& err) {
            std::string msg = "with ";
            msg += pair.varName;
            msg += " = ";
            msg += x.toJson();
            err.wrapMessage(std::move(msg));
          });
        }
      }
      return {};
    }
  };
  result.add(VPackSlice::nullSlice());
  return runIterators(ctx, 0, runIterators);
}

EvalResult Call(Machine& ctx, VPackSlice const functionSlice, ArrayIterator paramIterator,
                VPackBuilder& result, bool isEvaluateParameter) {
  VPackBuilder paramBuilder;
  if (isEvaluateParameter) {
    VPackArrayBuilder builder(&paramBuilder);
    for (; paramIterator.valid(); ++paramIterator) {
      StackFrameGuard<StackFrameGuardMode::KEEP_SCOPE> guard(ctx);
      if (auto res = Evaluate(ctx, *paramIterator, paramBuilder); !res) {
        return res.mapError([&](EvalError& err) {
          err.wrapParameter(functionSlice.copyString(), paramIterator.index());
        });
      }
    }
  } else {
    VPackArrayBuilder builder(&paramBuilder);
    paramBuilder.add(paramIterator);
  }
  return Apply(ctx, functionSlice.copyString(), paramBuilder.slice(), result);
}

EvalResult LambdaCall(Machine& ctx, VPackSlice paramNames, VPackSlice captures,
                      ArrayIterator paramIterator, VPackSlice body,
                      VPackBuilder& result, bool isEvaluateParams) {
  if (!paramNames.isArray()) {
    return EvalError(
        "bad lambda format: expected parameter name array, found: " + paramNames.toJson());
  }

  VPackBuilder paramBuilder;
  if (isEvaluateParams) {
    VPackArrayBuilder builder(&paramBuilder);
    for (; paramIterator.valid(); ++paramIterator) {
      StackFrameGuard<StackFrameGuardMode::KEEP_SCOPE> guard(ctx);
      if (auto res = Evaluate(ctx, *paramIterator, paramBuilder); !res) {
        return res.mapError([&](EvalError& err) {
          err.wrapParameter("<lambda>" + captures.toJson() + paramNames.toJson(),
                            paramIterator.index());
        });
      }
    }
  }

  StackFrameGuard<StackFrameGuardMode::NEW_SCOPE_HIDE_PARENT> captureFrameGuard(ctx);
  for (auto&& pair : ObjectIterator(captures)) {
    ctx.setVariable(pair.key.copyString(), pair.value);
  }

  StackFrameGuard<StackFrameGuardMode::NEW_SCOPE> parameterFrameGuard(ctx);
  VPackArrayIterator builderIter =
      (isEvaluateParams ? VPackArrayIterator(paramBuilder.slice()) : paramIterator);
  for (auto&& paramName : ArrayIterator(paramNames)) {
    if (!paramName.isString()) {
      return EvalError(
          "bad lambda format: expected parameter name (string), found: " +
          paramName.toJson());
    }

    if (!builderIter.valid()) {
      return EvalError("lambda expects " + std::to_string(paramNames.length()) +
                       " parameters " + paramNames.toJson() + ", found " +
                       std::to_string(builderIter.index()));
    }

    ctx.setVariable(paramName.copyString(), *builderIter);
    ++builderIter;
  }
  return Evaluate(ctx, body, result).mapError([&](EvalError& err) {
    VPackBuilder foo;
    {
      VPackArrayBuilder ab(&foo);
      foo.add(isEvaluateParams ? VPackArrayIterator(paramBuilder.slice()) : paramIterator);
    }
    err.wrapCall("<lambda>" + captures.toJson() + paramNames.toJson(), foo.slice());
  });
}

EvalResult SpecialLet(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  std::vector<VPackBuilder> store;

  if (!paramIterator.valid()) {
    return EvalError("Expected at least one argument");
  }

  auto bindings = *paramIterator++;
  if (!bindings.isArray()) {
    return EvalError("Expected list of bindings, found: " + bindings.toJson());
  }

  StackFrame frame(ctx.getAllVariables());  // immer_map does not support transient

  for (VPackArrayIterator iter(bindings); iter.valid(); ++iter) {
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
        return res.mapError([&](EvalError& err) {
          err.wrapMessage("when evaluating value for binding `" + nameSlice.copyString() +
                          "` at position " + std::to_string(iter.index()));
        });
      }

      if (auto res = frame.setVariable(nameSlice.copyString(), builder.slice());
          res.fail()) {
        return res;
      }
    } else {
      return EvalError("expected pair at position " + std::to_string(iter.index()) +
                       " at list of bindings, found: " + pair.toJson());
    }
  }

  StackFrameGuard<StackFrameGuardMode::NEW_SCOPE> guard(ctx, std::move(frame));

  // Now do a seq evaluation of the remaining parameter
  return SpecialSeq(ctx, paramIterator, result).mapError([](EvalError& err) {
    err.wrapMessage("in evaluation of let-statement");
  });
}

EvalResult Evaluate(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result);

EvalResult SpecialQuasiQuoteInternal(Machine& ctx, ArrayIterator other, VPackBuilder& result) {
  if (other.valid()) {
    Slice first = *other;
    if (first.isString() && first.isEqualString("unquote")) {
      ++other;
      if (!other.valid() || !other.isLast()) {
        return EvalError("expected one parameter for unquote");
      }
      return Evaluate(ctx, *other, result);
    } else if (first.isString() && first.isEqualString("unquote-splice")) {
      ++other;
      if (!other.valid() || !other.isLast()) {
        return EvalError("expected one parameter for unquote");
      }
      VPackBuilder tempResult;
      if (auto res = Evaluate(ctx, *other, tempResult); res.fail()) {
        return res;
      }
      auto tempSlice = tempResult.slice();
      if (tempSlice.isArray()) {
        result.add(VPackArrayIterator(tempSlice));
      } else {
        result.add(tempSlice);
      }
      return {};
    }
  }

  {
    VPackArrayBuilder ab(&result);

    for (; other.valid(); ++other) {
      auto&& part = *other;
      if (part.isArray()) {
        if (auto res = SpecialQuasiQuoteInternal(ctx, VPackArrayIterator(part), result);
            res.fail()) {
          return res;
        }
      } else {
        result.add(part);
      }
    }
  }

  return {};
}

EvalResult SpecialQuasiQuote(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  if (!paramIterator.valid()) {
    return EvalError("quasi-quote expects one parameter");
  }

  auto value = *paramIterator++;
  if (paramIterator.valid()) {
    return EvalError("Excess elements in quasi-quote call");
  }
  if (value.isArray()) {
    return SpecialQuasiQuoteInternal(ctx, ArrayIterator(value), result);
  }

  result.add(value);
  return {};
}

EvalResult EvaluateApply(Machine& ctx, VPackSlice const functionSlice,
                         ArrayIterator paramIterator, VPackBuilder& result,
                         bool isEvaluateParameter) {
  if (functionSlice.isString()) {
    auto callSpecialForm = [&](auto specialForm) {
      return specialForm(ctx, paramIterator, result).mapError([&](EvalError& err) {
        return err.wrapSpecialForm(functionSlice.copyString());
      });
    };
    // check for special forms
    if (functionSlice.isEqualString("if")) {
      return callSpecialForm(SpecialIf);
    } else if (functionSlice.isEqualString("quote")) {
      return callSpecialForm(SpecialQuote);
    } else if (functionSlice.isEqualString("quote-splice")) {
      return callSpecialForm(SpecialQuoteSplice);
    } else if (functionSlice.isEqualString("quasi-quote")) {
      return callSpecialForm(SpecialQuasiQuote);
    } else if (functionSlice.isEqualString("cons")) {
      return callSpecialForm(SpecialCons);
    } else if (functionSlice.isEqualString("and")) {
      return callSpecialForm(SpecialAnd);
    } else if (functionSlice.isEqualString("or")) {
      return callSpecialForm(SpecialOr);
    } else if (functionSlice.isEqualString("seq")) {
      return callSpecialForm(SpecialSeq);
    } else if (functionSlice.isEqualString("match")) {
      return callSpecialForm(SpecialMatch);
    } else if (functionSlice.isEqualString("for-each")) {
      return callSpecialForm(SpecialForEach);
    } else if (functionSlice.isEqualString("let")) {
      return callSpecialForm(SpecialLet);
    } else {
      return Call(ctx, functionSlice, paramIterator, result, isEvaluateParameter);
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
      return LambdaCall(ctx, params, captures, paramIterator, body, result, isEvaluateParameter);
    }
  }
  return EvalError("function is not a string, found " + functionSlice.toJson());
}

template <typename F>
auto exceptionIntoResult(F&& f) -> std::invoke_result_t<F> try {
  return std::forward<F>(f)();
} catch (std::exception const& e) {
  return EvalError(std::string{"uncaught exception with message: "} + e.what());
} catch (...) {
  return EvalError("uncaught exception");
}

EvalResult Evaluate(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  return exceptionIntoResult([&]() -> EvalResult {
    if (!paramIterator.valid()) {
      return EvalError("empty application");
    }

    VPackBuilder functionBuilder;
    {
      StackFrameGuard<StackFrameGuardMode::KEEP_SCOPE> guard(ctx);
      auto err = Evaluate(ctx, *paramIterator, functionBuilder);
      if (err.fail()) {
        return err.mapError(
            [&](EvalError& err) { err.wrapMessage("in function expression"); });
      }
    }
    ++paramIterator;
    VPackSlice functionSlice = functionBuilder.slice();
    return EvaluateApply(ctx, functionSlice, paramIterator, result, true);
  });
}

EvalResult Evaluate(Machine& ctx, VPackSlice const slice, VPackBuilder& result) {
  if (slice.isArray()) {
    return Evaluate(ctx, ArrayIterator(slice), result);
  }

  result.add(slice);
  return {};
}

Machine::Machine() {
  // Top level variables
  pushStack();
}

EvalResult Machine::getVariable(const std::string& name, VPackBuilder& result) {
  if (!frames.empty()) {
    if (auto* iter = frames.back().bindings.find(name); iter) {
      result.add(*iter);
      return {};
    }
  }
  result.add(VPackSlice::nullSlice());
  return EvalError("variable `" + name + "` not found");
}

EvalResult StackFrame::getVariable(std::string const& name, VPackBuilder& result) const {
  if (auto* iter = bindings.find(name); iter) {
    result.add(*iter);
    return {};
  }
  result.add(VPackSlice::nullSlice());
  return EvalError("variable `" + name + "` not found");
}

EvalResult StackFrame::setVariable(const std::string& name, VPackSlice value) {
  if (auto i = bindings.find(name); i) {
    return EvalError("duplicate variable `" + name + "`");
  }

  bindings = bindings.set(name, value);
  return {};
}

EvalResult Machine::setVariable(std::string const& name, VPackSlice value) {
  TRI_ASSERT(!frames.empty());
  return frames.back().setVariable(name, value);
}

void Machine::pushStack(bool noParentScope) {
  frames.emplace_back(noParentScope ? VariableBindings{} : getAllVariables());
}

void Machine::emplaceStack(StackFrame sf) {
  frames.emplace_back(std::move(sf));
}

void Machine::popStack() {
  // Top level variables must not be popped
  TRI_ASSERT(frames.size() > 1);

  frames.pop_back();
}

EvalResult Machine::setFunction(std::string_view name, function_type&& f) {
  auto sname = std::string{name};

  if (functions.find(sname) != functions.end()) {
    return EvalError("function `" + sname + "` already registered");
  }
  functions.try_emplace(sname, std::move(f));
  return {};
}

EvalResult Machine::applyFunction(std::string function, VPackSlice const params,
                                  VPackBuilder& result) {
  TRI_ASSERT(params.isArray());

  auto f = functions.find(function);
  if (f != functions.end()) {
    return f->second(*this, params, result).mapError([&](EvalError& err) {
      err.wrapCall(function, params);
    });
  }

  std::string minFunctionName;
  int minDistance = std::numeric_limits<int>::max();

  for (auto&& [key, value] : functions) {
    int distance = calc_levenshtein(function, key);
    if (distance < minDistance) {
      minFunctionName = key;
      minDistance = distance;
    }
  }

  return EvalError(
      "function not found `" + function + "`" +
      (!minFunctionName.empty() ? ", did you mean `" + minFunctionName + "`?" : ""));
}

VariableBindings Machine::getAllVariables() {
  if (frames.empty()) {
    return VariableBindings{};
  } else {
    return frames.back().bindings;
  }
}

std::string EvalError::toString() const {
  std::stringstream ss;

  struct PrintErrorVisistor {
    std::stringstream& ss;

    void operator()(EvalError::CallFrame const& f) {
      ss << "in function `" << f.function << "` called with (";
      bool first = true;
      for (auto&& s : f.parameter) {
        if (!first) {
          ss << ",";
        } else {
          first = false;
        }
        ss << " `" << s << "`";
      }
      ss << " )" << std::endl;
    }

    void operator()(EvalError::SpecialFormFrame const& f) {
      ss << "when evaluating special form `" << f.specialForm << "`" << std::endl;
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

std::string paramsToString(VPackArrayIterator const iter) {
  std::stringstream ss;

  for (auto&& p : iter) {
    if (p.isString()) {
      ss << p.stringView();
    } else if (p.isNumber()) {
      ss << p.getNumber<double>();
    } else if (p.isBool()) {
      ss << std::boolalpha << p.getBool();
    } else {
      ss << p.toJson();
    }
    ss << " ";
  }
  return ss.str();
}

std::string paramsToString(VPackSlice const params) {
  return paramsToString(VPackArrayIterator(params));
}

}  // namespace arangodb::greenspun
