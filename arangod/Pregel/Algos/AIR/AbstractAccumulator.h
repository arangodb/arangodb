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
#include <numeric>
#include <iostream>

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
  AccumulatorBase() {};
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
  virtual auto clearWithResult() -> greenspun::EvalResult = 0;

  virtual void serializeIntoBuilder(VPackBuilder& result) = 0;
  virtual auto finalizeIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult = 0;
  virtual auto setBySliceWithResult(VPackSlice v) -> greenspun::EvalResult = 0;
  virtual auto getIntoBuilderWithResult(VPackBuilder& result) -> greenspun::EvalResult = 0;
  virtual auto updateByMessage(MessageData const& msg) -> greenspun::EvalResultT<UpdateResult> = 0;
  virtual auto updateBySlice(VPackSlice v) -> greenspun::EvalResultT<UpdateResult> = 0;

  // Returns a pointer to the value of this accumulator.
  virtual const void* getValuePointer() const { return nullptr; }
  // Set the value from a value pointer.
  virtual void setValueFromPointer(const void*) { }
  virtual auto updateValueFromPointer(const void*) -> greenspun::EvalResultT<UpdateResult> {
    return UpdateResult::NO_CHANGE;
  }
};

template <typename T>
class Accumulator : public AccumulatorBase {
 public:
  using data_type = T;

  explicit Accumulator( AccumulatorOptions const&, CustomAccumulatorDefinitions const&){};
  ~Accumulator() override = default;

  // Needed to implement set by slice and clear
  virtual void set(data_type v) { _value = std::move(v); };

  auto clearWithResult() -> greenspun::EvalResult override {
    this->clear();
    return {};
  }
  virtual void clear() { this->set(T{}); }

  auto setBySliceWithResult(VPackSlice v) -> greenspun::EvalResult override {
    this->setBySlice(v);
    return {};
  }

  auto updateByMessage(MessageData const& msg) -> greenspun::EvalResultT<UpdateResult> override {
    return this->updateByMessageSlice(msg._value.slice());
  }

  auto getIntoBuilderWithResult(VPackBuilder& result) -> greenspun::EvalResult override {
    this->getValueIntoBuilder(result);
    return {};
  }

  void serializeIntoBuilder(VPackBuilder& result) override {
    this->getValueIntoBuilder(result);
  }

  auto finalizeIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult override {
    serializeIntoBuilder(result);
    return {};
  }

  // Needed to implement updates by slice
  virtual greenspun::EvalResultT<UpdateResult> update(data_type v) {
      return greenspun::EvalError("not implemented");
  }

  virtual greenspun::EvalResultT<UpdateResult> updateByMessageSlice(VPackSlice s) {
    // TODO proper error handling here!
    if constexpr (std::is_same_v<T, bool>) {
      return this->update(s.getBool());
    } else if constexpr (std::is_arithmetic_v<T>) {
      return this->update(s.getNumericValue<T>());
    } else {
      std::abort();
    }
  }

  virtual greenspun::EvalResultT<UpdateResult> updateBySlice(VPackSlice s) override {
    return updateByMessageSlice(s);
  }

  virtual void setBySlice(VPackSlice s) {
    if constexpr (std::is_same_v<T, bool>) {
      this->set(s.getBool());
    } else if constexpr (std::is_arithmetic_v<T>) {
      this->set(s.getNumericValue<T>());
    } else {
      std::abort();
    }
  }

  // Get the value into a builder. The formatting of this result
  // is entirely up to the accumulators implementation
  virtual void getValueIntoBuilder(VPackBuilder& builder) {
    if constexpr (std::is_same_v<T, VPackSlice>) {
      builder.add(_value);
    } else {
      builder.add(VPackValue(_value));
    }
  }

  const void * getValuePointer() const override {
    return &_value;
  }

  void setValueFromPointer(const void * ptr) override {
    _value = *reinterpret_cast<data_type const*>(ptr);
  }

  auto updateValueFromPointer(const void * ptr) -> greenspun::EvalResultT<UpdateResult> override {
    return update(*reinterpret_cast<data_type const*>(ptr));
  }

 protected:
  data_type _value;
  std::string _sender;
};

std::unique_ptr<AccumulatorBase> instantiateAccumulator(AccumulatorOptions const& options,
                                                        CustomAccumulatorDefinitions const& customDefinitions);
bool isValidAccumulatorOptions(AccumulatorOptions const& options);

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
