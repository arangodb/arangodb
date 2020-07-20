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
#include <numeric>

#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"

#include "AccumulatorOptionsDeserializer.h"

namespace arangodb {
namespace pregel {
namespace algos {
// FIXME:
// maybe template/parameter with update operation?

template <typename T>
class Accumulator;

struct AccumulatorBase {
  virtual ~AccumulatorBase() = default;
  template<typename T>
  Accumulator<T>* castAccumulatorType() {
    return dynamic_cast<Accumulator<T>*>(this);
  }

  virtual void setBySlice(VPackSlice) = 0;
  virtual void updateBySlice(VPackSlice) = 0;
  virtual void getIntoBuilder(VPackBuilder& builder) = 0;
};

template <typename T>
class Accumulator : public AccumulatorBase {
 public:
  using data_type = T;

  explicit Accumulator(AccumulatorOptions const&) {};
  ~Accumulator() override = default;

  virtual void set(data_type v) {
    _value = v;
  };
  virtual void update(data_type v) = 0;

  void setBySlice(VPackSlice s) override {
    if constexpr (std::is_arithmetic_v<T>) {
      this->set(s.getNumericValue<T>());
    } else {
      std::abort();
    }
  }
  void updateBySlice(VPackSlice s) override {
    if constexpr (std::is_arithmetic_v<T>) {
      this->update(s.getNumericValue<T>());
    } else if constexpr (std::is_same_v<T, std::string>) {
      this->update(s.copyString());
    } else if constexpr (std::is_same_v<T, bool>) {
      this->update(s.getBool());
    } else if constexpr (std::is_same_v<T, VPackSlice>) {
      this->update(s);
    } else {
      std::abort();
    }
  }
  void getIntoBuilder(VPackBuilder& builder) override {
    builder.add(VPackValue(_value));
  }

  data_type const& get() const { return _value; };

 protected:
  data_type _value;
};

std::unique_ptr<AccumulatorBase> instanciateAccumulator(::AccumulatorOptions const& options);

}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
