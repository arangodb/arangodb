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

#include <iostream>
#include <sstream>

#include <Basics/VelocyPackHelper.h>
#include <velocypack/Iterator.h>

using namespace arangodb::velocypack;

namespace arangodb {
namespace greenspun {

void InitMachine(Machine& m) { RegisterAllPrimitives(m); }

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

EvalResult SpecialQuote(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
  VPackArrayBuilder array(&result);
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

EvalResult SpecialOr(Machine& ctx, ArrayIterator paramIterator, VPackBuilder& result) {
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
      return res.wrapError([&](EvalError& err) {
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
      return res.wrapError([&](EvalError& err) {
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

EvalResult Call(Machine& ctx, VPackSlice const functionSlice,
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

EvalResult Evaluate(Machine& ctx, VPackSlice const slice, VPackBuilder& result) {
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
      } else {
        return Call(ctx, functionSlice, paramIterator, result);
      }
    } else {
      return EvalError("function is not a string, found " + functionSlice.toJson());
    }
  } else {
    result.add(slice);
    return {};
  }
}

Machine::Machine() noexcept {
  // Top level variables
  pushStack();
}

EvalResult Machine::getVariable(const std::string& name, VPackBuilder& result) {
  for (auto scope = variables.rbegin(); scope != variables.rend(); ++scope) {
    auto iter = scope->find(name);
    if (iter != std::end(*scope)) {
      result.add(iter->second);
      return {};
    }
  }
  // TODO: variable not found error.
  result.add(VPackSlice::noneSlice());
  return EvalError("variable `" + name + "` not found");
}

EvalResult Machine::setVariable(std::string name, VPackSlice value) {
  TRI_ASSERT(!variables.empty());
  variables.back().emplace(std::move(name), value);
  return {};
}

void Machine::pushStack() { variables.emplace_back(); }

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
  functions[sname] = f;
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
    return f->second(*this, params, result).wrapError([&](EvalError& err) {
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

}  // namespace greenspun
}  // namespace arangodb
