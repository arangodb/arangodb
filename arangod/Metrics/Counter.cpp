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

#include "Metrics/Counter.h"

#include <ostream>

namespace arangodb::metrics {

Counter::Counter(uint64_t n, std::string_view name, std::string_view help, std::string_view labels)
    : Metric{name, help, labels}, _c{n}, _b{_c} {}

Counter::~Counter() { _b.push(); }

std::string_view Counter::type() const noexcept { return "counter"; }

void Counter::toPrometheus(std::string& result, std::string_view globals,
                           std::string_view alternativeName) const {
  _b.push();
  result.append(alternativeName.empty() ? name() : alternativeName);
  result.push_back('{');
  bool haveGlobals = !globals.empty();
  if (haveGlobals) {
    result.append(globals);
  }
  if (!labels().empty()) {
    if (haveGlobals) {
      result.push_back(',');
    }
    result.append(labels());
  }
  result.append("} ");
  result.append(std::to_string(load()));
  result.push_back('\n');
}

uint64_t Counter::load() const noexcept {
  _b.push();
  return _c.load();
}

void Counter::store(uint64_t n) noexcept { _c.exchange(n); }

void Counter::count(uint64_t n) noexcept { _b += n; }

void Counter::count() noexcept { ++_b; }

Counter& Counter::operator=(uint64_t n) noexcept {
  store(n);
  return *this;
}

Counter& Counter::operator+=(uint64_t n) noexcept {
  count(n);
  return *this;
}

Counter& Counter::operator++() noexcept {
  count();
  return *this;
}

std::ostream& Counter::print(std::ostream& output) const {
  return output << _c;
}

std::ostream& operator<<(std::ostream& output, Counter const& s) {
  return output << s.load();
}

}  // namespace arangodb::metrics
