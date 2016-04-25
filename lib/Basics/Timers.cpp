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

#include "Timers.h"

using namespace arangodb::basics;

std::vector<double> Timers::starts(TIMER_MAX);
std::vector<double> Timers::totals(TIMER_MAX);
std::vector<uint64_t> Timers::counts(TIMER_MAX);

void Timers::clear() {
  for (auto& it : totals) {
    it = 0.0;
  }
  for (auto& it : counts) {
    it = 0;
  }
}

std::map<std::string, std::pair<double, uint64_t>> Timers::get() {
  std::map<std::string, std::pair<double, uint64_t>> result;

  int const n = static_cast<int>(TIMER_MAX);
  for (int i = static_cast<int>(TIMER_MIN) + 1; i < n; ++i) {
    result.emplace(translateName(TimerType(i)), std::make_pair(totals[i], counts[i]));
  }
  return result;
}
