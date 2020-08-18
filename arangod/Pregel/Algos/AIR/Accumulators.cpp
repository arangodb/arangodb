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

using namespace arangodb::pregel::algos::accumulators;

auto CustomAccumulator<VPackSlice>::update(VPackSlice v)
    -> greenspun::EvalResultT<AccumulatorBase::UpdateResult> {
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

auto CustomAccumulator<VPackSlice>::clearWithResult() -> greenspun::EvalResult {
  VPackBuilder result;
  return greenspun::Evaluate(_machine, _definition.clearProgram.slice(), result)
      .mapError([](auto& err) {
        err.wrapMessage("in clearProgram of custom accumulator");
      });
}

void CustomAccumulator<VPackSlice>::serializeIntoBuilder(VPackBuilder& result) {
  Accumulator::serializeIntoBuilder(result);
}

CustomAccumulator<VPackSlice>::CustomAccumulator(const VertexData& owner,
                                                 const AccumulatorOptions& options,
                                                 const CustomAccumulatorDefinitions& defs)
    : Accumulator<VPackSlice>(owner, options, defs) {
  _definition = defs.at(*options.customType);
  greenspun::InitMachine(_machine);
  SetupFunctions();
}

auto CustomAccumulator<VPackSlice>::setBySliceWithResult(VPackSlice v)
    -> arangodb::greenspun::EvalResult {
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

auto CustomAccumulator<VPackSlice>::getIntoBuilderWithResult(VPackBuilder& result)
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

void CustomAccumulator<VPackSlice>::SetupFunctions() {
  _machine.setFunctionMember("input-sender", &CustomAccumulator<VPackSlice>::AIR_InputSender, this);
  _machine.setFunctionMember("input-value", &CustomAccumulator<VPackSlice>::AIR_InputValue, this);
  _machine.setFunctionMember("current-value", &CustomAccumulator<VPackSlice>::AIR_CurrentValue, this);
  _machine.setFunctionMember("get-current-value", &CustomAccumulator<VPackSlice>::AIR_GetCurrentValue, this);
  _machine.setFunctionMember("this-set!", &CustomAccumulator<VPackSlice>::AIR_ThisSet, this);
  _machine.setFunctionMember("parameters", &CustomAccumulator<VPackSlice>::AIR_Parameters, this);
}

CustomAccumulator<VPackSlice>::~CustomAccumulator() = default;
