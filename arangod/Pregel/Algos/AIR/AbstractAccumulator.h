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

#ifndef ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_ABSTRACT_ACCUMULATOR_H
#define ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_ABSTRACT_ACCUMULATOR_H 1
#include <Greenspun/EvalResult.h>
#include <iostream>
#include <numeric>

#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"

#include "AccumulatorOptions.h"
#include "MessageData.h"

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {
class VertexData;

template <typename T>
class Accumulator;

struct AccumulatorBase {
  AccumulatorBase() = default;
  virtual ~AccumulatorBase() = default;

  enum class UpdateResult {
    CHANGED,
    NO_CHANGE,
  };

  // Resets the accumulator to a well-known value
  virtual auto clear() -> greenspun::EvalResult = 0;
  virtual auto setBySlice(VPackSlice v) -> greenspun::EvalResult = 0;
  virtual auto getIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult = 0;

  // This conflates two operations: updating the accumulator, and passing the
  // sender of the update message into the accumulator, i.e. the message passing and
  // the accumulator operation
  // One of these two operations should also be obsolete. This will need some consideration
  // wrt "efficiency" (whether velocypack is the best format for messages here. It probably is,
  // in particular since we can prevent copying stuff around)
  virtual auto updateByMessageSlice(VPackSlice msg) -> greenspun::EvalResultT<UpdateResult> = 0;
  virtual auto updateByMessage(MessageData const& msg) -> greenspun::EvalResultT<UpdateResult> = 0;

  // used to set state on WorkerContext from message by MasterContext
  virtual auto setStateBySlice(VPackSlice msg) -> greenspun::EvalResult = 0;
  // used to set state on WorkerContext from message by MasterContext
  virtual auto getStateIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult = 0;
  // used to send updates from WorkerContext to MasterContext, output of this
  // is given to aggregateStateBySlice on MasterContext
  virtual auto getStateUpdateIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult = 0;
  // used to aggregate states on MasterContext after receiving messages from WorkerContexts.
  virtual auto aggregateStateBySlice(VPackSlice msg) -> greenspun::EvalResult = 0;

  virtual auto finalizeIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult = 0;
};

template<typename T>
constexpr auto always_false_v = false;

template <typename T>
class Accumulator : public AccumulatorBase {
 public:
  using data_type = T;

  explicit Accumulator(AccumulatorOptions const&, CustomAccumulatorDefinitions const&) {}
  ~Accumulator() override = default;

  auto clear() -> greenspun::EvalResult override {
    this->set(T{});
    return {};
  }

  // Needed to implement set by slice and clear
  virtual auto set(data_type v) -> greenspun::EvalResult {
    _value = std::move(v);
    return {};
  }

  // Needed to implement updates by slice
  virtual auto update(data_type v) -> greenspun::EvalResultT<UpdateResult> {
    return greenspun::EvalError("update by slice not implemented");
  }

  auto setBySlice(VPackSlice s) -> greenspun::EvalResult override {
    if constexpr (std::is_same_v<T, bool>) {
      this->set(s.getBool());
      return {};
    } else if constexpr (std::is_arithmetic_v<T>) {
      this->set(s.getNumericValue<T>());
      return {};
    } else if constexpr (std::is_same_v<VPackSlice, T>) {
      return this->set(s);
    } else if constexpr (std::is_same_v<std::string, T>) {
      return this->set(s.copyString());
    } else {
      static_assert(always_false_v<T>);
      return greenspun::EvalError("set by slice not implemented");
    }
  }

  auto updateBySlice(VPackSlice s) -> greenspun::EvalResultT<UpdateResult> {
      if constexpr (std::is_same_v<T, bool>) {
        return this->update(s.getBool());
      } else if constexpr (std::is_arithmetic_v<T>) {
        return this->update(s.getNumericValue<T>());
      } else if constexpr (std::is_same_v<VPackSlice, T>) {
        return this->update(s);
      } else if constexpr (std::is_same_v<std::string, T>) {
        return this->update(s.copyString());
      } else {
        static_assert(always_false_v<T>);
      }
  }

  auto updateByMessageSlice(VPackSlice msg) -> greenspun::EvalResultT<UpdateResult> override {
    return updateBySlice(msg.get("value"));
  }

  auto updateByMessage(MessageData const& msg) -> greenspun::EvalResultT<UpdateResult> override {
    return updateBySlice(msg._value.slice());
  }

  auto setStateBySlice(VPackSlice s) -> greenspun::EvalResult override {
    return setBySlice(s);
  }
  auto getStateIntoBuilder(VPackBuilder& msg) -> greenspun::EvalResult override {
    return getIntoBuilder(msg);
  }
  auto getStateUpdateIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult override {
    return getIntoBuilder(result);
  }
  auto aggregateStateBySlice(VPackSlice msg) -> greenspun::EvalResult override {
    return updateBySlice(msg).asResult();
  }

  auto getIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult override {
    if constexpr (std::is_same_v<T, VPackSlice>) {
      result.add(_value);
    } else {
      result.add(VPackValue(_value));
    }
    return {};
  }

  auto finalizeIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult override {
    return getIntoBuilder(result);
  }

protected:
  data_type _value;
};

std::unique_ptr<AccumulatorBase> instantiateAccumulator(AccumulatorOptions const& options,
                                                        CustomAccumulatorDefinitions const& customDefinitions);
bool isValidAccumulatorOptions(AccumulatorOptions const& options);

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
