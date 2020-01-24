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
#include <velocypack/StringRef.h>

#ifndef ARANGODB_PREGEL_AGGREGATOR_H
#define ARANGODB_PREGEL_AGGREGATOR_H 1

namespace arangodb {
namespace pregel {

typedef std::string AggregatorID;

///
class IAggregator {
  IAggregator(const IAggregator&) = delete;
  IAggregator& operator=(const IAggregator&) = delete;

 public:
  IAggregator() {}
  virtual ~IAggregator() = default;

  /// @brief Used when updating aggregator value locally
  virtual void aggregate(void const* valuePtr) = 0;
  /// @brief Used when updating aggregator value from remote
  virtual void parseAggregate(arangodb::velocypack::Slice const& slice) = 0;

  virtual void const* getAggregatedValue() const = 0;
  /// @brief Value from superstep S-1 supplied by the conductor
  virtual void setAggregatedValue(arangodb::velocypack::Slice const& slice) = 0;

  virtual void serialize(std::string const& key,
                         arangodb::velocypack::Builder& builder) const = 0;

  virtual void reset() = 0;
  virtual bool isConverging() const = 0;
};

template <typename T>
struct NumberAggregator : public IAggregator {
  static_assert(std::is_arithmetic<T>::value, "Type must be numeric");

  NumberAggregator(T neutral, bool perm = false, bool conv = false)
      : _value(neutral), _neutral(neutral), _permanent(perm), _converging(conv) {}

  void parseAggregate(arangodb::velocypack::Slice const& slice) override {
    T f = slice.getNumber<T>();
    aggregate((void const*)(&f));
  };

  void const* getAggregatedValue() const override { return &_value; };

  void setAggregatedValue(arangodb::velocypack::Slice const& slice) override {
    _value = slice.getNumber<T>();
  }

  void serialize(std::string const& key,
                 arangodb::velocypack::Builder& builder) const override {
    builder.add(key, arangodb::velocypack::Value(_value));
  };

  void reset() override {
    if (!_permanent) {
      _value = _neutral;
    }
  }

  bool isConverging() const override { return _converging; }

 protected:
  T _value, _neutral;
  bool _permanent, _converging;
};

template <typename T>
struct MaxAggregator : public NumberAggregator<T> {
  explicit MaxAggregator(T init, bool perm = false)
      : NumberAggregator<T>(init, perm, true) {}
  void aggregate(void const* valuePtr) override {
    T other = *((T*)valuePtr);
    if (other > this->_value) this->_value = other;
  };
};

template <typename T>
struct MinAggregator : public NumberAggregator<T> {
  explicit MinAggregator(T init, bool perm = false)
      : NumberAggregator<T>(init, perm, true) {}
  void aggregate(void const* valuePtr) override {
    T other = *((T*)valuePtr);
    if (other < this->_value) this->_value = other;
  };
};

template <typename T>
struct SumAggregator : public NumberAggregator<T> {
  explicit SumAggregator(T init, bool perm = false)
      : NumberAggregator<T>(init, perm, true) {}

  void aggregate(void const* valuePtr) override {
    this->_value += *((T*)valuePtr);
  };
  void parseAggregate(arangodb::velocypack::Slice const& slice) override {
    this->_value += slice.getNumber<T>();
  }
};

/// Aggregator that stores a value that is overwritten once another value is
/// aggregated.
/// This aggregator is useful for one-to-many communication from
/// master.compute() or from a special vertex.
/// In case multiple vertices write to this aggregator, its behavior is
/// non-deterministic.
template <typename T>
struct OverwriteAggregator : public NumberAggregator<T> {
  explicit OverwriteAggregator(T val, bool perm = false)
      : NumberAggregator<T>(val, perm, true) {}

  void aggregate(void const* valuePtr) override {
    this->_value = *((T*)valuePtr);
  };
  void parseAggregate(arangodb::velocypack::Slice const& slice) override {
    this->_value = slice.getNumber<T>();
  }
};

/// always initializes to true.
struct BoolOrAggregator : public IAggregator {
  BoolOrAggregator(bool perm = false) : _permanent(perm) {}

  void aggregate(void const* valuePtr) override {
    _value = _value || *((bool*)valuePtr);
  };

  void parseAggregate(arangodb::velocypack::Slice const& slice) override {
    _value = _value || slice.getBool();
  }

  void const* getAggregatedValue() const override { return &_value; };
  void setAggregatedValue(arangodb::velocypack::Slice const& slice) override {
    _value = slice.getBool();
  }

  void serialize( std::string const& key, arangodb::velocypack::Builder& builder) const override {
    builder.add(key, arangodb::velocypack::Value(_value));
  };

  void reset() override {
    if (!_permanent) {
      _value = false;
    }
  }

  bool isConverging() const override { return false; }

 protected:
  bool _value = false, _permanent;
};
}  // namespace pregel
}  // namespace arangodb
#endif
