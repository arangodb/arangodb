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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_STATISTICS_FIGURES_H
#define ARANGOD_STATISTICS_FIGURES_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"

namespace arangodb {
namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief a simple counter
////////////////////////////////////////////////////////////////////////////////

struct StatisticsCounter {
  StatisticsCounter() : _count(0) {}

  StatisticsCounter& operator=(StatisticsCounter const& other) {
    _count.store(other._count.load());
    return *this;
  }

  void incCounter() { ++_count; }

  void decCounter() { --_count; }

  std::atomic<int64_t> _count;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief a distribution with count, min, max, mean, and variance
////////////////////////////////////////////////////////////////////////////////

struct StatisticsDistribution {
  StatisticsDistribution() : _count(0), _total(0.0), _cuts(), _counts() {}

  explicit StatisticsDistribution(std::vector<double> const& dist)
      : _count(0), _total(0.0), _cuts(dist), _counts() {
    _counts.resize(_cuts.size() + 1);
  }

  StatisticsDistribution const& operator=(StatisticsDistribution& other) {
    MUTEX_LOCKER(l1, _mutex);
    MUTEX_LOCKER(l2, other._mutex);

    _count = other._count;
    _total = other._total;
    _cuts = other._cuts;
    _counts = other._counts;

    return *this;
  }

  void addFigure(double value) {
    MUTEX_LOCKER(lock, _mutex);

    ++_count;
    _total += value;

    std::vector<double>::iterator i = _cuts.begin();
    std::vector<uint64_t>::iterator j = _counts.begin();

    for (; i != _cuts.end(); ++i, ++j) {
      if (value < *i) {
        ++(*j);
        return;
      }
    }

    ++(*j);
  }

  uint64_t _count;
  double _total;
  std::vector<double> _cuts;
  std::vector<uint64_t> _counts;

 private:
  Mutex _mutex;
};
}  // namespace basics
}  // namespace arangodb

#endif
