////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include <cstdint>
#include <string>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#ifndef ARANGODB_PREGEL_AGGREGATOR_H
#define ARANGODB_PREGEL_AGGREGATOR_H 1

namespace arangodb {
namespace pregel {

class Aggregator {
 public:
  Aggregator(const Aggregator&) = delete;
  Aggregator& operator=(const Aggregator&) = delete;

  /// @brief Value from superstep S-1 supplied by the conductor
  virtual void const* getAggregatedValue() const = 0;
  /// @brief Value from superstep S-1 supplied by the conductor
  virtual void setAggregatedValue(VPackSlice otherValue) = 0;

  /// @brief Value from superstep S calculated by the worker
  virtual void const* getValue() const = 0;
  virtual void aggregateValue(void const* otherValue) = 0;
  virtual void aggregateValue(VPackSlice otherValue) = 0;
  virtual void serializeValue(VPackBuilder& b) = 0;

  virtual void reset() = 0;
  std::string const& name() { return _name; }

 protected:
  Aggregator(std::string const& name) : _name(name) {}
  const std::string _name;
};

class FloatMaxAggregator : public Aggregator {
 public:
  FloatMaxAggregator(std::string const& name, float init)
      : Aggregator(name), _aggregated(init), _value(init), _initial(init) {}

  void const* getAggregatedValue() const override { return &_aggregated; };
  void setAggregatedValue(VPackSlice value) override {
    _aggregated = (float)value.getDouble();
  };

  virtual void const* getValue() const override { return &_value; }
  void aggregateValue(void const* otherValue) override {
    float other = *((float*)otherValue);
    if (other > _value) _value = other;
  };
  void aggregateValue(VPackSlice otherValue) override {
    float v = (float)otherValue.getDouble();
    this->aggregateValue(&v);
  };

  void serializeValue(VPackBuilder& b) override {
    b.add(_name, VPackValue(_value));
  };
  void reset() override { _value = _initial; }

 private:
  float _aggregated, _value, _initial;
};
}
}
#endif
