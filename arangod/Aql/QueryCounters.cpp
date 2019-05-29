////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "QueryCounters.h"

using namespace arangodb::aql;

void QueryCounters::clear(std::string const& name) {
  auto it = _counters.find(name);

  if (it != _counters.end()) {
    (*it).second = 0;
  }
}

int64_t QueryCounters::set(std::string const& name, int64_t newValue) {
  int64_t old;
  auto it = _counters.find(name);

  if (it == _counters.end()) {
    _counters.emplace(name, newValue);
    old = 0;
  } else {
    old = (*it).second;
    (*it).second = newValue;
  }
  return old;
}

int64_t QueryCounters::adjust(std::string const& name, bool increase, int64_t delta) {
  auto it = _counters.find(name);

  if (it == _counters.end()) {
    it = _counters.emplace(name, 0).first;
  } 
  int64_t old = (*it).second;
  if (increase) {
    (*it).second += delta;
  } else {
    (*it).second -= delta;
  }
  return old;
}

int64_t QueryCounters::get(std::string const& name) const {
  auto it = _counters.find(name);

  if (it == _counters.end()) {
    return 0;
  }
  return (*it).second;
}
