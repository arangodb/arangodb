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

#ifndef ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_ACCUMULATORS_H
#define ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_ACCUMULATORS_H 1
#include <Pregel/Algos/AIR/Greenspun/Interpreter.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <iostream>
#include "AbstractAccumulator.h"

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

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
    this->set(std::move(v));
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
  auto getValueIntoBuilder(VPackBuilder& builder) -> greenspun::EvalResult override {
    VPackArrayBuilder array(&builder);
    for (auto&& p : _list) {
      builder.add(VPackValue(p));
    }
    return {};
  }
  auto setBySlice(VPackSlice s) -> greenspun::EvalResult override {
    _list.clear();
    if (s.isArray()) {
      for (auto&& p : velocypack::ArrayIterator(s)) {
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
  auto getValueIntoBuilder(VPackBuilder& builder) -> greenspun::EvalResult override {
    VPackArrayBuilder array(&builder);
    for (auto&& p : _list) {
      builder.add(p.slice());
    }
    return {};
  }
  auto setBySlice(VPackSlice s) -> greenspun::EvalResult override {
    _list.clear();
    if (s.isArray()) {
      for (auto&& p : velocypack::ArrayIterator(s)) {
        _list.emplace_back().add(p);
      }
      return {};
    } else {
      auto msg = std::string{"setBySlice expected an array, but got "} + s.typeName();
      return greenspun::EvalError{msg};
    }
  }
  auto updateBySlice(VPackSlice v)
      -> greenspun::EvalResultT<AccumulatorBase::UpdateResult> override {
    return update(v);
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

  auto setBySlice(VPackSlice v) -> greenspun::EvalResult override;
  auto updateBySlice(VPackSlice s) -> greenspun::EvalResultT<UpdateResult> override;
  auto updateByMessage(MessageData const& msg) -> greenspun::EvalResultT<UpdateResult> override;

  auto getValueIntoBuilder(VPackBuilder& result) -> greenspun::EvalResult override;
  greenspun::EvalResult finalizeIntoBuilder(VPackBuilder& result) override;

  auto clear() -> greenspun::EvalResult override;

 private:
  void SetupFunctions();

  auto AIR_InputSender(greenspun::Machine& ctx, VPackSlice slice, VPackBuilder& result)
      -> greenspun::EvalResult;
  auto AIR_InputValue(greenspun::Machine& ctx, VPackSlice slice, VPackBuilder& result)
      -> greenspun::EvalResult;
  auto AIR_CurrentValue(greenspun::Machine& ctx, VPackSlice slice, VPackBuilder& result)
      -> greenspun::EvalResult;
  auto AIR_GetCurrentValue(greenspun::Machine& ctx, VPackSlice slice,
                           VPackBuilder& result) -> greenspun::EvalResult;
  auto AIR_ThisSet(greenspun::Machine& ctx, VPackSlice slice, VPackBuilder& result)
      -> greenspun::EvalResult;
  auto AIR_Parameters(greenspun::Machine& ctx, VPackSlice slice, VPackBuilder& result)
      -> greenspun::EvalResult;

  VPackSlice inputSlice = VPackSlice::noneSlice();
  std::string const* inputSender = nullptr;

  VPackBuilder _buffer;
  VPackBuilder _parameters;
  CustomAccumulatorDefinition _definition;
  greenspun::Machine _machine;
};

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb

#endif
