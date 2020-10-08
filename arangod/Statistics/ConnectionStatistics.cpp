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

#include "Rest/CommonDefines.h"

#include <boost/lockfree/queue.hpp>

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

static size_t const QUEUE_SIZE = 64 * 1024 - 2;  // current (1.62) boost maximum

static std::unique_ptr<ConnectionStatistics[]> _statisticsBuffer;

static boost::lockfree::queue<ConnectionStatistics*, boost::lockfree::capacity<QUEUE_SIZE>> _freeList;

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

void ConnectionStatistics::Item::SET_HTTP() {
  if (_stat != nullptr) {
    _stat->_http = true;

    statistics::HttpConnections.incCounter();
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

ConnectionStatistics::Item ConnectionStatistics::acquire() {
  ConnectionStatistics* statistics = nullptr;

  if (StatisticsFeature::enabled() && _freeList.pop(statistics)) {
    return Item{ statistics };
  }

  return Item{};
}

void ConnectionStatistics::getSnapshot(Snapshot& snapshot) {
  if (!StatisticsFeature::enabled()) {
    // all the below objects may be deleted if we don't have statistics enabled
    return;
  }

  snapshot.httpConnections = statistics::HttpConnections;
  snapshot.totalRequests = statistics::TotalRequests;
  snapshot.totalRequestsSuperuser = statistics::TotalRequestsSuperuser;
  snapshot.totalRequestsUser = statistics::TotalRequestsUser;
  snapshot.methodRequests = statistics::MethodRequests;
  snapshot.asyncRequests = statistics::AsyncRequests;
  snapshot.connectionTime = statistics::ConnectionTimeDistribution;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

void ConnectionStatistics::release() {
  {
    if (_http) {
      statistics::HttpConnections.decCounter();
    }

    if (_connStart != 0.0 && _connEnd != 0.0) {
      double totalTime = _connEnd - _connStart;
      statistics::ConnectionTimeDistribution.addFigure(totalTime);
    }
  }

  // clear statistics
  reset();

  // put statistics item back onto the freelist
  bool ok = _freeList.push(this);
  TRI_ASSERT(ok);
}
