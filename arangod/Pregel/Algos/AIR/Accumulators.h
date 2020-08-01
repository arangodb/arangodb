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
#include <iostream>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include "AbstractAccumulator.h"

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

template <typename T>
class MinAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  auto update(T v) -> AccumulatorBase::UpdateResult override {
    if (v < this->_value) {
      this->_value = v;
      return AccumulatorBase::UpdateResult::CHANGED;
    }
    return AccumulatorBase::UpdateResult::NO_CHANGE;
  }
  auto clear() -> void override {
    this->set(std::numeric_limits<T>::max());
  }
};

template <typename T>
class MaxAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  auto update(T v) -> AccumulatorBase::UpdateResult override {
    if (v > this->_value) {
      this->_value = v;
      return AccumulatorBase::UpdateResult::CHANGED;
    }
    return AccumulatorBase::UpdateResult::NO_CHANGE;
  }
  auto clear() -> void override {
    this->set(std::numeric_limits<T>::min());
  }
};

template <typename T>
class SumAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  auto update(T v) -> AccumulatorBase::UpdateResult override {
    auto old = this->_value;
    this->_value += v;
    return old == this->_value ? AccumulatorBase::UpdateResult::NO_CHANGE
                               : AccumulatorBase::UpdateResult::CHANGED;
  }
  auto clear() -> void override {
    this->set(T{0});
  }
};

template <typename T>
class AndAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  auto update(T v) -> AccumulatorBase::UpdateResult override {
    auto old = this->_value;
    this->_value &= v;
    return old == this->_value ? AccumulatorBase::UpdateResult::NO_CHANGE
                               : AccumulatorBase::UpdateResult::CHANGED;
  }
  auto clear() -> void override {
    this->set(true);
  }
};

template <typename T>
class OrAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  auto update(T v) -> AccumulatorBase::UpdateResult override {
    std::cout << "update or with " << v << std::endl;
    auto old = this->_value;
    this->_value |= v;
    std::cout << "new value is " << this->_value << std::endl;
    return old == this->_value ? AccumulatorBase::UpdateResult::NO_CHANGE
                               : AccumulatorBase::UpdateResult::CHANGED;
  }
  auto clear() -> void override {
    this->set(false);
  }
};

template <typename T>
class StoreAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  auto update(T v) -> AccumulatorBase::UpdateResult override {
    this->_value = std::move(v);
    return AccumulatorBase::UpdateResult::CHANGED;
  }
};

template <>
class StoreAccumulator<VPackSlice> : public Accumulator<VPackSlice> {
 public:
  using Accumulator<VPackSlice>::Accumulator;
  void set(VPackSlice&& v) override {
    _buffer.clear();
    _buffer.add(v);
    _value = _buffer.slice();
  }
  auto update(VPackSlice v) -> AccumulatorBase::UpdateResult override {
    this->set(std::move(v));
    return UpdateResult::CHANGED;
  }
  auto clear() -> void override {
    _buffer.clear();
  }

  void setValueFromPointer(const void * ptr) override {
    auto slice = *reinterpret_cast<VPackSlice const*>(ptr);
    _buffer.clear();
    _buffer.add(slice);
    _value = _buffer.slice();
  }

 private:
  VPackBuilder _buffer;
};

template <typename T>
class ListAccumulator : public Accumulator<T> {
  using Accumulator<T>::Accumulator;
  AccumulatorBase::UpdateResult update(T v) override {
    _list.emplace_back(std::move(v));
    return AccumulatorBase::UpdateResult::CHANGED;
  }
  auto clear() -> void override {
    _list.clear();
  }
  void getValueIntoBuilder(VPackBuilder& builder) override {
    VPackArrayBuilder array(&builder);
    for (auto&& p : _list) {
      builder.add(VPackValue(p));
    }
  }
  void setBySlice(VPackSlice s) override {
    _list.clear();
    if (s.isArray()) {
      for (auto&& p : velocypack::ArrayIterator(s)) {
        if constexpr (std::is_same_v<T, std::string>) {
          _list.emplace_back(p.stringView());
        } else {
          std::abort();
        }
      }
    }
  }

  const void* getValuePointer() const override {
    return &_list;
  }

 private:
  std::vector<T> _list;
};

template <>
class ListAccumulator<VPackSlice> : public Accumulator<VPackSlice> {
  using Accumulator<VPackSlice>::Accumulator;
  AccumulatorBase::UpdateResult update(VPackSlice v) override {
    _list.emplace_back().add(v);
    return AccumulatorBase::UpdateResult::CHANGED;
  }
  auto clear() -> void override {
    _list.clear();
  }
  void getValueIntoBuilder(VPackBuilder& builder) override {
    VPackArrayBuilder array(&builder);
    for (auto&& p : _list) {
      builder.add(p.slice());
    }
  }
  void setBySlice(VPackSlice s) override {
    _list.clear();
    if (s.isArray()) {
      for (auto&& p : velocypack::ArrayIterator(s)) {
        _list.emplace_back().add(p);
      }
    }
  }

  const void* getValuePointer() const override {
    return &_list;
  }
 private:
  std::vector<VPackBuilder> _list;
};

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb

#endif
