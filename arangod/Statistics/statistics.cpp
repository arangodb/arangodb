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

#include "statistics.h"

#include <boost/lockfree/queue.hpp>

#include "Logger/Logger.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Statistics/StatisticsFeature.h"

using namespace arangodb;
using namespace arangodb::basics;

#ifdef USE_DEV_TIMERS
thread_local TRI_request_statistics_t* TRI_request_statistics_t::STATS =
    nullptr;
#endif

static size_t const QUEUE_SIZE = 1000;

std::string TRI_request_statistics_t::to_string() {
  std::stringstream ss;
  ss << std::boolalpha << std::setprecision(20) << "statistics      "
      << std::endl
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

void TRI_request_statistics_t::trace_log(){
  LOG_TOPIC(TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
      << "_readStart      " << _readStart;
  LOG_TOPIC(TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
      << "_readEnd        " << _readEnd;
  LOG_TOPIC(TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
      << "_queueStart     " << _queueStart;
  LOG_TOPIC(TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
      << "_queueEnd       " << _queueEnd;
  LOG_TOPIC(TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
      << "_requestStart   " << _requestStart;
  LOG_TOPIC(TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
      << "_requestEnd     " << _requestEnd;
  LOG_TOPIC(TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
      << "_writeStart     " << _writeStart;
  LOG_TOPIC(TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
      << "_writeEnd       " << _writeEnd;
  LOG_TOPIC(TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
      << "_receivedBytes  " << _receivedBytes;
  LOG_TOPIC(TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
      << "_sentBytes      " << _sentBytes;
  LOG_TOPIC(TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
      << "_async          " << _async;
  LOG_TOPIC(TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
      << "_tooLarge       " << _tooLarge;
  LOG_TOPIC(TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
      << "_executeError   " << _executeError;
  LOG_TOPIC(TRACE, Logger::REQUESTS) << std::boolalpha << std::setprecision(20)
      << "_ignore         " << _ignore;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for request statistics data
////////////////////////////////////////////////////////////////////////////////

static arangodb::Mutex RequestDataLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief the request statistics queue
////////////////////////////////////////////////////////////////////////////////

static boost::lockfree::queue<TRI_request_statistics_t*,
                              boost::lockfree::capacity<QUEUE_SIZE>>
    RequestFreeList;

////////////////////////////////////////////////////////////////////////////////
/// @brief the request statistics queue for finished requests
////////////////////////////////////////////////////////////////////////////////

static boost::lockfree::queue<TRI_request_statistics_t*,
                              boost::lockfree::capacity<QUEUE_SIZE>>
    RequestFinishedList;

////////////////////////////////////////////////////////////////////////////////
/// @brief processes a statistics block
////////////////////////////////////////////////////////////////////////////////

static void ProcessRequestStatistics(TRI_request_statistics_t* statistics) {
  TRI_ASSERT(statistics != nullptr);

  {
    MUTEX_LOCKER(mutexLocker, RequestDataLock);

    TRI_TotalRequestsStatistics.incCounter();

    if (statistics->_async) {
      TRI_AsyncRequestsStatistics.incCounter();
    }

    TRI_MethodRequestsStatistics[(int)statistics->_requestType].incCounter();

    // check that the request was completely received and transmitted
    if (statistics->_readStart != 0.0 &&
        (statistics->_async || statistics->_writeEnd != 0.0)) {
      double totalTime;

      if (statistics->_async) {
        totalTime = statistics->_requestEnd - statistics->_readStart;
      } else {
        totalTime = statistics->_writeEnd - statistics->_readStart;
      }

      TRI_TotalTimeDistributionStatistics->addFigure(totalTime);

      double requestTime = statistics->_requestEnd - statistics->_requestStart;
      TRI_RequestTimeDistributionStatistics->addFigure(requestTime);

      double queueTime = 0.0;

      if (statistics->_queueStart != 0.0 && statistics->_queueEnd != 0.0) {
        queueTime = statistics->_queueEnd - statistics->_queueStart;
        TRI_QueueTimeDistributionStatistics->addFigure(queueTime);
      }

      double ioTime = totalTime - requestTime - queueTime;

      if (ioTime >= 0.0) {
        TRI_IoTimeDistributionStatistics->addFigure(ioTime);
      }

      TRI_BytesSentDistributionStatistics->addFigure(statistics->_sentBytes);
      TRI_BytesReceivedDistributionStatistics->addFigure(
          statistics->_receivedBytes);

#ifdef USE_DEV_TIMERS
      LOG_TOPIC(INFO, Logger::REQUESTS)
          << "\"http-request-timing\",\"" << statistics->_id << "\","
          << (statistics->_async ? "async" : "sync") << ",total(us),"
          << Logger::FIXED(totalTime) << ",io," << Logger::FIXED(ioTime)
          << ",queue," << Logger::FIXED(queueTime) << ",request,"
          << Logger::FIXED(requestTime) << ",received,"
          << statistics->_receivedBytes << ",sent," << statistics->_sentBytes;
#endif
    }
  }

  // clear statistics
  statistics->reset();

  // put statistics item back onto the freelist
  int tries = 0;
  while (++tries < 1000) {
    if (RequestFreeList.push(statistics)) {
      break;
    }
    usleep(10000);
  }
  if (tries > 1) {
    LOG_TOPIC(WARN, Logger::REQUESTS) << "RequestFreeList.push failed "
      << tries - 1 << " times.";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processes finished statistics block
////////////////////////////////////////////////////////////////////////////////

static size_t ProcessAllRequestStatistics() {
  TRI_request_statistics_t* statistics = nullptr;
  size_t count = 0;

  while (RequestFinishedList.pop(statistics)) {
    if (statistics != nullptr) {
      ProcessRequestStatistics(statistics);
      ++count;
    }
  }

  return count;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a new statistics block
////////////////////////////////////////////////////////////////////////////////

TRI_request_statistics_t* TRI_AcquireRequestStatistics() {
  TRI_request_statistics_t* statistics = nullptr;

  if (StatisticsFeature::enabled() && RequestFreeList.pop(statistics)) {
    return statistics;
  }

  LOG(TRACE) << "no free element on statistics queue";

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a statistics block
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseRequestStatistics(TRI_request_statistics_t* statistics) {
  if (statistics == nullptr) {
    return;
  }

  if (!statistics->_ignore) {
    bool ok = RequestFinishedList.push(statistics);
    TRI_ASSERT(ok);
  } else {
    statistics->reset();

    bool ok = RequestFreeList.push(statistics);
    TRI_ASSERT(ok);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the current statistics
////////////////////////////////////////////////////////////////////////////////

void TRI_FillRequestStatistics(StatisticsDistribution& totalTime,
                               StatisticsDistribution& requestTime,
                               StatisticsDistribution& queueTime,
                               StatisticsDistribution& ioTime,
                               StatisticsDistribution& bytesSent,
                               StatisticsDistribution& bytesReceived) {
  if (!StatisticsFeature::enabled()) {
    // all the below objects may be deleted if we don't have statistics enabled
    return;
  }

  MUTEX_LOCKER(mutexLocker, RequestDataLock);

  totalTime = *TRI_TotalTimeDistributionStatistics;
  requestTime = *TRI_RequestTimeDistributionStatistics;
  queueTime = *TRI_QueueTimeDistributionStatistics;
  ioTime = *TRI_IoTimeDistributionStatistics;
  bytesSent = *TRI_BytesSentDistributionStatistics;
  bytesReceived = *TRI_BytesReceivedDistributionStatistics;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for connection data
////////////////////////////////////////////////////////////////////////////////

static arangodb::Mutex ConnectionDataLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief free list
////////////////////////////////////////////////////////////////////////////////

static boost::lockfree::queue<TRI_connection_statistics_t*,
                              boost::lockfree::capacity<QUEUE_SIZE>>
    ConnectionFreeList;

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a new statistics block
////////////////////////////////////////////////////////////////////////////////

TRI_connection_statistics_t* TRI_AcquireConnectionStatistics() {
  TRI_connection_statistics_t* statistics = nullptr;

  if (StatisticsFeature::enabled() && ConnectionFreeList.pop(statistics)) {
    return statistics;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a statistics block
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseConnectionStatistics(TRI_connection_statistics_t* statistics) {
  if (statistics == nullptr) {
    return;
  }

  {
    MUTEX_LOCKER(mutexLocker, ConnectionDataLock);

    if (statistics->_http) {
      TRI_HttpConnectionsStatistics.decCounter();
    }

    if (statistics->_connStart != 0.0 && statistics->_connEnd != 0.0) {
      double totalTime = statistics->_connEnd - statistics->_connStart;
      TRI_ConnectionTimeDistributionStatistics->addFigure(totalTime);
    }
  }

  // clear statistics
  statistics->reset();

// put statistics item back onto the freelist
  bool ok = ConnectionFreeList.push(statistics);
  TRI_ASSERT(ok);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the current statistics
////////////////////////////////////////////////////////////////////////////////

void TRI_FillConnectionStatistics(
    StatisticsCounter& httpConnections, StatisticsCounter& totalRequests,
    std::vector<StatisticsCounter>& methodRequests,
    StatisticsCounter& asyncRequests, StatisticsDistribution& connectionTime) {
  if (!StatisticsFeature::enabled()) {
    // all the below objects may be deleted if we don't have statistics enabled
    for (int i = 0;
         i < ((int)arangodb::rest::RequestType::ILLEGAL) + 1; ++i) {
      methodRequests.emplace_back(StatisticsCounter());
    }
    return;
  }

  MUTEX_LOCKER(mutexLocker, ConnectionDataLock);

  httpConnections = TRI_HttpConnectionsStatistics;
  totalRequests = TRI_TotalRequestsStatistics;
  methodRequests = TRI_MethodRequestsStatistics;
  asyncRequests = TRI_AsyncRequestsStatistics;
  connectionTime = *TRI_ConnectionTimeDistributionStatistics;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the global server statistics
////////////////////////////////////////////////////////////////////////////////

TRI_server_statistics_t TRI_GetServerStatistics() {
  TRI_server_statistics_t server;

  server._startTime = TRI_ServerStatistics._startTime;
  server._uptime = TRI_microtime() - server._startTime;

  return server;
}

/// @brief thread used for statistics
void StatisticsThread::run() {
  uint64_t sleepTime = 100 * 1000;
  uint64_t const MaxSleepTime = 250 * 1000;
  int nothingHappened = 0;

  while (!isStopping() && StatisticsFeature::enabled()) {
    size_t count = ProcessAllRequestStatistics();

    if (count == 0) {
      if (++nothingHappened == 10 * 30) {
        // increase sleep time every 30 seconds
        nothingHappened = 0;
        sleepTime += 50 * 1000;

        if (sleepTime > MaxSleepTime) {
          sleepTime = MaxSleepTime;
        }
      }
#ifdef _WIN32
      usleep((unsigned long)sleepTime);
#else
      usleep((useconds_t)sleepTime);
#endif
    } else {
      nothingHappened = 0;

      if (count < 10) {
        usleep(10 * 1000);
      } else if (count < 100) {
        usleep(1 * 1000);
      }
    }
  }

  delete TRI_ConnectionTimeDistributionStatistics;
  TRI_ConnectionTimeDistributionStatistics = nullptr;

  delete TRI_TotalTimeDistributionStatistics;
  TRI_TotalTimeDistributionStatistics = nullptr;

  delete TRI_RequestTimeDistributionStatistics;
  TRI_RequestTimeDistributionStatistics = nullptr;

  delete TRI_QueueTimeDistributionStatistics;
  TRI_QueueTimeDistributionStatistics = nullptr;

  delete TRI_IoTimeDistributionStatistics;
  TRI_IoTimeDistributionStatistics = nullptr;

  delete TRI_BytesSentDistributionStatistics;
  TRI_BytesSentDistributionStatistics = nullptr;

  delete TRI_BytesReceivedDistributionStatistics;
  TRI_BytesReceivedDistributionStatistics = nullptr;

  {
    TRI_request_statistics_t* entry = nullptr;
    while (RequestFreeList.pop(entry)) {
      delete entry;
    }
  }

  {
    TRI_request_statistics_t* entry = nullptr;
    while (RequestFinishedList.pop(entry)) {
      delete entry;
    }
  }

  {
    TRI_connection_statistics_t* entry = nullptr;
    while (ConnectionFreeList.pop(entry)) {
      delete entry;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief number of http connections
////////////////////////////////////////////////////////////////////////////////

StatisticsCounter TRI_HttpConnectionsStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief total number of requests
////////////////////////////////////////////////////////////////////////////////

StatisticsCounter TRI_TotalRequestsStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of requests by HTTP method
////////////////////////////////////////////////////////////////////////////////

std::vector<StatisticsCounter> TRI_MethodRequestsStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of async requests
////////////////////////////////////////////////////////////////////////////////

StatisticsCounter TRI_AsyncRequestsStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief connection time distribution vector
////////////////////////////////////////////////////////////////////////////////

StatisticsVector TRI_ConnectionTimeDistributionVectorStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief total time distribution
////////////////////////////////////////////////////////////////////////////////

StatisticsDistribution* TRI_ConnectionTimeDistributionStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief request time distribution vector
////////////////////////////////////////////////////////////////////////////////

StatisticsVector TRI_RequestTimeDistributionVectorStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief total time distribution
////////////////////////////////////////////////////////////////////////////////

StatisticsDistribution* TRI_TotalTimeDistributionStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief request time distribution
////////////////////////////////////////////////////////////////////////////////

StatisticsDistribution* TRI_RequestTimeDistributionStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief queue time distribution
////////////////////////////////////////////////////////////////////////////////

StatisticsDistribution* TRI_QueueTimeDistributionStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief i/o distribution
////////////////////////////////////////////////////////////////////////////////

StatisticsDistribution* TRI_IoTimeDistributionStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief bytes sent distribution vector
////////////////////////////////////////////////////////////////////////////////

StatisticsVector TRI_BytesSentDistributionVectorStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief bytes sent distribution
////////////////////////////////////////////////////////////////////////////////

StatisticsDistribution* TRI_BytesSentDistributionStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief bytes received distribution vector
////////////////////////////////////////////////////////////////////////////////

StatisticsVector TRI_BytesReceivedDistributionVectorStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief bytes received distribution
////////////////////////////////////////////////////////////////////////////////

StatisticsDistribution* TRI_BytesReceivedDistributionStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief global server statistics
////////////////////////////////////////////////////////////////////////////////

TRI_server_statistics_t TRI_ServerStatistics;

/// @brief module init function
void TRI_InitializeStatistics() {
  TRI_ServerStatistics._startTime = TRI_microtime();

  // .............................................................................
  // sets up the statistics
  // .............................................................................

  TRI_ConnectionTimeDistributionVectorStatistics << (0.1) << (1.0) << (60.0);

  TRI_BytesSentDistributionVectorStatistics << (250) << (1000) << (2 * 1000)
                                            << (5 * 1000) << (10 * 1000);
  TRI_BytesReceivedDistributionVectorStatistics << (250) << (1000) << (2 * 1000)
                                                << (5 * 1000) << (10 * 1000);
  TRI_RequestTimeDistributionVectorStatistics << (0.01) << (0.05) << (0.1)
                                              << (0.2) << (0.5) << (1.0);

  TRI_ConnectionTimeDistributionStatistics = new StatisticsDistribution(
      TRI_ConnectionTimeDistributionVectorStatistics);
  TRI_TotalTimeDistributionStatistics =
      new StatisticsDistribution(TRI_RequestTimeDistributionVectorStatistics);
  TRI_RequestTimeDistributionStatistics =
      new StatisticsDistribution(TRI_RequestTimeDistributionVectorStatistics);
  TRI_QueueTimeDistributionStatistics =
      new StatisticsDistribution(TRI_RequestTimeDistributionVectorStatistics);
  TRI_IoTimeDistributionStatistics =
      new StatisticsDistribution(TRI_RequestTimeDistributionVectorStatistics);
  TRI_BytesSentDistributionStatistics =
      new StatisticsDistribution(TRI_BytesSentDistributionVectorStatistics);
  TRI_BytesReceivedDistributionStatistics =
      new StatisticsDistribution(TRI_BytesReceivedDistributionVectorStatistics);

  // initialize counters for all HTTP request types
  TRI_MethodRequestsStatistics.clear();

  for (int i = 0; i < ((int)arangodb::rest::RequestType::ILLEGAL) + 1;
       ++i) {
    StatisticsCounter c;
    TRI_MethodRequestsStatistics.emplace_back(c);
  }

  // .............................................................................
  // generate the request statistics queue
  // .............................................................................

  for (size_t i = 0; i < QUEUE_SIZE; ++i) {
    auto entry = new TRI_request_statistics_t;
    bool ok = RequestFreeList.push(entry);
    TRI_ASSERT(ok);
  }

  // .............................................................................
  // generate the connection statistics queue
  // .............................................................................

  for (size_t i = 0; i < QUEUE_SIZE; ++i) {
    auto entry = new TRI_connection_statistics_t;
    bool ok = ConnectionFreeList.push(entry);
    TRI_ASSERT(ok);
  }
}

