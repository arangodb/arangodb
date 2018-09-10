////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_STATISTICS_CONNECTION_STATISTICS_H
#define ARANGOD_STATISTICS_CONNECTION_STATISTICS_H 1

#include "Basics/Common.h"

#include "Basics/Mutex.h"
#include "Statistics/StatisticsFeature.h"
#include "Statistics/figures.h"

#include <boost/lockfree/queue.hpp>

namespace arangodb {
class ConnectionStatistics {
 public:
  static void initialize();

  static ConnectionStatistics* acquire();
  void release();

  static void SET_START(ConnectionStatistics* stat) {
    if (stat != nullptr) {
      stat->_connStart = StatisticsFeature::time();
    }
  }

  static void SET_HTTP(ConnectionStatistics* stat);

  static void fill(basics::StatisticsCounter& httpConnections,
                   basics::StatisticsCounter& totalRequests,
                   std::array<basics::StatisticsCounter,
                              basics::MethodRequestsStatisticsSize>& methodRequests,
                   basics::StatisticsCounter& asyncRequests,
                   basics::StatisticsDistribution& connectionTime);

 private:
  ConnectionStatistics() { reset(); }

  void reset() {
    _connStart = 0.0;
    _connEnd = 0.0;
    _http = false;
    _error = false;
  }

  static size_t const QUEUE_SIZE = 64 * 1024 - 2; // current (1.62) boost maximum

  static std::unique_ptr<ConnectionStatistics[]> _statisticsBuffer;

  static boost::lockfree::queue<ConnectionStatistics*,
                                boost::lockfree::capacity<QUEUE_SIZE>>
      _freeList;

 private:
  double _connStart;
  double _connEnd;

  bool _http;
  bool _error;
};
}

#endif
