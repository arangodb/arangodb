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
#include "counter.h"

class Counter;
template<typename T> class Histogram;

class Metrics {

public:
  
  enum Type {COUNTER, HISTOGRAM};

  using counter_type = gcl::counter::simplex<uint64_t, gcl::counter::atomicity::full>;
  using hist_type = gcl::counter::simplex_array<uint64_t, gcl::counter::atomicity::full>;
  using buffer_type = gcl::counter::buffer<uint64_t>;
  using var_type = std::variant<counter_type,hist_type>;
  

  /**
   * @brief Metric class with variant of counter or histogram
   */
  struct Metric {
    Metric() = delete;
    Metric(Metrics const&) = delete;
    explicit Metric(Type);
    explicit Metric(uint64_t);
    Metric(uint64_t, uint64_t, uint64_t);
    ~Metric();
    std::ostream& print(std::ostream&) const;
    void toBuilder(VPackBuilder&);
    std::string _help;
    var_type _var;
  };

  using registry_type = std::unordered_map<std::string, std::shared_ptr<Metric>>;
  
  Metrics() {}
  virtual ~Metrics();
  void clear();
  
  VPackBuilder toBuilder() const;
  void toBuilder(VPackBuilder& builder) const;

private:

public:
  Counter getCounter(std::string const& name);
  Counter registerCounter(std::string const& name, uint64_t init = 0);
    
  template<typename T>
  Histogram<T> getHistogram (std::string const& name, size_t buckets, T low, T high) {
    std::lock_guard<std::mutex> guard(_lock);
    return Histogram<T>(histogram(name, buckets), low, high);
  };
  template<typename T>
  Histogram<T> registerHistogram (std::string const& name, size_t buckets, T low, T high) {
    std::lock_guard<std::mutex> guard(_lock);
    return Histogram<T>(histogram(name, buckets), low, high);
  };

private:
  counter_type& counter(std::string const& name, uint64_t val);
  hist_type& histogram(std::string const& name, uint64_t buckets);

  counter_type& counter(std::string const& name);
  hist_type& histogram(std::string const& name);

  registry_type _registry;
  mutable std::mutex _lock;
  
};


/**
 * @brief Counter functionality
 */
class Counter {
public:
  Counter(Counter const&) = delete;
  Counter(Metrics::counter_type& c);
  ~Counter();
  std::ostream& print (std::ostream&) const;
  Counter& operator++();
  Counter& operator++(int);
  void count();
  void count(uint64_t);
  uint64_t load() const;
private:
  Metrics::counter_type& _c;
  mutable Metrics::buffer_type _b; 
};

std::ostream& operator<< (std::ostream&, Metrics::hist_type&);

/**
 * @brief Histogram functionality
 */ 
template<typename T> class Histogram {

public:

  Histogram() = delete;
    
  Histogram (Metrics::hist_type& hist, T const& low, T const& high)
    : _c(hist), _low(low), _high(high),
      _lowr(std::numeric_limits<uint64_t>::max()), _highr(0) {
    assert(hist.size() > 0);
    _n = _c.size() - 1;
    _div = (double)(high - low) / (double)hist.size();
  }

  ~Histogram() {
    std::cout << *this << std::endl;
  }
    
  inline void records(T const& t) {
    if(t < _lowr) {
      _lowr = t;
    } else if (t > _highr) {
      _highr = t;
    }    
  }
      
  inline void count(T const& t) {
    if (t < _low) {
      ++_c[0];
    } else if (t >= _high) {
      ++_c[_n];
    } else {
      ++_c[std::floor(t / _div)];
    }
    records(t);
  }

  inline void count(T const& t, uint64_t n) {
    if (t < _low) {
      _c[0] += n;
    } else if (t >= _high) {
      _c[_n] += n;
    } else {
      _c[std::floor(t / _div)] += n;
    }
    records(t);
  }

  Metrics::hist_type::value_type& operator[](size_t n) {
    return _c[n];
  }

  uint64_t load(size_t i) const { return _c.load(i); };
      
  size_t size() const { return _c.size(); }

  std::ostream& print(std::ostream& o) const {
    o << "_div: " << _div << ", _c: " << _c << ", _r: [" << _lowr << ", " << _highr << "]";;    
    return o;
  }


  Metrics::hist_type& _c;
  T _low, _high, _div, _lowr, _highr;
  size_t _n;
      
};


std::ostream& operator<< (std::ostream&, Metrics::var_type&);
std::ostream& operator<< (std::ostream&, Metrics::counter_type&);
std::ostream& operator<< (std::ostream&, Metrics::hist_type&);
template<typename T>
std::ostream& operator<<(std::ostream& o, Histogram<T> const& h) {
  return h.print(o);
}


#endif
