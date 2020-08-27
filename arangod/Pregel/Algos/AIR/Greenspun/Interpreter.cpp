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
#include "Primitives.h"
#include "lib/DateTime.h"

#include <iostream>
#include <sstream>

#include <Basics/VelocyPackHelper.h>
#include <velocypack/Iterator.h>

using namespace arangodb::velocypack;

namespace arangodb::greenspun {

void InitMachine(Machine& m) {
  RegisterAllPrimitives(m);
  // FIXME: Demo hack. better provide real libraries!
  // Also adds a dependency on Basics which is just a whole lot of pain I don't
  // want right now.
  // RegisterAllDateTimeFunctions(m);
}

EvalResult Apply(Machine& ctx, std::string const& function,
                 VPackSlice const params, VPackBuilder& result) {
  return ctx.applyFunction(function, params, result);
}

EvalResult SpecialIf(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
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
        return res.mapError([&](EvalError& err) {
          err.wrapMessage("in condition " + std::to_string(iter.index()));
        });
      }
    }
    if (!condResult.slice().isFalse()) {
      StackFrameGuard<false> guard(ctx);
      return Evaluate(ctx, body, result).mapError([&](EvalError& err) {
        err.wrapMessage("in case " + std::to_string(iter.index()));
      });
    }
  }

  result.add(VPackSlice::noneSlice());
  return {};
}

EvalResult SpecialQuote(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  VPackArrayBuilder array(&result);
  result.add(paramIterator);
  return {};
}

EvalResult SpecialQuoteSplice(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  if (!result.isOpenArray()) {
    return EvalError("nothing to splice into");
  }
  result.add(paramIterator);
  return {};
}

EvalResult SpecialCons(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
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

EvalResult SpecialAnd(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  for (; paramIterator.valid(); paramIterator++) {
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
  for (; paramIterator.valid(); paramIterator++) {
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
  for (; paramIterator.valid(); paramIterator++) {
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

  result.add(VPackSlice::noneSlice());
  return {};
}

EvalResult SpecialForEach(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
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
      return res.mapError([&](EvalError& err) {
        err.wrapMessage("at position " + std::to_string(paramIterator.index() - 1));
      });
    }
  }

  VPackSlice body = paramIterator.value();

  auto const runIterators = [&](Machine& ctx, std::size_t index, auto next) -> EvalResult {
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

EvalResult Call(Machine& ctx, VPackSlice const functionSlice, ArrayIterator paramIterator,
                VPackBuilder& result, bool isEvaluateParameter) {
  VPackBuilder paramBuilder;
  if (isEvaluateParameter) {
    VPackArrayBuilder builder(&paramBuilder);
    for (; paramIterator.valid(); ++paramIterator) {
      StackFrameGuard<false> guard(ctx);
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
  VPackBuilder paramBuilder;
  if (isEvaluateParams) {
    VPackArrayBuilder builder(&paramBuilder);
    for (; paramIterator.valid(); ++paramIterator) {
      StackFrameGuard<false> guard(ctx);
      if (auto res = Evaluate(ctx, *paramIterator, paramBuilder); !res) {
        return res.mapError([&](EvalError& err) {
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
                       std::to_string(builderIter.index() - 1));
    }

    ctx.setVariable(paramName.copyString(), *builderIter);
    builderIter++;
  }
  return Evaluate(ctx, body, result).mapError([&](EvalError& err) {
    VPackBuilder foo;
    {
      VPackArrayBuilder ab(&foo);
      foo.add(isEvaluateParams ? VPackArrayIterator(paramBuilder.slice()) : paramIterator);
    }
    err.wrapCall("<lambda>" + captures.toJson() + paramNames.toJson(),
                 foo.slice());
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
        return res.mapError([&](EvalError& err) {
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
  return SpecialSeq(ctx, paramIterator, result).mapError([](EvalError& err) {
    err.wrapMessage("in evaluation of let-statement");
  });
}


EvalResult Evaluate(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result);


EvalResult SpecialQuasiQuote(Machine& ctx, ArrayIterator other, VPackBuilder& result) {

  if (other.valid()) {
    Slice first = *other;
    if (first.isString() && first.isEqualString("unquote")) {
      other++;
      if (!other.valid() || !other.isLast()) {
        return EvalError("expected one parameter for unquote");
      }
      return Evaluate(ctx, *other, result);
    } else if (first.isString() && first.isEqualString("unquote-splice")) {
      other++;
      if (!other.valid() || !other.isLast()) {
        return EvalError("expected one parameter for unquote");
      }
      VPackBuilder tempResult;
      if (auto res = Evaluate(ctx, *other, tempResult); res.fail()) {
        return res;
      }
      auto tempSlice = tempResult.slice();
      if (tempSlice.isArray()) {
        result.add(VPackArrayIterator (tempSlice));
      } else {
        result.add(tempSlice);
      }
      return {};
    }
  }

  {
    VPackArrayBuilder ab(&result);

    for(; other.valid(); other++) {
      auto&& part = *other;
      if (part.isArray()) {
        if (auto res = SpecialQuasiQuote(ctx, VPackArrayIterator(part), result); res.fail()) {
          return res;
        }
      } else {
        result.add(part);
      }
    }
  }

  return {};
}

EvalResult SpecialStr(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  std::stringstream ss;

  for (; paramIterator.valid(); paramIterator++) {
    if ((*paramIterator).isString()) {
      ss << (*paramIterator).copyString();
    } else {
      return EvalError(std::string{"`str` expecting string, not "} +
                       (*paramIterator).typeName());
    }
  }

  result.add(VPackValue(ss.str()));
  return {};
}

EvalResult EvaluateApply(Machine& ctx, VPackSlice const functionSlice,
                         ArrayIterator paramIterator, VPackBuilder& result,
                         bool isEvaluateParameter) {
  if (functionSlice.isString()) {
    // check for special forms
    if (functionSlice.isEqualString("if")) {
      return SpecialIf(ctx, paramIterator, result);
    } else if (functionSlice.isEqualString("quote")) {
      return SpecialQuote(ctx, paramIterator, result);
    } else if (functionSlice.isEqualString("quote-splice")) {
      return SpecialQuoteSplice(ctx, paramIterator, result);
    } else if (functionSlice.isEqualString("quasi-quote")) {
      return SpecialQuasiQuote(ctx, paramIterator, result);
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
    } else if (functionSlice.isEqualString("str")) {
      return SpecialStr(ctx, paramIterator, result);
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

EvalResult Evaluate(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  if (!paramIterator.valid()) {
    return EvalError("empty application");
  }

  VPackBuilder functionBuilder;
  {
    StackFrameGuard<false> guard(ctx);
    auto err = Evaluate(ctx, *paramIterator, functionBuilder);
    if (err.fail()) {
      return err.mapError(
          [&](EvalError& err) { err.wrapMessage("in function expression"); });
    }
  }
  ++paramIterator;
  VPackSlice functionSlice = functionBuilder.slice();
  return EvaluateApply(ctx, functionSlice, paramIterator, result, true);
}

EvalResult Evaluate(Machine& ctx, VPackSlice const slice, VPackBuilder& result) {
  if (slice.isArray()) {
    return Evaluate(ctx, ArrayIterator(slice), result);
  }

  result.add(slice);
  return {};
}

Machine::Machine() noexcept {
  // Top level variables
  pushStack();
}

EvalResult Machine::getVariable(const std::string& name, VPackBuilder& result) {
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

EvalResult Machine::setVariable(std::string const& name, VPackSlice value) {
  TRI_ASSERT(!variables.empty());
  variables.back().bindings.operator[](name) = value;  // insert or create
  return {};
}

void Machine::pushStack(bool noParentScope) {
  variables.emplace_back().noParentScope = noParentScope;
}

void Machine::popStack() {
  // Top level variables must not be popped
  TRI_ASSERT(variables.size() > 1);

  variables.pop_back();
}

EvalResult Machine::setFunction(std::string_view name, function_type&& f) {
  auto sname = std::string{name};

  if (functions.find(sname) != functions.end()) {
    return EvalError("function `" + sname + "` already registered");
  }
  functions[sname] = std::move(f);
  return {};
}

EvalResult Machine::unsetFunction(std::string_view name) {
  auto sname = std::string{name};

  auto n = functions.erase(sname);
  if (n == 0) {
    return EvalError("function `" + sname + "` not known");
  }
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
  } else {
    return EvalError("function not found `" + function + "`");
  }
  return {};
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

std::string paramsToString(const VPackArrayIterator iter) {
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

std::string paramsToString(const VPackSlice params) {
  return paramsToString(VPackArrayIterator(params));
}

}  // namespace arangodb::greenspun
