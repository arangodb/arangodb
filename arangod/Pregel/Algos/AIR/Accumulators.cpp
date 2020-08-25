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

#include "Accumulators.h"
#include <Basics/ScopeGuard.h>
#include <Logger/LogMacros.h>

using namespace arangodb::pregel::algos::accumulators;

auto CustomAccumulator<VPackSlice>::updateByMessage(MessageData const& msg)
  -> greenspun::EvalResultT<AccumulatorBase::UpdateResult> {
  this->inputSlice = msg._value.slice();
  this->inputSender =  &msg._sender;

  TRI_DEFER({ this->inputSlice = VPackSlice::noneSlice();
      this->inputSender = nullptr; })

    VPackBuilder result;
  auto res = greenspun::Evaluate(_machine, _definition.updateProgram.slice(), result);
  if (res.fail()) {
    return res.error().wrapMessage("in updateProgram of custom accumulator");
  }

  auto resultSlice = result.slice();
  if (resultSlice.isString()) {
    if (resultSlice.isEqualString("hot")) {
      return UpdateResult::CHANGED;
    } else if (resultSlice.isEqualString("cold")) {
      return UpdateResult::NO_CHANGE;
    }
  } else if (resultSlice.isNone()) {
    return AccumulatorBase::UpdateResult::NO_CHANGE;
  }
  return greenspun::EvalError(
    "update program did not return a valid value: expected `\"hot\"` or "
    "`\"cold\"`, found: " +
    result.toJson());
}

auto CustomAccumulator<VPackSlice>::updateBySlice(VPackSlice s)
    -> greenspun::EvalResultT<AccumulatorBase::UpdateResult> {
  this->inputSlice = s;
  TRI_DEFER({ this->inputSlice = VPackSlice::noneSlice(); })

  VPackBuilder result;
  auto res = greenspun::Evaluate(_machine, _definition.updateProgram.slice(), result);
  if (res.fail()) {
    return res.error().wrapMessage("in updateProgram of custom accumulator");
  }

  auto resultSlice = result.slice();
  if (resultSlice.isString()) {
    if (resultSlice.isEqualString("hot")) {
      return UpdateResult::CHANGED;
    } else if (resultSlice.isEqualString("cold")) {
      return UpdateResult::NO_CHANGE;
    }
  } else if (resultSlice.isNone()) {
    return AccumulatorBase::UpdateResult::NO_CHANGE;
  }
  return greenspun::EvalError(
      "update program did not return a valid value: expected `\"hot\"` or "
      "`\"cold\"`, found: " +
      result.toJson());
}

auto CustomAccumulator<VPackSlice>::clear() -> greenspun::EvalResult {
  VPackBuilder result;
  return greenspun::Evaluate(_machine, _definition.clearProgram.slice(), result).mapError([](auto& err) {
    err.wrapMessage("in clearProgram of custom accumulator");
  });
}

CustomAccumulator<VPackSlice>::CustomAccumulator(const AccumulatorOptions& options,
                                                 const CustomAccumulatorDefinitions& defs)
    : Accumulator<VPackSlice>(options, defs) {
  _definition = defs.at(*options.customType);
  greenspun::InitMachine(_machine);
  SetupFunctions();
  if (options.parameters) {
    _parameters = *options.parameters;
  }
}

auto CustomAccumulator<VPackSlice>::setBySlice(VPackSlice v)
    -> arangodb::greenspun::EvalResult {
  inputSlice = v;
  TRI_DEFER({ inputSlice = Slice::noneSlice(); })

  if (_definition.setProgram.isEmpty()) {
    _buffer.clear();
    _buffer.add(v);
    _value = _buffer.slice();
    return {};
  }

  VPackBuilder result;
  return greenspun::Evaluate(_machine, this->_definition.setProgram.slice(), result)
      .mapError([](auto& err) {
        err.wrapMessage("in setProgram of custom accumulator");
      });
}

auto CustomAccumulator<VPackSlice>::getValueIntoBuilder(VPackBuilder& result)
    -> arangodb::greenspun::EvalResult {
  if (_definition.getProgram.isEmpty()) {
    result.add(_value);
    return {};
  }

  return greenspun::Evaluate(_machine, this->_definition.getProgram.slice(), result)
      .mapError([](auto& err) {
        err.wrapMessage("in getProgram of custom accumulator");
      });
}

auto CustomAccumulator<VPackSlice>::finalizeIntoBuilder(VPackBuilder& result)
    -> arangodb::greenspun::EvalResult {
  if (_definition.finalizeProgram.isEmpty()) {
    result.add(_value);
    return {};
  }

  return greenspun::Evaluate(_machine, this->_definition.finalizeProgram.slice(), result)
      .mapError([](auto& err) {
        err.wrapMessage("in finalizeProgram of custom accumulator");
      });
}

void CustomAccumulator<VPackSlice>::SetupFunctions() {
  _machine.setFunctionMember("input-sender",
                             &CustomAccumulator<VPackSlice>::AIR_InputSender, this);
  _machine.setFunctionMember("input-value",
                             &CustomAccumulator<VPackSlice>::AIR_InputValue, this);
  _machine.setFunctionMember("current-value",
                             &CustomAccumulator<VPackSlice>::AIR_CurrentValue, this);
  _machine.setFunctionMember("get-current-value",
                             &CustomAccumulator<VPackSlice>::AIR_GetCurrentValue, this);
  _machine.setFunctionMember("this-set!", &CustomAccumulator<VPackSlice>::AIR_ThisSet, this);
  _machine.setFunctionMember("parameters",
                             &CustomAccumulator<VPackSlice>::AIR_Parameters, this);
}

auto CustomAccumulator<VPackSlice>::AIR_Parameters(arangodb::greenspun::Machine& ctx,
                                                   const VPackSlice slice,
                                                   VPackBuilder& result)
    -> arangodb::greenspun::EvalResult {
  result.add(_parameters.slice());
  return {};
}

auto CustomAccumulator<VPackSlice>::AIR_ThisSet(arangodb::greenspun::Machine& ctx,
                                                const VPackSlice slice, VPackBuilder& result)
    -> arangodb::greenspun::EvalResult {
  if (!slice.isArray() || slice.length() != 1) {
    return greenspun::EvalError("expected a single argument");
  }

  _buffer.clear();
  _buffer.add(slice.at(0));
  _value = _buffer.slice();
  return {};
}

auto CustomAccumulator<VPackSlice>::AIR_GetCurrentValue(arangodb::greenspun::Machine& ctx,
                                                        VPackSlice slice,
                                                        VPackBuilder& result)
    -> arangodb::greenspun::EvalResult {
  if (!slice.isEmptyArray()) {
    return greenspun::EvalError("expected no arguments");
  }

  return getValueIntoBuilder(result);
}

auto CustomAccumulator<VPackSlice>::AIR_CurrentValue(arangodb::greenspun::Machine& ctx,
                                                     VPackSlice slice, VPackBuilder& result)
    -> arangodb::greenspun::EvalResult {
  if (!slice.isEmptyArray()) {
    return greenspun::EvalError("expected no arguments");
  }

  result.add(_value);
  return {};
}

auto CustomAccumulator<VPackSlice>::AIR_InputValue(arangodb::greenspun::Machine& ctx,
                                                   VPackSlice slice, VPackBuilder& result)
    -> arangodb::greenspun::EvalResult {
  result.add(inputSlice);
  return {};
}

auto CustomAccumulator<VPackSlice>::AIR_InputSender(arangodb::greenspun::Machine& ctx,
                                                    VPackSlice slice, VPackBuilder& result)
    -> arangodb::greenspun::EvalResult {
  if (inputSender != nullptr) {
    result.add(VPackValue(*inputSender));
    return {};
  }
  return greenspun::EvalError("input-sender not available here");
}

CustomAccumulator<VPackSlice>::~CustomAccumulator() = default;
