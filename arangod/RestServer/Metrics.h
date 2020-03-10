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

#include "Basics/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "counter.h"

class Counter;
template<typename T> class Histogram;

class Metric {
public:
  Metric(std::string const& name, std::string const& help);
  virtual ~Metric();
  std::string const& help() const;
  std::string const& name() const;
  virtual void toPrometheus(std::string& result) const = 0;
  void header(std::string& result) const;
protected:
  std::string const _name;
  std::string const _help;
};

struct Metrics {

  enum Type {COUNTER, HISTOGRAM};

  using counter_type = gcl::counter::simplex<uint64_t, gcl::counter::atomicity::full>;
  using hist_type = gcl::counter::simplex_array<uint64_t, gcl::counter::atomicity::full>;
  using buffer_type = gcl::counter::buffer<uint64_t>;

};


/**
 * @brief Counter functionality
 */
class Counter : public Metric {
public:
  Counter(uint64_t const& val, std::string const& name, std::string const& help);
  Counter(Counter const&) = delete;
  ~Counter();
  std::ostream& print (std::ostream&) const;
  Counter& operator++();
  Counter& operator++(int);
  Counter& operator+=(uint64_t const&);
  Counter& operator=(uint64_t const&);
  void count();
  void count(uint64_t);
  uint64_t load() const;
  void store(uint64_t const&);
  void push();
  virtual void toPrometheus(std::string&) const override;
private:
  mutable Metrics::counter_type _c;
  mutable Metrics::buffer_type _b;
};


template<typename T> class Gauge : public Metric {
public:
  Gauge() = delete;
  Gauge(T const& val, std::string const& name, std::string const& help)
    : Metric(name, help) {
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
  };
  virtual void toPrometheus(std::string& result) const override {
    result += "#TYPE " + name() + " gauge\n";
    result += "#HELP " + name() + " " + help() + "\n";
    result += name() + " " + std::to_string(load()) + "\n";
  };
private:
  std::atomic<T> _g;
};

std::ostream& operator<< (std::ostream&, Metrics::hist_type const&);

/**
 * @brief Histogram functionality
 */
template<typename T> class Histogram : public Metric {

public:

  Histogram() = delete;

  Histogram (size_t const& buckets, T const& low, T const& high, std::string const& name, std::string const& help = "")
    : Metric(name, help), _c(Metrics::hist_type(buckets)), _low(low), _high(high),
      _lowr(std::numeric_limits<T>::max()), _highr(std::numeric_limits<T>::min()) {
    TRI_ASSERT(_c.size() > 0);
    _n = _c.size() - 1;
    _div = (high - low) / (double)_c.size();
    TRI_ASSERT(_div != 0);
  }

  ~Histogram() = default;

  void records(T const& t) {
    if(t < _lowr) {
      _lowr = t;
    } else if (t > _highr) {
      _highr = t;
    }
  }

  size_t pos(T const& t) const {
    return static_cast<size_t>(std::floor((t - _low)/ _div));
  }

  void count(T const& t) {
    count(t, 1);
  }

  void count(T const& t, uint64_t n) {
    if (t < _low) {
      _c[0] += n;
    } else if (t >= _high) {
      _c[_n] += n;
    } else {
      _c[pos(t)] += n;
    }
    records(t);
  }

  T const& low() const { return _low; }
  T const& high() const { return _high; }

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

  uint64_t load(size_t i) const { return _c.load(i); };

  size_t size() const { return _c.size(); }

  virtual void toPrometheus(std::string& result) const override {
    result += "#TYPE " + name() + " histogram\n";
    result += "#HELP " + name() + " " + help() + "\n";
    T le = _low;
    T sum = T(0);
    for (size_t i = 0; i < size(); ++i) {
      uint64_t n = load(i);
      sum += n;
      result += name() + "_bucket{le=\"" + std::to_string(le) + "\"} " +
        std::to_string(n) + "\n";
      le += _div;
    }
    result += name() + "_count " + std::to_string(sum) + "\n";
  }

  std::ostream& print(std::ostream& o) const {
    o << "_div: " << _div << ", _c: " << _c << ", _r: [" << _lowr << ", " << _highr << "] " << name();
    return o;
  }

private:
  Metrics::hist_type _c;
  T _low, _high, _div, _lowr, _highr;
  size_t _n;

};


std::ostream& operator<< (std::ostream&, Metrics::counter_type const&);
template<typename T>
std::ostream& operator<<(std::ostream& o, Histogram<T> const& h) {
  return h.print(o);
}


#endif
