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

#include "RequestStatistics.h"
#include "Basics/MutexLocker.h"
#include "Logger/Logger.h"

#include <iomanip>

using namespace arangodb;
using namespace arangodb::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                    static members
// -----------------------------------------------------------------------------

std::unique_ptr<RequestStatistics[]> RequestStatistics::_statisticsBuffer;

boost::lockfree::queue<RequestStatistics*, boost::lockfree::capacity<RequestStatistics::QUEUE_SIZE>> RequestStatistics::_freeList;

boost::lockfree::queue<RequestStatistics*, boost::lockfree::capacity<RequestStatistics::QUEUE_SIZE>> RequestStatistics::_finishedList;

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

RequestStatistics* RequestStatistics::acquire() {
  if (!StatisticsFeature::enabled()) {
    return nullptr;
  }

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

  return statistics;
}

// -----------------------------------------------------------------------------
// --SECTION--                                            static private methods
// -----------------------------------------------------------------------------

void RequestStatistics::process(RequestStatistics* statistics) {
  TRI_ASSERT(statistics != nullptr);

  {
    TRI_TotalRequestsStatistics.incCounter();

    if (statistics->_async) {
      TRI_AsyncRequestsStatistics.incCounter();
    }

    {
      MUTEX_LOCKER(locker, TRI_RequestsStatisticsMutex);
      TRI_MethodRequestsStatistics[(size_t)statistics->_requestType].incCounter();
    }

    // check that the request was completely received and transmitted
    if (statistics->_readStart != 0.0 &&
        (statistics->_async || statistics->_writeEnd != 0.0)) {
      double totalTime;

      if (statistics->_async) {
        totalTime = statistics->_requestEnd - statistics->_readStart;
      } else {
        totalTime = statistics->_writeEnd - statistics->_readStart;
      }

      TRI_TotalTimeDistributionStatistics.addFigure(totalTime);

      double requestTime = statistics->_requestEnd - statistics->_requestStart;
      TRI_RequestTimeDistributionStatistics.addFigure(requestTime);

      double queueTime = 0.0;

      if (statistics->_queueStart != 0.0 && statistics->_queueEnd != 0.0) {
        queueTime = statistics->_queueEnd - statistics->_queueStart;
        TRI_QueueTimeDistributionStatistics.addFigure(queueTime);
      }

      double ioTime = totalTime - requestTime - queueTime;

      if (ioTime >= 0.0) {
        TRI_IoTimeDistributionStatistics.addFigure(ioTime);
      }

      TRI_BytesSentDistributionStatistics.addFigure(statistics->_sentBytes);
      TRI_BytesReceivedDistributionStatistics.addFigure(statistics->_receivedBytes);
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
    std::this_thread::sleep_for(std::chrono::microseconds(10000));
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

  if (!_ignore) {
    _inQueue = true;
    bool ok = _finishedList.push(this);
    TRI_ASSERT(ok);
  } else {
    reset();
    bool ok = _freeList.push(this);
    TRI_ASSERT(ok);
  }
}

void RequestStatistics::fill(StatisticsDistribution& totalTime,
                             StatisticsDistribution& requestTime,
                             StatisticsDistribution& queueTime,
                             StatisticsDistribution& ioTime, StatisticsDistribution& bytesSent,
                             StatisticsDistribution& bytesReceived) {
  if (!StatisticsFeature::enabled()) {
    // all the below objects may be deleted if we don't have statistics enabled
    return;
  }

  totalTime = TRI_TotalTimeDistributionStatistics;
  requestTime = TRI_RequestTimeDistributionStatistics;
  queueTime = TRI_QueueTimeDistributionStatistics;
  ioTime = TRI_IoTimeDistributionStatistics;
  bytesSent = TRI_BytesSentDistributionStatistics;
  bytesReceived = TRI_BytesReceivedDistributionStatistics;
}

std::string RequestStatistics::timingsCsv() {
  std::stringstream ss;

  ss << std::setprecision(9) << std::fixed << "read," << (_readEnd - _readStart)
     << ",queue," << (_queueEnd - _queueStart) << ",queue-size," << _queueSize
     << ",request," << (_requestEnd - _requestStart) << ",total,"
     << (StatisticsFeature::time() - _readStart) << ",error,"
     << (_executeError ? "true" : "false");

  return ss.str();
}

std::string RequestStatistics::to_string() {
  std::stringstream ss;

  ss << std::boolalpha << std::setprecision(20) << "statistics      " << std::endl
     << "_readStart      " << _readStart << std::endl
     << "_readEnd        " << _readEnd << std::endl
     << "_queueStart     " << _queueStart << std::endl
     << "_queueEnd       " << _queueEnd << std::endl
     << "_requestStart   " << _requestStart << std::endl
     << "_requestEnd     " << _requestEnd << std::endl
     << "_writeStart     " << _writeStart << std::endl
     << "_writeEnd       " << _writeEnd << std::endl
     << "_receivedBytes  " << _receivedBytes << std::endl
     << "_sentBytes      " << _sentBytes << std::endl
     << "_async          " << _async << std::endl
     << "_tooLarge       " << _tooLarge << std::endl
     << "_executeError   " << _executeError << std::endl
     << "_ignore         " << _ignore << std::endl;

  return ss.str();
}

void RequestStatistics::trace_log() {
  LOG_TOPIC("4a0b6", TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
                                     << "_readStart      " << _readStart;

  LOG_TOPIC("8620b", TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
                                     << "_readEnd        " << _readEnd;

  LOG_TOPIC("13bae", TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
                                     << "_queueStart     " << _queueStart;

  LOG_TOPIC("e6292", TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
                                     << "_queueEnd       " << _queueEnd;

  LOG_TOPIC("9c947", TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
                                     << "_requestStart   " << _requestStart;

  LOG_TOPIC("09e63", TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
                                     << "_requestEnd     " << _requestEnd;

  LOG_TOPIC("4eef0", TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
                                     << "_writeStart     " << _writeStart;

  LOG_TOPIC("3922b", TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
                                     << "_writeEnd       " << _writeEnd;

  LOG_TOPIC("49e75", TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
                                     << "_receivedBytes  " << _receivedBytes;

  LOG_TOPIC("399d0", TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
                                     << "_sentBytes      " << _sentBytes;

  LOG_TOPIC("54d62", TRACE, Logger::REQUESTS)
      << std::boolalpha << std::setprecision(20) << "_async          " << _async;

  LOG_TOPIC("5e68c", TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
                                     << "_tooLarge       " << _tooLarge;

  LOG_TOPIC("f4089", TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
                                     << "_executeError   " << _executeError;

  LOG_TOPIC("31657", TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
                                     << "_ignore         " << _ignore;
}
