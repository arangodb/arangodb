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

namespace arangodb {
namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief vector generator
////////////////////////////////////////////////////////////////////////////////

struct StatisticsVector {
  StatisticsVector() : _value() {}

  StatisticsVector& operator<<(double v) {
    _value.push_back(v);
    return *this;
  }

  std::vector<double> _value;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief a simple counter
////////////////////////////////////////////////////////////////////////////////

struct StatisticsCounter {
  StatisticsCounter() : _count(0) {}

  void incCounter() { ++_count; }

  void decCounter() { --_count; }

  int64_t _count;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief a distribution with count, min, max, mean, and variance
////////////////////////////////////////////////////////////////////////////////

struct StatisticsDistribution {
  StatisticsDistribution() : _count(0), _total(0.0), _cuts(), _counts() {}

  StatisticsDistribution(StatisticsVector const& dist)
      : _count(0), _total(0.0), _cuts(dist._value), _counts() {
    _counts.resize(_cuts.size() + 1);
  }

  void addFigure(double value) {
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
};
}
}

#endif
