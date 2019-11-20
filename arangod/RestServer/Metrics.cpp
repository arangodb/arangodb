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
  push();
}

void Counter::count(uint64_t n) {
  _b += n;
  push();
}

void Counter::push() {
  _b.push();
}

std::ostream& Counter::print(std::ostream& o) const {
  o << _c;
  return o;
}

uint64_t Counter::load() const {
  return _c.load();
}

void Counter::toPrometheus(std::string& result) const {
  result += "#TYPE " + name() + " counter\n";
  result += "#HELP " + name() + " " + help() + "\n";
  result += name() + " " + std::to_string(load()) + "\n";
}

Counter::Counter(uint64_t const& val, std::string const& name, std::string const& help) :
  Metric(name, help), _c(val), _b(_c) {}

Counter::~Counter() { _b.push(); }

