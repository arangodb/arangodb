////////////////////////////////////////////////////////////////////////////////
/// @brief statistics basics
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "statistics.h"

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/threads.h"

#ifndef BSD
#ifdef __FreeBSD__
#define BSD
#endif
#endif

#if (defined(BSD) || defined(TRI_HAVE_MACOS_MEM_STATS))
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include <boost/lockfree/queue.hpp>

using namespace triagens::basics;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                              private request statistics variables
// -----------------------------------------------------------------------------

static size_t const QUEUE_SIZE = 1000;

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for request statistics data
////////////////////////////////////////////////////////////////////////////////

static triagens::basics::Mutex RequestDataLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief the request statistics queue
////////////////////////////////////////////////////////////////////////////////

boost::lockfree::queue<TRI_request_statistics_t*, boost::lockfree::capacity<QUEUE_SIZE>> RequestFreeList;

////////////////////////////////////////////////////////////////////////////////
/// @brief the request statistics queue for finished requests
////////////////////////////////////////////////////////////////////////////////

boost::lockfree::queue<TRI_request_statistics_t*, boost::lockfree::capacity<QUEUE_SIZE>> RequestFinishedList;

// -----------------------------------------------------------------------------
// --SECTION--                              private request statistics functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief processes a statistics block
////////////////////////////////////////////////////////////////////////////////

static void ProcessRequestStatistics (TRI_request_statistics_t* statistics) {
  {
    MUTEX_LOCKER(RequestDataLock);

    TRI_TotalRequestsStatistics.incCounter();

    if (statistics->_async) {
      TRI_AsyncRequestsStatistics.incCounter();
    }

    TRI_MethodRequestsStatistics[(int) statistics->_requestType].incCounter();

    // check that the request was completely received and transmitted
    if (statistics->_readStart != 0.0 && statistics->_writeEnd != 0.0) {
      double totalTime = statistics->_writeEnd - statistics->_readStart;
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
      TRI_BytesReceivedDistributionStatistics->addFigure(statistics->_receivedBytes);
    }
  }

  // clear statistics
  memset(statistics, 0, sizeof(TRI_request_statistics_t));
  statistics->_requestType = triagens::rest::HttpRequest::HTTP_REQUEST_ILLEGAL;

  // put statistics item back onto the freelist
  bool ok = RequestFreeList.push(statistics);
  TRI_ASSERT(ok);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processes finished statistics block
////////////////////////////////////////////////////////////////////////////////

static size_t ProcessAllRequestStatistics () {
  TRI_request_statistics_t* statistics = nullptr;
  size_t count = 0;

  while (RequestFinishedList.pop(statistics)) {
    ProcessRequestStatistics(statistics);
    ++count;
  }

  return count;
}

// -----------------------------------------------------------------------------
// --SECTION--                               public request statistics functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a new statistics block
////////////////////////////////////////////////////////////////////////////////

TRI_request_statistics_t* TRI_AcquireRequestStatistics () {
  TRI_request_statistics_t* statistics = nullptr;

  if (TRI_ENABLE_STATISTICS && RequestFreeList.pop(statistics)) {
    return statistics;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a statistics block
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseRequestStatistics (TRI_request_statistics_t* statistics) {
  if (statistics == nullptr) {
    return;
  }
 
  if (! statistics->_ignore) {
    bool ok = RequestFinishedList.push(statistics);
    TRI_ASSERT(ok);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the current statistics
////////////////////////////////////////////////////////////////////////////////

void TRI_FillRequestStatistics (StatisticsDistribution& totalTime,
                                StatisticsDistribution& requestTime,
                                StatisticsDistribution& queueTime,
                                StatisticsDistribution& ioTime,
                                StatisticsDistribution& bytesSent,
                                StatisticsDistribution& bytesReceived) {
  MUTEX_LOCKER(RequestDataLock);

  totalTime     = *TRI_TotalTimeDistributionStatistics;
  requestTime   = *TRI_RequestTimeDistributionStatistics;
  queueTime     = *TRI_QueueTimeDistributionStatistics;
  ioTime        = *TRI_IoTimeDistributionStatistics;
  bytesSent     = *TRI_BytesSentDistributionStatistics;
  bytesReceived = *TRI_BytesReceivedDistributionStatistics;
}

// -----------------------------------------------------------------------------
// --SECTION--                           private connection statistics variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for connection data
////////////////////////////////////////////////////////////////////////////////

static triagens::basics::Mutex ConnectionDataLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief free list
////////////////////////////////////////////////////////////////////////////////

boost::lockfree::queue<TRI_connection_statistics_t*, boost::lockfree::capacity<QUEUE_SIZE>> ConnectionFreeList;

// -----------------------------------------------------------------------------
// --SECTION--                            public connection statistics functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a new statistics block
////////////////////////////////////////////////////////////////////////////////

TRI_connection_statistics_t* TRI_AcquireConnectionStatistics () {
  TRI_connection_statistics_t* statistics = nullptr;

  if (TRI_ENABLE_STATISTICS && ConnectionFreeList.pop(statistics)) {
    return statistics;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a statistics block
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseConnectionStatistics (TRI_connection_statistics_t* statistics) {
  if (statistics == nullptr) {
    return;
  }
  
  {
    MUTEX_LOCKER(ConnectionDataLock);

    if (statistics->_http) {
      if (statistics->_connStart != 0.0) {
        if (statistics->_connEnd == 0.0) {
          TRI_HttpConnectionsStatistics.incCounter();
        }
        else {
          TRI_HttpConnectionsStatistics.decCounter();

          double totalTime = statistics->_connEnd - statistics->_connStart;
          TRI_ConnectionTimeDistributionStatistics->addFigure(totalTime);
        }
      }
    }
  }

  // clear statistics
  memset(statistics, 0, sizeof(TRI_connection_statistics_t));

  // put statistics item back onto the freelist
  bool ok = ConnectionFreeList.push(statistics);
  TRI_ASSERT(ok);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the current statistics
////////////////////////////////////////////////////////////////////////////////

void TRI_FillConnectionStatistics (StatisticsCounter& httpConnections,
                                   StatisticsCounter& totalRequests,
                                   vector<StatisticsCounter>& methodRequests,
                                   StatisticsCounter& asyncRequests,
                                   StatisticsDistribution& connectionTime) {
  MUTEX_LOCKER(ConnectionDataLock);

  httpConnections = TRI_HttpConnectionsStatistics;
  totalRequests   = TRI_TotalRequestsStatistics;
  methodRequests  = TRI_MethodRequestsStatistics;
  asyncRequests   = TRI_AsyncRequestsStatistics;
  connectionTime  = *TRI_ConnectionTimeDistributionStatistics;
}

// -----------------------------------------------------------------------------
// --SECTION--                                public server statistics functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the global server statistics
////////////////////////////////////////////////////////////////////////////////

TRI_server_statistics_t TRI_GetServerStatistics () {
  TRI_server_statistics_t server;

  server._startTime = TRI_ServerStatistics._startTime;
  server._uptime    = TRI_microtime() - server._startTime;

  return server;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown flag
////////////////////////////////////////////////////////////////////////////////

static std::atomic<bool> Shutdown;

////////////////////////////////////////////////////////////////////////////////
/// @brief thread used for statistics
////////////////////////////////////////////////////////////////////////////////

static TRI_thread_t StatisticsThread;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the physical memory
////////////////////////////////////////////////////////////////////////////////

#if (defined(BSD) || defined(TRI_HAVE_MACOS_MEM_STATS))

static uint64_t GetPhysicalMemory () {
  int mib[2];
  int64_t physicalMemory;
  size_t length;

  // Get the Physical memory size
  mib[0] = CTL_HW;
#ifdef TRI_HAVE_MACOS_MEM_STATS
  mib[1] = HW_MEMSIZE;
#else
  mib[1] = HW_PHYSMEM; // The bytes of physical memory. (kenel + user space)
#endif
  length = sizeof(int64_t);
  sysctl(mib, 2, &physicalMemory, &length, nullptr, 0);

  return (uint64_t) physicalMemory;
}

#else
#ifdef TRI_HAVE_SC_PHYS_PAGES

static uint64_t GetPhysicalMemory () {
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);

  return (uint64_t)(pages * page_size);
}

#else
#ifdef TRI_HAVE_WIN32_GLOBAL_MEMORY_STATUS

static uint64_t GetPhysicalMemory () {
  MEMORYSTATUSEX status;
  status.dwLength = sizeof(status);
  GlobalMemoryStatusEx(&status);

  return (uint64_t) status.ullTotalPhys;
}

#else

static uint64_t TRI_GetPhysicalMemory () {
  PROCESS_MEMORY_COUNTERS  pmc;
  memset(&result, 0, sizeof(result));
  pmc.cb = sizeof(PROCESS_MEMORY_COUNTERS);
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ms684874(v=vs.85).aspx
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, pmc.cb)) {
    return pmc.PeakWorkingSetSize;
  }
  return 0;
}
#endif
#endif
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for new statistics and process them
////////////////////////////////////////////////////////////////////////////////

static void StatisticsQueueWorker (void* data) {
  while (! Shutdown && TRI_ENABLE_STATISTICS) {
    size_t count = ProcessAllRequestStatistics();

    if (count == 0) {
      usleep(100 * 1000);
    }
    else if (count < 10) {
      usleep(10 * 1000);
    }
    else if (count < 100) {
      usleep(1 * 1000);
    }
  }

  delete TRI_ConnectionTimeDistributionStatistics;
  delete TRI_TotalTimeDistributionStatistics;
  delete TRI_RequestTimeDistributionStatistics;
  delete TRI_QueueTimeDistributionStatistics;
  delete TRI_IoTimeDistributionStatistics;
  delete TRI_BytesSentDistributionStatistics;
  delete TRI_BytesReceivedDistributionStatistics;

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

// -----------------------------------------------------------------------------
// --SECTION--                                                   public variable
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics enabled flags
////////////////////////////////////////////////////////////////////////////////

bool TRI_ENABLE_STATISTICS = true;

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

////////////////////////////////////////////////////////////////////////////////
/// @brief physical memory
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_PhysicalMemory;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the current wallclock time
////////////////////////////////////////////////////////////////////////////////

double TRI_StatisticsTime () {
  return TRI_microtime();
}

// -----------------------------------------------------------------------------
// --SECTION--                                             module initialisation
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief module init function
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseStatistics () {
  TRI_ServerStatistics._startTime = TRI_microtime();
  TRI_PhysicalMemory = GetPhysicalMemory();

  // .............................................................................
  // sets up the statistics
  // .............................................................................

  TRI_ConnectionTimeDistributionVectorStatistics << (0.1) << (1.0) << (60.0);

  TRI_BytesSentDistributionVectorStatistics << (250) << (1000) << (2 * 1000) << (5 * 1000) << (10 * 1000);
  TRI_BytesReceivedDistributionVectorStatistics << (250) << (1000) << (2 * 1000) << (5 * 1000) << (10 * 1000);
  TRI_RequestTimeDistributionVectorStatistics << (0.01) << (0.05) << (0.1) << (0.2) << (0.5) << (1.0);

  TRI_ConnectionTimeDistributionStatistics = new StatisticsDistribution(TRI_ConnectionTimeDistributionVectorStatistics);
  TRI_TotalTimeDistributionStatistics = new StatisticsDistribution(TRI_RequestTimeDistributionVectorStatistics);
  TRI_RequestTimeDistributionStatistics = new StatisticsDistribution(TRI_RequestTimeDistributionVectorStatistics);
  TRI_QueueTimeDistributionStatistics = new StatisticsDistribution(TRI_RequestTimeDistributionVectorStatistics);
  TRI_IoTimeDistributionStatistics = new StatisticsDistribution(TRI_RequestTimeDistributionVectorStatistics);
  TRI_BytesSentDistributionStatistics = new StatisticsDistribution(TRI_BytesSentDistributionVectorStatistics);
  TRI_BytesReceivedDistributionStatistics = new StatisticsDistribution(TRI_BytesReceivedDistributionVectorStatistics);

  // initialise counters for all HTTP request types
  TRI_MethodRequestsStatistics.clear();

  for (int i = 0; i < ((int) triagens::rest::HttpRequest::HTTP_REQUEST_ILLEGAL) + 1; ++i) {
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

  // .............................................................................
  // use a separate thread for statistics
  // .............................................................................

  Shutdown = false;
  TRI_StartThread(&StatisticsThread, nullptr, "[statistics]", StatisticsQueueWorker, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut down statistics
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownStatistics (void) {
  Shutdown = true;
  int res = TRI_JoinThread(&StatisticsThread);
  TRI_ASSERT(res == 0);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
