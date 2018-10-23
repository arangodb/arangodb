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

#include "ConnectionStatistics.h"

#include "Basics/MutexLocker.h"
#include "Rest/CommonDefines.h"

using namespace arangodb;
using namespace arangodb::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                    static members
// -----------------------------------------------------------------------------

std::unique_ptr<ConnectionStatistics[]> ConnectionStatistics::_statisticsBuffer;

boost::lockfree::queue<
    ConnectionStatistics*,
    boost::lockfree::capacity<ConnectionStatistics::QUEUE_SIZE>>
    ConnectionStatistics::_freeList;

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

void ConnectionStatistics::SET_HTTP(ConnectionStatistics* stat) {
  if (stat != nullptr) {
    stat->_http = true;

    TRI_HttpConnectionsStatistics.incCounter();
  }
}

void ConnectionStatistics::initialize() {
  _statisticsBuffer.reset(new ConnectionStatistics[QUEUE_SIZE]());

  for (size_t i = 0; i < QUEUE_SIZE; ++i) {
    ConnectionStatistics* entry = &_statisticsBuffer[i];
    bool ok = _freeList.push(entry);
    TRI_ASSERT(ok);
  }
}

ConnectionStatistics* ConnectionStatistics::acquire() {
  ConnectionStatistics* statistics = nullptr;

  if (StatisticsFeature::enabled() && _freeList.pop(statistics)) {
    return statistics;
  }

  return nullptr;
}

void ConnectionStatistics::fill(StatisticsCounter& httpConnections,
                                StatisticsCounter& totalRequests,
                                std::array<StatisticsCounter, MethodRequestsStatisticsSize>& methodRequests,
                                StatisticsCounter& asyncRequests,
                                StatisticsDistribution& connectionTime) {
  if (!StatisticsFeature::enabled()) {
    // all the below objects may be deleted if we don't have statistics enabled
    return;
  }

  httpConnections = TRI_HttpConnectionsStatistics;
  totalRequests = TRI_TotalRequestsStatistics;
  {
    MUTEX_LOCKER(locker, TRI_RequestsStatisticsMutex);
    methodRequests = TRI_MethodRequestsStatistics;
  }
  asyncRequests = TRI_AsyncRequestsStatistics;
  connectionTime = TRI_ConnectionTimeDistributionStatistics;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

void ConnectionStatistics::release() {
  {
    if (_http) {
      TRI_HttpConnectionsStatistics.decCounter();
    }

    if (_connStart != 0.0 && _connEnd != 0.0) {
      double totalTime = _connEnd - _connStart;
      TRI_ConnectionTimeDistributionStatistics.addFigure(totalTime);
    }
  }

  // clear statistics
  reset();

  // put statistics item back onto the freelist
  bool ok = _freeList.push(this);
  TRI_ASSERT(ok);
}
