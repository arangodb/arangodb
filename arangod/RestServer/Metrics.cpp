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

#include "Metrics.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/exitcodes.h"
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

Metric::Metric(std::string const& name, std::string const& help, std::string const& labels)
  : _name(name), _help(help), _labels(labels) {
/*  // We look up the name at runtime in the list of metrics
  // arangodb::metricsNameList. This ensures that each metric is listed
  // in that list. External scripts then parse the source code of the
  // list and ensure that all metrics have documentation snippets in
  // `Documentation/Metrics` and that these are well-formed.
  char const** p = metricsNameList;
  bool found = false;
  while (*p != nullptr) {
    if (name.compare(*p) == 0) {
      found = true;
      break;
    }
    ++p;
  }
  if (!found) {
    LOG_TOPIC("77777", FATAL, Logger::STATISTICS)
      << "Tried to register a metric whose name is not listed in "
         "`arangodb::metricsNameList`, please add the name " << name
      << " to the list in source file `arangod/RestServer/Metrics.cpp` "
         "and create a corresponding documentation snippet in "
         "`Documentation/Metrics`.";
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_FAILED);
    }*/
}

Metric::~Metric() = default;

std::string const& Metric::help() const { return _help; }
std::string const& Metric::name() const { return _name; }
std::string const& Metric::labels() const { return _labels; }

Counter& Counter::operator++() {
  count();
  return *this;
}

Counter& Counter::operator++(int n) {
  count(1);
  return *this;
}

Counter& Counter::operator+=(uint64_t const& n) {
  count(n);
  return *this;
}

Counter& Counter::operator=(uint64_t const& n) {
  store(n);
  return *this;
}

void Counter::count() {
  ++_b;
}

void Counter::count(uint64_t n) {
  _b += n;
}

std::ostream& Counter::print(std::ostream& o) const {
  o << _c;
  return o;
}

uint64_t Counter::load() const {
  _b.push();
  return _c.load();
}

void Counter::store(uint64_t const& n) {
  _c.exchange(n);
}

void Counter::toPrometheus(std::string& result) const {
  _b.push();
  result += "\n#TYPE " + name() + " counter\n";
  result += "#HELP " + name() + " " + help() + "\n";
  result += name();
  if (!labels().empty()) {
    result += "{" + labels() + "}";
  }
  result += " " + std::to_string(load()) + "\n";
}

Counter::Counter(
  uint64_t const& val, std::string const& name, std::string const& help,
  std::string const& labels) :
  Metric(name, help, labels), _c(val), _b(_c) {}

Counter::~Counter() { _b.push(); }

