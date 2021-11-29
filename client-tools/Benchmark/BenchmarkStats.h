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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <limits>
#include <utility>

namespace arangodb::arangobench {

struct BenchmarkStats {
  constexpr BenchmarkStats() noexcept 
      : min(std::numeric_limits<double>::max()),
        max(std::numeric_limits<double>::min()),
        total(0.0),
        count(0) {}

  void reset() noexcept {
    *this = BenchmarkStats{};
  }

  void track(double time) noexcept {
    min = std::min(min, time);
    max = std::max(max, time);
    total += time;
    ++count;
  }

  void add(BenchmarkStats const& other) noexcept {
    min = std::min(min, other.min);
    max = std::max(max, other.max);
    total += other.total;
    count += other.count;
  }

  double avg() const noexcept {
    return (count != 0) ? (total / count) : 0.0;
  }

  double min;
  double max;
  double total;
  uint64_t count;
};

}
