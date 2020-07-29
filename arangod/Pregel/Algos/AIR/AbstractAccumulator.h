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
namespace accumulators {
class VertexData;

template <typename T>
class Accumulator;

struct AccumulatorBase {
  AccumulatorBase(VertexData const& owner) : _owner(owner){};
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
  virtual void clear() = 0;

  // Set the accumulator's value by slice; it is the
  // responsibility of the particular accumulator
  // implementation to interpret the slice
  virtual void setBySlice(VPackSlice) = 0;

  // Update the accumulator's value by a message slice
  // The accumulator can assume that the slice was builder
  // by getIntoMessageSlice
  virtual auto updateByMessageSlice(VPackSlice) -> UpdateResult = 0;

  // Write all information that is needed to send an update
  // The slice built here must be interpretable by
  // updateByMessageSlice
  virtual void getUpdateMessageIntoBuilder(VPackBuilder& builder) = 0;

  // Get the value into a builder. The formatting of this result
  // is entirely up to the accumulators implementation
  virtual void getValueIntoBuilder(VPackBuilder& builder) = 0;

  VertexData const& _owner;
};

template <typename T>
class Accumulator : public AccumulatorBase {
 public:
  using data_type = T;

  explicit Accumulator(VertexData const& owner, AccumulatorOptions const&)
      : AccumulatorBase(owner){};
  ~Accumulator() override = default;

  // Needed to implement set by slice and clear
  virtual void set(data_type&& v) { _value = v; };

  virtual void clear() override { this->set(T{}); };

  // Needed to implement updates by slice
  virtual UpdateResult update(data_type v) = 0;

  void setBySlice(VPackSlice s) override {
    // TODO proper error handling here!
    if constexpr (std::is_same_v<T, bool>) {
      this->set(s.getBool());
    } else if constexpr (std::is_arithmetic_v<T>) {
      this->set(s.getNumericValue<T>());
    } else {
      std::abort();
    }
  }

  UpdateResult updateByMessageSlice(VPackSlice s) override {
    if constexpr (std::is_same_v<T, bool>) {
      return this->update(s.getBool());
    } else if constexpr (std::is_arithmetic_v<T>) {
      return this->update(s.getNumericValue<T>());
    } else if constexpr (std::is_same_v<T, std::string>) {
      return this->update(s.copyString());
    } else if constexpr (std::is_same_v<T, VPackSlice>) {
      return this->update(s);
    } else {
      std::abort();
    }
  }

  void getUpdateMessageIntoBuilder(VPackBuilder& builder) override {
    getValueIntoBuilder(builder);
  }

  void getValueIntoBuilder(VPackBuilder& builder) override {
    if constexpr (std::is_same_v<T, VPackSlice>) {
      builder.add(_value);
    } else {
      builder.add(VPackValue(_value));
    }
  }

 protected:
  data_type _value;
  std::string _sender;
};

std::unique_ptr<AccumulatorBase> instantiateAccumulator(VertexData const& owner,
                                                        AccumulatorOptions const& options);

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
