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

#include "AbstractAccumulator.h"

namespace arangodb {
namespace pregel {
namespace algos {

template<typename T>
class MinAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  void update(T v) override {
    this->_value = std::min(v, this->_value);
  }
};

template<typename T>
class MaxAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  void update(T v) override {
    this->_value = std::max(v, this->_value);
  }
};

template<typename T>
class SumAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  void update(T v) override {
    this->_value += v;
  }
};

template<typename T>
class AndAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  void update(T v) override {
    this->_value &= v;
  }
};

template<typename T>
class OrAccumulator : public Accumulator<T> {
 public:
  using Accumulator<T>::Accumulator;
  void update(T v) override {
    this->_value |= v;
  }
};

}  // namespace algos
}  // namespace pregel
}  // namespace arangodb

#endif
