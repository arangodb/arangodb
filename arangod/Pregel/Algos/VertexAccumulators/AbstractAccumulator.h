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

class VertexData;

template <typename T>
class Accumulator;

struct AccumulatorBase {
  AccumulatorBase(VertexData const& owner) : _owner(owner) {};
  virtual ~AccumulatorBase() = default;
  template <typename T>
  Accumulator<T>* castAccumulatorType() {
    return dynamic_cast<Accumulator<T>*>(this);
  }

  enum class UpdateResult {
    CHANGED,
    NO_CHANGE,
  };

  virtual void setBySlice(VPackSlice) = 0;
  virtual void updateBySlice(VPackSlice) = 0;
  virtual auto updateBySlice(VPackSlice, std::string_view) -> UpdateResult = 0;
  virtual void getIntoBuilder(VPackBuilder& builder) = 0;
  virtual std::string const& getSender() const = 0;

  VertexData const& _owner;
};

template <typename T>
class Accumulator : public AccumulatorBase {
 public:
  using data_type = T;

  explicit Accumulator(VertexData const& owner, AccumulatorOptions const&) : AccumulatorBase(owner) {};
  ~Accumulator() override = default;

  virtual void set(data_type v) { _value = v; };
  virtual void update(data_type v) = 0;
  virtual auto update(data_type v, std::string_view sender) -> UpdateResult = 0;

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
  void updateBySlice(VPackSlice s) override {
    // TODO proper error handling here!
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
  UpdateResult updateBySlice(VPackSlice s, std::string_view sender) override {
    if constexpr (std::is_arithmetic_v<T>) {
      return this->update(s.getNumericValue<T>(), sender);
    } else if constexpr (std::is_same_v<T, std::string>) {
      return this->update(s.copyString(), sender);
    } else if constexpr (std::is_same_v<T, bool>) {
      return this->update(s.getBool(), sender);
    } else if constexpr (std::is_same_v<T, VPackSlice>) {
      return this->update(s, sender);
    } else {
      std::abort();
    }
  }
  void getIntoBuilder(VPackBuilder& builder) override {
    if constexpr (std::is_same_v<T, VPackSlice>) {
      builder.add(_value);
    } else {
      builder.add(VPackValue(_value));
    }
  }

  data_type const& get() const { return _value; };
  std::string const& getSender() const override { return _sender; };

 protected:
  data_type _value;
  std::string _sender;
};

std::unique_ptr<AccumulatorBase> instantiateAccumulator(VertexData const& owner, ::AccumulatorOptions const& options);

}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
