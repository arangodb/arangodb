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

#ifndef ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_ACCUMULATORS_H
#define ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_ACCUMULATORS_H 1
#include <Greenspun/Interpreter.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <iostream>
#include "AbstractAccumulator.h"

namespace arangodb::pregel::algos::accumulators {

template <typename T>
class MinAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  auto update(T v) -> greenspun::EvalResultT<AccumulatorBase::UpdateResult> override {
    if (v < this->_value) {
      this->_value = v;
      return AccumulatorBase::UpdateResult::CHANGED;
    }
    return AccumulatorBase::UpdateResult::NO_CHANGE;
  }
  auto clear() -> greenspun::EvalResult override {
    return this->set(std::numeric_limits<T>::max());
  }
};

template <typename T>
class MaxAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  auto update(T v) -> greenspun::EvalResultT<AccumulatorBase::UpdateResult> override {
    if (v > this->_value) {
      this->_value = v;
      return AccumulatorBase::UpdateResult::CHANGED;
    }
    return AccumulatorBase::UpdateResult::NO_CHANGE;
  }
  auto clear() -> greenspun::EvalResult override {
    return this->set(std::numeric_limits<T>::min());
  }
};

template <typename T>
class SumAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  auto update(T v) -> greenspun::EvalResultT<AccumulatorBase::UpdateResult> override {
    auto old = this->_value;
    this->_value += v;
    return old == this->_value ? AccumulatorBase::UpdateResult::NO_CHANGE
                               : AccumulatorBase::UpdateResult::CHANGED;
  }
  auto clear() -> greenspun::EvalResult override { return this->set(T{0}); }
};

template <typename T>
class AndAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  auto update(T v) -> greenspun::EvalResultT<AccumulatorBase::UpdateResult> override {
    auto old = this->_value;
    this->_value &= v;
    return old == this->_value ? AccumulatorBase::UpdateResult::NO_CHANGE
                               : AccumulatorBase::UpdateResult::CHANGED;
  }
  auto clear() -> greenspun::EvalResult override {
    this->set(true);
    return {};
  }
};

template <typename T>
class OrAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  auto update(T v) -> greenspun::EvalResultT<AccumulatorBase::UpdateResult> override {
    auto old = this->_value;
    this->_value |= v;
    return old == this->_value ? AccumulatorBase::UpdateResult::NO_CHANGE
                               : AccumulatorBase::UpdateResult::CHANGED;
  }
  auto clear() -> greenspun::EvalResult override {
    this->set(false);
    return {};
  }
};

template <typename T>
class StoreAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  auto update(T v) -> greenspun::EvalResultT<AccumulatorBase::UpdateResult> override {
    this->_value = std::move(v);
    return AccumulatorBase::UpdateResult::CHANGED;
  }
};

template <>
class StoreAccumulator<VPackSlice> : public Accumulator<VPackSlice> {
 public:
  using Accumulator<VPackSlice>::Accumulator;
  auto set(VPackSlice v) -> greenspun::EvalResult override {
    _buffer.clear();
    _buffer.add(v);
    _value = _buffer.slice();
    return {};
  }
  auto update(VPackSlice v) -> greenspun::EvalResultT<UpdateResult> override {
    this->set(v);
    return UpdateResult::CHANGED;
  }
  auto clear() -> greenspun::EvalResult override {
    _buffer.clear();
    _value = _buffer.slice();
    return {};
  }

 private:
  VPackBuilder _buffer;
};

template <typename T>
class ListAccumulator : public Accumulator<T> {
  using Accumulator<T>::Accumulator;
  auto update(T v) -> greenspun::EvalResultT<AccumulatorBase::UpdateResult> override {
    _list.emplace_back(std::move(v));
    return AccumulatorBase::UpdateResult::CHANGED;
  }
  auto clear() -> greenspun::EvalResult override {
    _list.clear();
    return {};
  }
  auto getIntoBuilder(VPackBuilder& builder) -> greenspun::EvalResult override {
    VPackArrayBuilder array(&builder);
    for (auto const& p : _list) {
      builder.add(VPackValue(p));
    }
    return {};
  }
  auto setBySlice(VPackSlice s) -> greenspun::EvalResult override {
    _list.clear();
    if (s.isArray()) {
      _list.reserve(s.length());
      for (auto const& p : velocypack::ArrayIterator(s)) {
        if constexpr (std::is_same_v<T, std::string>) {
          _list.emplace_back(p.stringView());
        } else {
          return greenspun::EvalError{"not implemented"};
        }
      }
      return {};
    } else {
      auto msg = std::string{"setBySlice expected an array, got "} + s.typeName();
      return greenspun::EvalError{msg};
    }
  }

 private:
  std::vector<T> _list;
};

template <>
class ListAccumulator<VPackSlice> : public Accumulator<VPackSlice> {
  using Accumulator<VPackSlice>::Accumulator;
  greenspun::EvalResultT<AccumulatorBase::UpdateResult> update(VPackSlice v) override {
    _list.emplace_back().add(v);
    return AccumulatorBase::UpdateResult::CHANGED;
  }
  auto clear() -> greenspun::EvalResult override {
    _list.clear();
    return {};
  }
  auto getIntoBuilder(VPackBuilder& builder) -> greenspun::EvalResult override {
    VPackArrayBuilder array(&builder);
    for (auto const& p : _list) {
      builder.add(p.slice());
    }
    return {};
  }
  auto setBySlice(VPackSlice s) -> greenspun::EvalResult override {
    _list.clear();
    if (s.isArray()) {
      _list.reserve(s.length());
      for (auto const& p : velocypack::ArrayIterator(s)) {
        _list.emplace_back().add(p);
      }
      return {};
    } else {
      auto msg = std::string{"setBySlice expected an array, but got "} + s.typeName();
      return greenspun::EvalError{msg};
    }
  }

 private:
  std::vector<VPackBuilder> _list;
};

template <typename>
struct CustomAccumulator;
template <>
struct CustomAccumulator<VPackSlice> : Accumulator<VPackSlice> {
 public:
  CustomAccumulator(AccumulatorOptions const& options,
                    CustomAccumulatorDefinitions const& defs);

  CustomAccumulator(CustomAccumulator&&) = delete;
  CustomAccumulator(CustomAccumulator const&) = delete;
  CustomAccumulator& operator=(CustomAccumulator&&) = delete;
  CustomAccumulator& operator=(CustomAccumulator const&) = delete;

  ~CustomAccumulator() override;

  virtual auto clear() -> greenspun::EvalResult override;
  virtual auto setBySlice(VPackSlice v) -> greenspun::EvalResult override;
  virtual auto getIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult override;

  virtual auto updateByMessageSlice(VPackSlice msg) -> greenspun::EvalResultT<UpdateResult> override;
  virtual auto updateByMessage(MessageData const& msg) -> greenspun::EvalResultT<UpdateResult> override;
  
  virtual auto setStateBySlice(VPackSlice s) -> greenspun::EvalResult override;
  virtual auto getStateIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult override;
  virtual auto getStateUpdateIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult override;
  virtual auto aggregateStateBySlice(VPackSlice s) -> greenspun::EvalResult override;

  greenspun::EvalResult finalizeIntoBuilder(VPackBuilder& result) override;


 private:
  void SetupFunctions();

  auto AIR_InputSender(greenspun::Machine& ctx, VPackSlice slice, VPackBuilder& result)
      -> greenspun::EvalResult;
  auto AIR_InputValue(greenspun::Machine& ctx, VPackSlice slice, VPackBuilder& result)
      -> greenspun::EvalResult;
  auto AIR_InputState(greenspun::Machine& ctx, VPackSlice slice, VPackBuilder& result)
      -> greenspun::EvalResult;
  auto AIR_CurrentValue(greenspun::Machine& ctx, VPackSlice slice, VPackBuilder& result)
      -> greenspun::EvalResult;
  auto AIR_GetCurrentValue(greenspun::Machine& ctx, VPackSlice slice,
                           VPackBuilder& result) -> greenspun::EvalResult;
  auto AIR_ThisSet(greenspun::Machine& ctx, VPackSlice slice, VPackBuilder& result)
      -> greenspun::EvalResult;
  auto AIR_Parameters(greenspun::Machine& ctx, VPackSlice slice, VPackBuilder& result)
      -> greenspun::EvalResult;

  VPackSlice _inputSlice = VPackSlice::noneSlice();
  VPackSlice _inputSender = VPackSlice::noneSlice();
  VPackSlice _inputState = VPackSlice::noneSlice();

  VPackBuilder _buffer;
  VPackBuilder _parameters;
  CustomAccumulatorDefinition _definition;
  greenspun::Machine _machine;
};

}  // namespace arangodb

#endif
