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
  
typedef std::string AggregatorID;

class IAggregator {
  IAggregator(const IAggregator&) = delete;
  IAggregator& operator=(const IAggregator&) = delete;

 public:
  IAggregator() {}
  virtual ~IAggregator() {}

  /// @brief Value from superstep S-1 supplied by the conductor
  virtual void aggregate(void const* valuePtr) = 0;

  virtual void const* getValue() const = 0;
  virtual VPackValue vpackValue() const = 0;
  virtual void parse(VPackSlice slice) = 0;


  virtual void reset(bool force) = 0;
  virtual bool isConverging() const = 0;
};
  
template<typename T>
struct NumberAggregator : public IAggregator {
  static_assert(std::is_arithmetic<T>::value, "Type must be numeric");
  
  NumberAggregator(T init, bool perm = false, bool conv = false)
  : _value(init), _initial(init), _permanent(perm), _converging(conv) {}
  
  void const* getValue() const override { return &_value; };
  VPackValue vpackValue() const override { return VPackValue(_value); };
  void parse(VPackSlice slice) override {
    T f = slice.getNumber<T>();
    aggregate((void const*)(&f));
  }
  
  void reset(bool force) override {
    if (!_permanent || force) {_value = _initial; }
  }
  
  bool isConverging() const override { return _converging; }
  
protected:
  T _value, _initial;
  bool _permanent, _converging;
};
  
template <typename T>
struct MaxAggregator : public NumberAggregator<T> {
  MaxAggregator(T init, bool perm = false) : NumberAggregator<T>(init, perm, true) {}
  void aggregate(void const* valuePtr) override {
    T other = *((T*)valuePtr);
    if (other > this->_value) this->_value = other;
  };
};

template <typename T>
struct MinAggregator : public NumberAggregator<T> {
  MinAggregator(T init, bool perm = false) : NumberAggregator<T>(init, perm, true) {}
  void aggregate(void const* valuePtr) override {
    T other = *((T*)valuePtr);
    if (other < this->_value) this->_value = other;
  };
};

template <typename T>
struct SumAggregator : public NumberAggregator<T> {
  SumAggregator(T init, bool perm = false) : NumberAggregator<T>(init, perm, true) {}
  
  void aggregate(void const* valuePtr) override { this->_value += *((T*)valuePtr); };
  void parse(VPackSlice slice) override { this->_value += slice.getNumber<T>(); }
};

template <typename T>
struct ValueAggregator : public NumberAggregator<T> {
  ValueAggregator(T val, bool perm = false) : NumberAggregator<T>(val, perm, true) {}

  void aggregate(void const* valuePtr) override { this->_value = *((T*)valuePtr); };
  void parse(VPackSlice slice) override {this-> _value = slice.getNumber<T>(); }
};

/// always initializes to true.
struct BoolOrAggregator : public IAggregator {
  
  BoolOrAggregator(bool perm = false) : _permanent(perm) {}
  
  void const* getValue() const override { return &_value; };
  VPackValue vpackValue() const override { return VPackValue(_value); };
  
  void aggregate(void const* valuePtr) override { _value = _value || *((bool*)valuePtr); };
  void parse(VPackSlice slice) override {
    _value = _value || slice.getBool();
  }
  
  void reset(bool force) override {
    if (!_permanent || force) {_value = false; }
  }
  
  bool isConverging() const override { return false; }
  
protected:
  bool _value = false, _permanent;
};

}
}
#endif
