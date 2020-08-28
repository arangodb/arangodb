////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_SERVER_METRICS_H
#define ARANGODB_REST_SERVER_METRICS_H 1

#include <atomic>
#include <map>
#include <iostream>
#include <string>
#include <memory>
#include <variant>
#include <vector>
#include <unordered_map>
#include <cassert>
#include <cmath>
#include <limits>

#include "Basics/StringUtils.h"
#include "Basics/debugging.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

#include "counter.h"

class Metric {
 public:
  Metric(std::string const& name, std::string const& help, std::string const& labels);
  virtual ~Metric();
  std::string const& help() const;
  std::string const& name() const;
  std::string const& labels() const;
  
  virtual void toPrometheus(std::string& result) const = 0;

  virtual void toBuilder(arangodb::velocypack::Builder& result) const {
    result.add("name", arangodb::velocypack::Value(name()));
    result.add("help", arangodb::velocypack::Value(help()));
    result.add(arangodb::velocypack::Value("labels"));
    result.openArray();
    for (auto const& pair: arangodb::basics::StringUtils::split(labels(), ',')) {
      result.openObject();
      auto keyValuePair = arangodb::basics::StringUtils::split(pair, '=');
      keyValuePair[0] = arangodb::basics::StringUtils::replace(keyValuePair[0], "\"", "");
      if (keyValuePair.size() > 1) {
        keyValuePair[1] = arangodb::basics::StringUtils::replace(keyValuePair[1], "\"", "");
        result.add(keyValuePair[0], arangodb::velocypack::Value(keyValuePair[1]));
      } else {
        result.add(keyValuePair[0], arangodb::velocypack::Value(""));
      }
      result.close();
    }
    result.close();
  }

 protected:
  std::string const _name;
  std::string const _help;
  std::string const _labels;
};

struct Metrics {
  using counter_type = gcl::counter::simplex<uint64_t, gcl::counter::atomicity::full>;
  using hist_type = gcl::counter::simplex_array<uint64_t, gcl::counter::atomicity::full>;
  using buffer_type = gcl::counter::buffer<uint64_t, gcl::counter::atomicity::full, gcl::counter::atomicity::full>;
};


/**
 * @brief Counter functionality
 */
class Counter final : public Metric {
 public:
  Counter(uint64_t const& val, std::string const& name, std::string const& help,
          std::string const& labels = std::string());
  Counter(Counter const&) = delete;
  ~Counter();
  std::ostream& print (std::ostream&) const;
  Counter& operator++();
  Counter& operator++(int);
  Counter& operator+=(uint64_t);
  Counter& operator=(uint64_t);
  void count();
  void count(uint64_t);
  uint64_t load() const;
  void store(uint64_t);
  void push();
  void toPrometheus(std::string&) const override;
  void toBuilder(arangodb::velocypack::Builder& result) const override;
 private:
  mutable Metrics::counter_type _c;
  mutable Metrics::buffer_type _b;
};


template<typename T> class Gauge final : public Metric {
 public:
  Gauge() = delete;
  Gauge(T const& val, std::string const& name, std::string const& help,
        std::string const& labels = std::string())
    : Metric(name, help, labels) {
    _g.store(val);
  }
  Gauge(Gauge const&) = delete;
  ~Gauge() = default;
  std::ostream& print (std::ostream&) const;
  Gauge<T>& operator+=(T const& t) {
    _g.store(_g + t);
    return *this;
  }
  Gauge<T>& operator-=(T const& t) {
    _g.store(_g - t);
    return *this;
  }
  Gauge<T>& operator*=(T const& t) {
    _g.store(_g * t);
    return *this;
  }
  Gauge<T>& operator/=(T const& t) {
    TRI_ASSERT(t != T(0));
    _g.store(_g / t);
    return *this;
  }
  Gauge<T>& operator=(T const& t) {
    _g.store(t);
    return *this;
  }
  T load() const {
    return _g.load();
  }

  void toPrometheus(std::string& result) const override {
    result += "\n#TYPE " + name() + " gauge\n";
    result += "#HELP " + name() + " " + help() + "\n";
    result += name() + "{" + labels() + "} " + std::to_string(load()) + "\n";
  }

  void toBuilder(arangodb::velocypack::Builder& result) const override {
    result.openObject();
    Metric::toBuilder(result);
    result.add("type", arangodb::velocypack::Value("gauge"));
    result.add("value", arangodb::velocypack::Value(load()));
    result.close();
  }
 private:
  std::atomic<T> _g;
};

std::ostream& operator<< (std::ostream&, Metrics::hist_type const&);

enum ScaleType {LINEAR, LOGARITHMIC};

template<typename T>
struct scale_t {
 public:

  using value_type = T;

  scale_t(T const& low, T const& high, size_t n) :
    _low(low), _high(high), _n(n) {
    TRI_ASSERT(n > 1);
    _delim.resize(n - 1);
  }
  virtual ~scale_t() = default;
  /**
   * @brief number of buckets
   */
  size_t n() const {
    return _n;
  }
  
  T low() const {
    return _low;
  }
  
  T high() const {
    return _high;
  }
  
  std::string const delim(size_t s) const {
    return (s < _n - 1) ? std::to_string(_delim.at(s)) : "+Inf";
  }
  
  std::vector<T> const& delims() const {
    return _delim;
  }

  /**
   * @brief dump to builder
   */
  virtual void toVelocyPack(VPackBuilder& b) const {
    TRI_ASSERT(b.isOpenObject());
    b.add("lower-limit", VPackValue(_low));
    b.add("upper-limit", VPackValue(_high));
    b.add("value-type", VPackValue(typeid(T).name()));
    b.add(VPackValue("range"));
    VPackArrayBuilder abb(&b);
    for (auto const& i : _delim) {
      b.add(VPackValue(i));
    }
  }
  
  /**
   * @brief dump to
   */
  std::ostream& print(std::ostream& o) const {
    VPackBuilder b;
    {
      VPackObjectBuilder bb(&b);
      this->toVelocyPack(b);
    }
    o << b.toJson();
    return o;
  }

 protected:
  T _low, _high;
  std::vector<T> _delim;
  size_t _n;
};

template<typename T>
std::ostream& operator<< (std::ostream& o, scale_t<T> const& s) {
  return s.print(o);
}

template<typename T>
struct log_scale_t final : public scale_t<T> {
 public:

  using value_type = T;
  static constexpr ScaleType scale_type = LOGARITHMIC;

  log_scale_t(T const& base, T const& low, T const& high, size_t n) :
    scale_t<T>(low, high, n), _base(base) {
    TRI_ASSERT(base > T(0));
    double nn = -1.0 * (n - 1);
    for (auto& i : this->_delim) {
      i = static_cast<T>(
        static_cast<double>(high - low) *
        std::pow(static_cast<double>(base), static_cast<double>(nn++)) + static_cast<double>(low));
    }
    _div = this->_delim.front() - low;
    TRI_ASSERT(_div > T(0));
    _lbase = log(_base);
  }
  virtual ~log_scale_t() = default;
  /**
   * @brief index for val
   * @param val value
   * @return    index
   */
  size_t pos(T const& val) const {
    return static_cast<size_t>(1 + std::floor(log((val - this->_low) / _div) / _lbase));
  }

  /**
   * @brief Dump to builder
   * @param b Envelope
   */
  virtual void toVelocyPack(VPackBuilder& b) const {
    b.add("scale-type", VPackValue("logarithmic"));
    b.add("base", VPackValue(_base));
    scale_t<T>::toVelocyPack(b);
  }
  
  /**
   * @brief Base
   * @return base
   */
  T base() const {
    return _base;
  }
 
 private:
  T _base, _div;
  double _lbase;
};

template<typename T>
struct lin_scale_t final : public scale_t<T> {
 public:

  using value_type = T;
  static constexpr ScaleType scale_type = LINEAR;

  lin_scale_t(T const& low, T const& high, size_t n) :
    scale_t<T>(low, high, n) {
    this->_delim.resize(n-1);
    _div = (high - low) / (T)n;
    if (_div <= 0) {
    }
    TRI_ASSERT(_div > 0);
    T le = low;
    for (auto& i : this->_delim) {
      le += _div;
      i = le;
    }
  }
  virtual ~lin_scale_t() = default;
  /**
   * @brief index for val
   * @param val value
   * @return    index
   */
  size_t pos(T const& val) const {
    return static_cast<size_t>(std::floor((val - this->_low) / _div));
  }
  
  virtual void toVelocyPack(VPackBuilder& b) const {
    b.add("scale-type", VPackValue("linear"));
    scale_t<T>::toVelocyPack(b);
  }
 
 private:
  T _base, _div;
};


/**
 * @brief Histogram functionality
 */
template<typename Scale> class Histogram final : public Metric {

 public:

  using value_type = typename Scale::value_type;

  Histogram() = delete;

  Histogram(Scale&& scale, std::string const& name, std::string const& help,
            std::string const& labels = std::string())
    : Metric(name, help, labels), _c(Metrics::hist_type(scale.n())), _scale(std::move(scale)),
      _lowr(std::numeric_limits<value_type>::max()),
      _highr(std::numeric_limits<value_type>::min()),
      _n(_scale.n() - 1) {}

  Histogram(Scale const& scale, std::string const& name, std::string const& help,
            std::string const& labels = std::string())
    : Metric(name, help, labels), _c(Metrics::hist_type(scale.n())), _scale(scale),
      _lowr(std::numeric_limits<value_type>::max()),
      _highr(std::numeric_limits<value_type>::min()),
      _n(_scale.n() - 1) {}

  ~Histogram() = default;

  void records(value_type const& val) {
    if (val < _lowr) {
      _lowr = val;
    } else if (val > _highr) {
      _highr = val;
    }
  }

  Scale const& scale() {
    return _scale;
  }

  size_t pos(value_type const& t) const {
    return _scale.pos(t);
  }

  void count(value_type const& t) {
    count(t, 1);
  }

  void count(value_type const& t, uint64_t n) {
    if (t < _scale.delims().front()) {
      _c[0] += n;
    } else if (t >= _scale.delims().back()) {
      _c[_n] += n;
    } else {
      _c[pos(t)] += n;
    }
    records(t);
  }

  value_type const& low() const { return _scale.low(); }
  value_type const& high() const { return _scale.high(); }

  Metrics::hist_type::value_type& operator[](size_t n) {
    return _c[n];
  }

  std::vector<uint64_t> load() const {
    std::vector<uint64_t> v(size());
    for (size_t i = 0; i < size(); ++i) {
      v[i] = load(i);
    }
    return v;
  }

  uint64_t load(size_t i) const { return _c.load(i); }

  size_t size() const { return _c.size(); }

  void toPrometheus(std::string& result) const override {
    result += "\n#TYPE " + name() + " histogram\n";
    result += "#HELP " + name() + " " + help() + "\n";
    std::string lbs = labels();
    auto const haveLabels = !lbs.empty();
    auto const separator = haveLabels && lbs.back() != ',';
    uint64_t sum(0);
    for (size_t i = 0; i < size(); ++i) {
      uint64_t n = load(i);
      sum += n;
      result += name() + "_bucket{";
      if (haveLabels) {
        result += lbs;
      }
      if (separator) {
        result += ",";
      }
      result += "le=\"" + _scale.delim(i) + "\"} " + std::to_string(n) + "\n";
    }
    result += name() + "_count{" + labels() + "} " + std::to_string(sum) + "\n";
  }

  void toBuilder(arangodb::velocypack::Builder& result) const override {
    result.openObject();
    result.add("type", arangodb::velocypack::Value("histogram"));
    Metric::toBuilder(result);
    result.add(arangodb::velocypack::Value("buckets"));
    result.openArray();
    uint64_t sum = 0;
    auto const& delimiters = _scale.delims();
    for (size_t i = 0; i < size(); ++i) {
      uint64_t n = load(i);
      sum += n;
      result.openObject();
      if (i < delimiters.size()) {
        result.add("lower", arangodb::velocypack::Value(delimiters[i]));
      } else {
        result.add("lower", arangodb::velocypack::Value("+Inf"));
      }
      result.add("count", arangodb::velocypack::Value(n));
      result.close();
    }
    result.close();
    result.add("total", arangodb::velocypack::Value(sum));
    result.close();
  }

  std::ostream& print(std::ostream& o) const {
    o << name() << " scale: " <<  _scale << " extremes: [" << _lowr << ", " << _highr << "]";
    return o;
  }

 private:
  Metrics::hist_type _c;
  Scale _scale;
  value_type _lowr, _highr;
  size_t _n;
};

std::ostream& operator<< (std::ostream&, Metrics::counter_type const&);
template<typename T>
std::ostream& operator<<(std::ostream& o, Histogram<T> const& h) {
  return h.print(o);
}

#endif
