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
      return EvalError("in case " + std::to_string(iter.index()) + ", expected pair, found: " + pair.toJson());
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
      return res.wrapError([&](EvalError &err) {
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
      return res.wrapError([&](EvalError &err) {
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
      return res.wrapError([&](EvalError &err) {
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
  paramIterator++;
  for (; paramIterator.valid(); paramIterator++) {
    auto pair = *paramIterator;
    if (!pair.isArray() || pair.length() != 2) {
      return EvalError("in case " + std::to_string(paramIterator.index()) + ", expected pair, found: " + pair.toJson());
    }
    auto&& [cmp, body] = unpackTuple<VPackSlice, VPackSlice>(pair);
    VPackBuilder cmpValue;
    if (auto res = Evaluate(ctx, cmp, cmpValue); !res) {
      return res.wrapError([&](EvalError& err) {
        err.wrapMessage("in condition " + std::to_string(paramIterator.index() - 1));
      });
    }

    if (arangodb::basics::VelocyPackHelper::compare(proto.slice(), cmpValue.slice(), true) == 0) {
      return Evaluate(ctx, body, result).wrapError([&](EvalError &err) {
        err.wrapMessage("in case " + std::to_string(paramIterator.index() - 1));
      });
    }
  }

  result.add(VPackSlice::noneSlice());
  return {};
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

EvalResult Evaluate(EvalContext& ctx, VPackSlice const slice, VPackBuilder& result) {
  if (slice.isArray()) {
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



EvalContext::EvalContext() noexcept {
  // Top level variables
  pushStack();
}


EvalResult EvalContext::getVariable(const std::string& name, VPackBuilder& result) {
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

EvalResult EvalContext::setVariable(std::string name, VPackSlice value) {
  TRI_ASSERT(!variables.empty());
  variables.back().emplace(std::move(name), value);
  return {};
}

void EvalContext::pushStack() { variables.emplace_back(); }

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
