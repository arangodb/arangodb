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

#include "Metrics.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Basics/debugging.h"
#include <type_traits>

using namespace arangodb;

Metrics::Metric::Metric(uint64_t val) {
  _var.emplace<0>(val);
}

Metrics::Metric::Metric(uint64_t buckets, uint64_t low, uint64_t high) {
  _var.emplace<1>(buckets);
}

Metrics::Metric::~Metric() {}

void Metrics::clear() {
  _registry.clear();
}

std::ostream& operator<< (std::ostream& o, Metrics::counter_type const& s) {
  o << s.load();
  return o;
}

std::ostream& operator<< (std::ostream& o, Counter const& s) {
  o << s.load();
  return o;
}

std::ostream& operator<< (std::ostream& o, Metrics::hist_type const& v) {
  o << "[";
  for (size_t i = 0; i < v.size(); ++i) {
    if (i > 0) { o << ", "; }
    o << v.load(i);
  }
  o << "]";
  return o;
}

Counter& Counter::operator++() {
  count();
  return *this;
}

Counter& Counter::operator++(int n) {
  count(n);
  return *this;
}

void Counter::count() {
  ++_b;
}

void Counter::count(uint64_t n) {
  _b += n;
}

std::ostream& Counter::print(std::ostream& o) const {
  _b.push();
  o << _c;
  return o;
}

uint64_t Counter::load() const {
  return _c.load();
}
Counter::Counter(Metrics::counter_type& c) : _c(c), _b(c) {}
Counter::~Counter() { _b.push(); }

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

std::ostream& operator<<(std::ostream& o, Metrics::var_type const& v) {
  std::visit(overloaded {
      [&o](Metrics::counter_type const& arg) { o << arg; },
      [&o](Metrics::hist_type const& arg) { o << arg; },
    }, v);
  return o;
}

std::ostream& operator<< (std::ostream& o, Metrics::Metric const& c) {
  return c.print(o);
}

Metrics::~Metrics() {

  for (auto const& i : _registry) {
    std::cerr << i.first << ": " << i.second->_var << std::endl;
  }

}

Metrics::counter_type& Metrics::counter(
  std::string const& name, uint64_t val) {
  auto it = _registry.find(name);
  if (it != _registry.end()) {
    return std::get<Metrics::counter_type>(it->second->_var);
  }
  auto ptr = std::make_shared<Metrics::Metric>(val);
  _registry.emplace(name, ptr);
  return std::get<Metrics::counter_type>(ptr->_var);
}

Metrics::hist_type& Metrics::histogram(
  std::string const& name, uint64_t buckets) {
  auto it = _registry.find(name);
  if (it != _registry.end()) {
    return std::get<Metrics::hist_type>(it->second->_var);
  }
  auto ptr = std::make_shared<Metrics::Metric>(buckets, 0, 0);
  _registry.emplace(name, ptr);
  return std::get<Metrics::hist_type>(ptr->_var);
}

Counter Metrics::getCounter(std::string const& name) {
  std::lock_guard<std::mutex> guard(_lock);
  return Counter(counter(name));
}
Counter Metrics::registerCounter(std::string const& name, uint64_t init) {
  std::lock_guard<std::mutex> guard(_lock);
  return Counter(counter(name, init));
}

Metrics::counter_type& Metrics::counter(std::string const& name) {
  auto it = _registry.find(name);
  LOG_TOPIC("4ca33", ERR, Logger::FIXME) << "No such counter " << name;
  TRI_ASSERT(it != _registry.end());
  return std::get<Metrics::counter_type>(it->second->_var);
}

Metrics::hist_type& Metrics::histogram(std::string const& name) {
  auto it = _registry.find(name);
  LOG_TOPIC("3ca34", ERR, Logger::FIXME) << "No such histogram " << name;
  TRI_ASSERT(it != _registry.end());
  return std::get<Metrics::hist_type>(it->second->_var);
}

template<class T> struct always_false : std::false_type {};

std::ostream& Metrics::Metric::print(std::ostream& o) const {
  std::visit(overloaded {
      [&o](Metrics::counter_type const& arg) { o << "counter with value " << arg; },
      [&o](Metrics::hist_type const& arg) { o << "histogram with value " << arg; },
    }, _var);
  return o;
}

void Metrics::Metric::toBuilder(VPackBuilder& b) {
  std::visit(overloaded {
      [&b](Metrics::counter_type const& arg) { b.add(VPackValue(arg.load())); },
      [&b](Metrics::hist_type const& arg) {
        VPackArrayBuilder a(&b);
        for (size_t i = 0; i < arg.size(); ++i) {
          b.add(VPackValue(arg.load(i)));
        }
      },
    }, _var);
}
