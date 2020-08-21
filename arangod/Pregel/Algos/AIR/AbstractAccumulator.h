////////////////////////////////////////////////////////////////////////////////
///
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
///
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_ABSTRACT_ACCUMULATOR_H
#define ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_ABSTRACT_ACCUMULATOR_H 1
#include <Pregel/Algos/AIR/Greenspun/EvalResult.h>
#include <iostream>
#include <numeric>

#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"

#include "AccumulatorOptionsDeserializer.h"
#include "MessageData.h"

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {
class VertexData;

template <typename T>
class Accumulator;

struct AccumulatorBase {
  AccumulatorBase(){};
  virtual ~AccumulatorBase() = default;
  template <typename T>
  Accumulator<T>* castAccumulatorType() {
    return dynamic_cast<Accumulator<T>*>(this);
  }

  enum class UpdateResult {
    CHANGED,
    NO_CHANGE,
  };

  // Resets the accumulator to a well-known value
  virtual auto clear() -> greenspun::EvalResult = 0;

  virtual auto setBySlice(VPackSlice v) -> greenspun::EvalResult = 0;
  virtual auto updateBySlice(VPackSlice v) -> greenspun::EvalResultT<UpdateResult> = 0;
  virtual auto getValueIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult = 0;
  virtual auto finalizeIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult = 0;

  void setSender(std::string const& sender) {
    _sender = sender;
  }
  auto sender() -> std::string const& {
    return _sender;
  }
  std::string _sender;
};

template <typename T>
class Accumulator : public AccumulatorBase {
 public:
  using data_type = T;

  explicit Accumulator(AccumulatorOptions const&, CustomAccumulatorDefinitions const&){};
  ~Accumulator() override = default;

  auto clear() -> greenspun::EvalResult override {
    this->set(T{});
    return {};
  }

  // Needed to implement set by slice and clear
  virtual auto set(data_type v) -> greenspun::EvalResult {
    _value = std::move(v);
    return {};
  };

  // Needed to implement updates by slice
  virtual auto update(data_type v) -> greenspun::EvalResultT<UpdateResult> {
    return greenspun::EvalError("not implemented");
  }

  auto setBySlice(VPackSlice s) -> greenspun::EvalResult override {
    if constexpr (std::is_same_v<T, bool>) {
      this->set(s.getBool());
      return {};
    } else if constexpr (std::is_arithmetic_v<T>) {
      this->set(s.getNumericValue<T>());
      return {};
    } else {
      return greenspun::EvalError("not implemented");
    }
  }

  auto updateBySlice(VPackSlice s) -> greenspun::EvalResultT<UpdateResult> override {
    // TODO proper error handling here!
    if constexpr (std::is_same_v<T, bool>) {
      return this->update(s.getBool());
    } else if constexpr (std::is_arithmetic_v<T>) {
      return this->update(s.getNumericValue<T>());
    } else {
      return greenspun::EvalError("not implemented");
    }
  }

  auto getValueIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult override {
    if constexpr (std::is_same_v<T, VPackSlice>) {
      result.add(_value);
    } else {
      result.add(VPackValue(_value));
    }
    return {};
  }

  auto finalizeIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult override {
    return getValueIntoBuilder(result);
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
