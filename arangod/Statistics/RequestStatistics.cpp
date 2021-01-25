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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RequestStatistics.h"
#include "Basics/MutexLocker.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#include <iomanip>

#include <boost/lockfree/queue.hpp>

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

static size_t const QUEUE_SIZE = 64 * 1024 - 2;  // current (1.62) boost maximum

static std::unique_ptr<RequestStatistics[]> _statisticsBuffer;

static boost::lockfree::queue<RequestStatistics*, boost::lockfree::capacity<QUEUE_SIZE>> _freeList;

static boost::lockfree::queue<RequestStatistics*, boost::lockfree::capacity<QUEUE_SIZE>> _finishedList;

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

void RequestStatistics::initialize() {
  _statisticsBuffer.reset(new RequestStatistics[QUEUE_SIZE]());

  for (size_t i = 0; i < QUEUE_SIZE; ++i) {
    RequestStatistics* entry = &_statisticsBuffer[i];
    TRI_ASSERT(entry->_released);
    TRI_ASSERT(!entry->_inQueue);
    bool ok = _freeList.push(entry);
    TRI_ASSERT(ok);
  }
}

size_t RequestStatistics::processAll() {
  RequestStatistics* statistics = nullptr;
  size_t count = 0;

  while (_finishedList.pop(statistics)) {
    if (statistics != nullptr) {
      TRI_ASSERT(!statistics->_released);
      TRI_ASSERT(statistics->_inQueue);
      statistics->_inQueue = false;
      process(statistics);
      ++count;
    }
  }

  return count;
}

RequestStatistics::Item RequestStatistics::acquire() {
  RequestStatistics* statistics = nullptr;

  if (_freeList.pop(statistics)) {
    TRI_ASSERT(statistics->_released);
    TRI_ASSERT(!statistics->_inQueue);
    statistics->_released = false;
  } else {
    statistics = nullptr;
    LOG_TOPIC("62d99", TRACE, arangodb::Logger::FIXME)
        << "no free element on statistics queue";
  }

  return Item{statistics};
}

// -----------------------------------------------------------------------------
// --SECTION--                                            static private methods
// -----------------------------------------------------------------------------

void RequestStatistics::process(RequestStatistics* statistics) {
  TRI_ASSERT(statistics != nullptr);

  {
    statistics::TotalRequests.incCounter();

    if (statistics->_async) {
      statistics::AsyncRequests.incCounter();
    }

    statistics::MethodRequests[(size_t)statistics->_requestType].incCounter();

    // check that the request was completely received and transmitted
    if (statistics->_readStart != 0.0 &&
        (statistics->_async || statistics->_writeEnd != 0.0)) {
      double totalTime;

      if (statistics->_async) {
        totalTime = statistics->_requestEnd - statistics->_readStart;
      } else {
        totalTime = statistics->_writeEnd - statistics->_readStart;
      }
      
      bool const isSuperuser = statistics->_superuser;
      if (isSuperuser) {
        statistics::TotalRequestsSuperuser.incCounter();
      } else {
        statistics::TotalRequestsUser.incCounter();
      }

      statistics::RequestFigures& figures = isSuperuser
        ? statistics::SuperuserRequestFigures
        : statistics::UserRequestFigures;

      figures.totalTimeDistribution.addFigure(totalTime);

      double requestTime = statistics->_requestEnd - statistics->_requestStart;
      figures.requestTimeDistribution.addFigure(requestTime);

      double queueTime = 0.0;
      if (statistics->_queueStart != 0.0 && statistics->_queueEnd != 0.0) {
        queueTime = statistics->_queueEnd - statistics->_queueStart;
        figures.queueTimeDistribution.addFigure(queueTime);
      }

      double ioTime = totalTime - requestTime - queueTime;
      if (ioTime >= 0.0) {
        figures.ioTimeDistribution.addFigure(ioTime);
      }

      figures.bytesSentDistribution.addFigure(statistics->_sentBytes);
      figures.bytesReceivedDistribution.addFigure(statistics->_receivedBytes);
    }
  }

  // clear statistics
  statistics->reset();

  // put statistics item back onto the freelist
  int tries = 0;

  while (++tries < 1000) {
    if (_freeList.push(statistics)) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  if (tries > 1) {
    LOG_TOPIC("fb453", WARN, Logger::MEMORY) << "_freeList.push failed " << tries - 1 << " times.";
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

void RequestStatistics::release() {
  TRI_ASSERT(!_released);
  TRI_ASSERT(!_inQueue);

  _inQueue = true;
  bool ok = _finishedList.push(this);
  TRI_ASSERT(ok);
}

void RequestStatistics::getSnapshot(Snapshot& snapshot, stats::RequestStatisticsSource source) {
  statistics::RequestFigures& figures = source == stats::RequestStatisticsSource::USER
    ? statistics::UserRequestFigures
    : statistics::SuperuserRequestFigures;

  snapshot.totalTime = figures.totalTimeDistribution;
  snapshot.requestTime = figures.requestTimeDistribution;
  snapshot.queueTime = figures.queueTimeDistribution;
  snapshot.ioTime = figures.ioTimeDistribution;
  snapshot.bytesSent = figures.bytesSentDistribution;
  snapshot.bytesReceived = figures.bytesReceivedDistribution;
  
  if (source == stats::RequestStatisticsSource::ALL) {
    TRI_ASSERT(&figures == &statistics::SuperuserRequestFigures);
    snapshot.totalTime.add(statistics::UserRequestFigures.totalTimeDistribution);
    snapshot.requestTime.add(statistics::UserRequestFigures.requestTimeDistribution);
    snapshot.queueTime.add(statistics::UserRequestFigures.queueTimeDistribution);
    snapshot.ioTime.add(statistics::UserRequestFigures.ioTimeDistribution);
    snapshot.bytesSent.add(statistics::UserRequestFigures.bytesSentDistribution);
    snapshot.bytesReceived.add(statistics::UserRequestFigures.bytesReceivedDistribution);
  }
}

std::string RequestStatistics::Item::timingsCsv() const {
  TRI_ASSERT(_stat != nullptr);
  std::stringstream ss;

  ss << std::setprecision(9) << std::fixed
     << "read," << (_stat->_readEnd - _stat->_readStart)
     << ",queue," << (_stat->_queueEnd - _stat->_queueStart)
     << ",queue-size," << _stat->_queueSize
     << ",request," << (_stat->_requestEnd - _stat->_requestStart)
     << ",total," << (StatisticsFeature::time() - _stat->_readStart);

  return ss.str();
}
