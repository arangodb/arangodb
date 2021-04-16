////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

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
  virtual std::string type() const = 0;
  virtual void toPrometheus(std::string& result, std::string const& globals, std::string const& alternativeName = std::string()) const = 0;
  void header(std::string& result) const;
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
class Counter : public Metric {
 public:
  Counter(uint64_t const& val, std::string const& name, std::string const& help,
          std::string const& labels = std::string());
  Counter(Counter const&) = delete;
  ~Counter();
  std::ostream& print (std::ostream&) const;
  Counter& operator++();
  Counter& operator++(int);
  Counter& operator+=(uint64_t const&);
  Counter& operator=(uint64_t const&);
  virtual std::string type() const override { return "counter"; }
  void count();
  void count(uint64_t);
  uint64_t load() const;
  void store(uint64_t const&);
  void push();
  virtual void toPrometheus(std::string&, std::string const&, std::string const&) const override;
 private:
  mutable Metrics::counter_type _c;
  mutable Metrics::buffer_type _b;
};


template<typename T> class Gauge : public Metric {
 public:
  Gauge() = delete;
  Gauge(T const& val, std::string const& name, std::string const& help,
        std::string const& labels = std::string())
    : Metric(name, help, labels), _g(val) { }
  
  Gauge(Gauge const&) = delete;
  ~Gauge() = default;
  
  std::ostream& print(std::ostream&) const;
  virtual std::string type() const override { return "gauge"; }

  T fetch_add(T t, std::memory_order mo = std::memory_order_relaxed) noexcept {
    if constexpr(std::is_integral_v<T>) {
      return _g.fetch_add(t, mo);
    } else {
      T tmp(_g.load(std::memory_order_relaxed));
      do {
      } while (!_g.compare_exchange_weak(tmp, tmp + t, mo, std::memory_order_relaxed));
      return tmp;
    }
  }
  
  Gauge<T>& operator+=(T const& t) noexcept {
    fetch_add(t, std::memory_order_relaxed);
    return *this;
  }
  
  // prefix increment
  Gauge<T>& operator++() noexcept {
    fetch_add(1, std::memory_order_relaxed);
    return *this;
  }

  // postfix increment. as this would be inefficient, we simply forbid it
  Gauge<T>& operator++(int) = delete;

  T fetch_sub(T t, std::memory_order mo = std::memory_order_relaxed) noexcept {
    if constexpr(std::is_integral_v<T>) {
      return _g.fetch_sub(t, mo);
    } else {
      T tmp(_g.load(std::memory_order_relaxed));
      do {
      } while (!_g.compare_exchange_weak(tmp, tmp - t, mo, std::memory_order_relaxed));
      return tmp;
    }
  }
  
  Gauge<T>& operator-=(T const& t) noexcept {
    fetch_sub(t, std::memory_order_relaxed);
    return *this;
  }
  
  // prefix decrement
  Gauge<T>& operator--() noexcept {
    fetch_sub(1, std::memory_order_relaxed);
    return *this;
  }
  
  // postfix decrement. as this would be inefficient, we simply forbid it
  Gauge<T>& operator--(int) = delete;
  
  Gauge<T>& operator*=(T const& t) noexcept {
    T tmp(_g.load(std::memory_order_relaxed));
    do {
    } while (!_g.compare_exchange_weak(tmp, tmp * t));
    return *this;
  }
  
  Gauge<T>& operator/=(T const& t) noexcept {
    TRI_ASSERT(t != T(0));
    T tmp(_g.load(std::memory_order_relaxed));
    do {
    } while (!_g.compare_exchange_weak(tmp, tmp / t));
    return *this;
  }
  
  Gauge<T>& operator=(T const& t) noexcept {
    _g.store(t, std::memory_order_relaxed);
    return *this;
  }
  
  T load(std::memory_order mo = std::memory_order_relaxed) const noexcept { return _g.load(mo); }
  
  void toPrometheus(std::string& result, std::string const& globalLabels, std::string const& alternativeName) const override {
    result += !alternativeName.empty() ? alternativeName : name();
    result += "{";
    bool haveGlobals = false;
    if (!globalLabels.empty()) {
      result += globalLabels;
      haveGlobals = true;
    }
    if (!labels().empty()) {
      if (haveGlobals) {
        result += ",";
      }
      result += labels();
    }
    result += "} " + std::to_string(load()) + "\n";
  }
 private:
  std::atomic<T> _g;
};

std::ostream& operator<< (std::ostream&, Metrics::hist_type const&);

enum ScaleType { Fixed, Linear, Logarithmic };

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
  /**
   * @brief number of buckets
   */
  T low() const {
    return _low;
  }
  /**
   * @brief number of buckets
   */
  T high() const {
    return _high;
  }
  /**
   * @brief number of buckets
   */
  std::string const delim(size_t s) const {
    return (s < _n - 1) ? std::to_string(_delim.at(s)) : "+Inf";
  }
  /**
   * @brief number of buckets
   */
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

template <typename T>
struct fixed_scale_t : public scale_t<T> {
 public:
  using value_type = T;
  static constexpr ScaleType scale_type = Fixed;

  fixed_scale_t(T const& low, T const& high, std::initializer_list<T> const& list)
      : scale_t<T>(low, high, list.size() + 1) {
    this->_delim = list;
  }
  virtual ~fixed_scale_t() = default;
  /**
   * @brief index for val
   * @param val value
   * @return    index
   */
  size_t pos(T const& val) const {
    for (std::size_t i = 0; i < this->_delim.size(); ++i) {
      if (val <= this->_delim[i]) {
        return i;
      }
    }
    return this->_delim.size();
  }

  virtual void toVelocyPack(VPackBuilder& b) const override {
    b.add("scale-type", VPackValue("fixed"));
    scale_t<T>::toVelocyPack(b);
  }

 private:
  T _base, _div;
};

template<typename T>
struct log_scale_t : public scale_t<T> {
 public:

  using value_type = T;
  static constexpr ScaleType scale_type = Logarithmic;

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
  size_t pos(T val) const {
    return static_cast<size_t>(1+std::floor(log((val - this->_low)/_div)/_lbase));
  }
  /**
   * @brief Dump to builder
   * @param b Envelope
   */
  virtual void toVelocyPack(VPackBuilder& b) const override {
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
struct lin_scale_t : public scale_t<T> {
 public:

  using value_type = T;
  static constexpr ScaleType scale_type = Linear;

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
  size_t pos(T val) const {
    return static_cast<size_t>(std::floor((val - this->_low)/ _div));
  }

  virtual void toVelocyPack(VPackBuilder& b) const override {
    b.add("scale-type", VPackValue("linear"));
    scale_t<T>::toVelocyPack(b);
  }

 private:
  T _base, _div;
};


/**
 * @brief Histogram functionality
 */
template<typename Scale> class Histogram : public Metric {

 public:

  using value_type = typename Scale::value_type;

  Histogram() = delete;

  Histogram(Scale&& scale, std::string const& name, std::string const& help,
            std::string const& labels = std::string())
    : Metric(name, help, labels), _c(Metrics::hist_type(scale.n())), _scale(std::move(scale)),
      _lowr(std::numeric_limits<value_type>::max()),
      _highr(std::numeric_limits<value_type>::min()),
      _n(_scale.n() - 1),
      _sum(0) {}

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

  virtual std::string type() const override { return "histogram"; }
  
  Scale const& scale() const {
    return _scale;
  }

  size_t pos(value_type t) const {
    return _scale.pos(t);
  }

  void count(value_type t) {
    count(t, 1);
  }

  void count(value_type t, uint64_t n) {
    if (t < _scale.delims().front()) {
      _c[0] += n;
    } else if (t >= _scale.delims().back()) {
      _c[_n] += n;
    } else {
      _c[pos(t)] += n;
    }
    value_type tmp = _sum.load(std::memory_order_relaxed);
    do {
    } while (!_sum.compare_exchange_weak(tmp,
                                       tmp + static_cast<value_type>(n) * t,
                                       std::memory_order_relaxed,
                                       std::memory_order_relaxed));

    records(t);
  }

  value_type low() const { return _scale.low(); }
  value_type high() const { return _scale.high(); }

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

  virtual void toPrometheus(std::string& result, std::string const& globals, std::string const& alternativeName) const override {
    uint64_t sum(0);
    std::string ls;
    bool haveGlobals = false;
    if (!globals.empty()) {
      ls += globals;
      haveGlobals = true;
    }
    if (!labels().empty()) {
      if (haveGlobals) {
        ls += ",";
      }
      ls += labels();
    }

    std::string const& theName 
      = !alternativeName.empty() ? alternativeName : name();

    for (size_t i = 0; i < size(); ++i) {
      uint64_t n = load(i);
      sum += n;
      result += theName;
      result += "_bucket{";
      if (!ls.empty()) {
        result += ls + ",";
      }
      auto v = !alternativeName.empty() ? n : sum;
      result += "le=\"" + _scale.delim(i) + "\"} " + std::to_string(v) + "\n";
    }
    result += theName + "_count";
    if (!ls.empty()) {
      result += "{" + ls + "}";
    }
    result += " " + std::to_string(sum) + "\n";
    if (alternativeName.empty()) {  // This is version 2 of the API
      result += theName + "_sum";
      if (!ls.empty()) {
        result += "{" + ls + "}";
      }
      result += " " + std::to_string(_sum.load(std::memory_order_relaxed)) + "\n";
    }
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
  std::atomic<value_type> _sum;
};

std::ostream& operator<< (std::ostream&, Metrics::counter_type const&);
template<typename T>
std::ostream& operator<<(std::ostream& o, Histogram<T> const& h) {
  return h.print(o);
}

#endif
