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

#include "Basics/locks.h"

#ifdef TRI_HAVE_MACOS_MEM_STATS
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

using namespace triagens::basics;
using namespace std;

#ifdef TRI_USE_SPIN_LOCK_STATISTICS
#define STATISTICS_TYPE TRI_spin_t
#define STATISTICS_INIT TRI_InitSpin
#define STATISTICS_DESTROY TRI_DestroySpin
#define STATISTICS_LOCK TRI_LockSpin
#define STATISTICS_UNLOCK TRI_UnlockSpin
#else
#define STATISTICS_TYPE TRI_mutex_t
#define STATISTICS_INIT TRI_InitMutex
#define STATISTICS_DESTROY TRI_DestroyMutex
#define STATISTICS_LOCK TRI_LockMutex
#define STATISTICS_UNLOCK TRI_UnlockMutex
#endif

// -----------------------------------------------------------------------------
// --SECTION--                              private request statistics variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for lists
////////////////////////////////////////////////////////////////////////////////

static STATISTICS_TYPE RequestListLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief free list
////////////////////////////////////////////////////////////////////////////////

static TRI_statistics_list_t RequestFreeList;

// -----------------------------------------------------------------------------
// --SECTION--                               public request statistics functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a new statistics block
////////////////////////////////////////////////////////////////////////////////

TRI_request_statistics_t* TRI_AcquireRequestStatistics () {
  TRI_request_statistics_t* statistics = nullptr;

  STATISTICS_LOCK(&RequestListLock);

  if (RequestFreeList._first != nullptr) {
    statistics = (TRI_request_statistics_t*) RequestFreeList._first;
    RequestFreeList._first = RequestFreeList._first->_next;
  }

  STATISTICS_UNLOCK(&RequestListLock);

  return statistics;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a statistics block
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseRequestStatistics (TRI_request_statistics_t* statistics) {
  STATISTICS_LOCK(&RequestListLock);

  if (statistics == nullptr) {
    STATISTICS_UNLOCK(&RequestListLock);
    return;
  }

  if (! statistics->_ignore) {
    TRI_TotalRequestsStatistics.incCounter();

    if (statistics->_async) {
      TRI_AsyncRequestsStatistics.incCounter();
    }

    TRI_MethodRequestsStatistics[(int) statistics->_requestType].incCounter();

    // check the request was completely received and transmitted
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

  // clear statistics and put back an the free list
  memset(statistics, 0, sizeof(TRI_request_statistics_t));
  statistics->_requestType = triagens::rest::HttpRequest::HTTP_REQUEST_ILLEGAL;

  if (RequestFreeList._first == nullptr) {
    RequestFreeList._first = (TRI_statistics_entry_t*) statistics;
    RequestFreeList._last = (TRI_statistics_entry_t*) statistics;
  }
  else {
    RequestFreeList._last->_next = (TRI_statistics_entry_t*) statistics;
    RequestFreeList._last = (TRI_statistics_entry_t*) statistics;
  }

  RequestFreeList._last->_next = 0;

  STATISTICS_UNLOCK(&RequestListLock);
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
  STATISTICS_LOCK(&RequestListLock);

  totalTime = *TRI_TotalTimeDistributionStatistics;
  requestTime = *TRI_RequestTimeDistributionStatistics;
  queueTime = *TRI_QueueTimeDistributionStatistics;
  ioTime = *TRI_IoTimeDistributionStatistics;
  bytesSent = *TRI_BytesSentDistributionStatistics;
  bytesReceived = *TRI_BytesReceivedDistributionStatistics;

  STATISTICS_UNLOCK(&RequestListLock);
}

// -----------------------------------------------------------------------------
// --SECTION--                           private connection statistics variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for lists
////////////////////////////////////////////////////////////////////////////////

static STATISTICS_TYPE ConnectionListLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief free list
////////////////////////////////////////////////////////////////////////////////

static TRI_statistics_list_t ConnectionFreeList;

// -----------------------------------------------------------------------------
// --SECTION--                            public connection statistics functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a new statistics block
////////////////////////////////////////////////////////////////////////////////

TRI_connection_statistics_t* TRI_AcquireConnectionStatistics () {
  TRI_connection_statistics_t* statistics = 0;

  STATISTICS_LOCK(&ConnectionListLock);

  if (ConnectionFreeList._first != nullptr) {
    statistics = (TRI_connection_statistics_t*) ConnectionFreeList._first;
    ConnectionFreeList._first = ConnectionFreeList._first->_next;
  }

  STATISTICS_UNLOCK(&ConnectionListLock);

  return statistics;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a statistics block
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseConnectionStatistics (TRI_connection_statistics_t* statistics) {
  STATISTICS_LOCK(&ConnectionListLock);

  if (statistics == nullptr) {
    STATISTICS_UNLOCK(&ConnectionListLock);
    return;
  }

  if (statistics->_http) {
    if (statistics->_connStart != 0) {
      if (statistics->_connEnd == 0) {
        TRI_HttpConnectionsStatistics.incCounter();
      }
      else {
        TRI_HttpConnectionsStatistics.decCounter();

        double totalTime = statistics->_connEnd - statistics->_connStart;
        TRI_ConnectionTimeDistributionStatistics->addFigure(totalTime);
      }
    }
  }

  // clear statistics and put back an the free list
  memset(statistics, 0, sizeof(TRI_connection_statistics_t));

  if (ConnectionFreeList._first == nullptr) {
    ConnectionFreeList._first = (TRI_statistics_entry_t*) statistics;
    ConnectionFreeList._last = (TRI_statistics_entry_t*) statistics;
  }
  else {
    ConnectionFreeList._last->_next = (TRI_statistics_entry_t*) statistics;
    ConnectionFreeList._last = (TRI_statistics_entry_t*) statistics;
  }

  ConnectionFreeList._last->_next = nullptr;

  STATISTICS_UNLOCK(&ConnectionListLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the current statistics
////////////////////////////////////////////////////////////////////////////////

void TRI_FillConnectionStatistics (StatisticsCounter& httpConnections,
                                   StatisticsCounter& totalRequests,
                                   vector<StatisticsCounter>& methodRequests,
                                   StatisticsCounter& asyncRequests,
                                   StatisticsDistribution& connectionTime) {
  STATISTICS_LOCK(&ConnectionListLock);

  httpConnections = TRI_HttpConnectionsStatistics;
  totalRequests   = TRI_TotalRequestsStatistics;
  methodRequests  = TRI_MethodRequestsStatistics;
  asyncRequests   = TRI_AsyncRequestsStatistics;
  connectionTime  = *TRI_ConnectionTimeDistributionStatistics;

  STATISTICS_UNLOCK(&ConnectionListLock);
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
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief fills a linked list
////////////////////////////////////////////////////////////////////////////////

static void FillStatisticsList (TRI_statistics_list_t* list, size_t element, size_t count) {
  size_t i;
  TRI_statistics_entry_t* entry;

  entry = (TRI_statistics_entry_t*) TRI_Allocate(TRI_CORE_MEM_ZONE, element, true);

  list->_first = entry;
  list->_last = entry;

  for (i = 1;  i < count;  ++i) {
    entry = (TRI_statistics_entry_t*) TRI_Allocate(TRI_CORE_MEM_ZONE, element, true);

    list->_last->_next = entry;
    list->_last = entry;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a linked list
////////////////////////////////////////////////////////////////////////////////

static void DestroyStatisticsList (TRI_statistics_list_t* list) {
  TRI_statistics_entry_t* entry = list->_first;
  while (entry != nullptr) {
    TRI_statistics_entry_t* next = entry->_next;
    TRI_Free(TRI_CORE_MEM_ZONE, entry);
    entry = next;
  }

  list->_first = list->_last = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the physical memory
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_MACOS_MEM_STATS

static uint64_t GetPhysicalMemory () {
  int mib[2];
  int64_t physicalMemory;
  size_t length;

  // Get the Physical memory size
  mib[0] = CTL_HW;
  mib[1] = HW_MEMSIZE;
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
/// @brief physical memeory
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_PhysicalMemory;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the current wallclock time
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_HIRES_FIGURES

double TRI_StatisticsTime () {
  struct timespec tp;

  clock_gettime(CLOCK_REALTIME, &tp);

  return tp.tv_sec + (tp.tv_nsec / 1000000000.0);
}

#else

double TRI_StatisticsTime () {
  return TRI_microtime();
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                             module initialisation
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief module init function
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseStatistics () {
  TRI_ServerStatistics._startTime = TRI_microtime();
  TRI_PhysicalMemory = GetPhysicalMemory();

#if TRI_ENABLE_FIGURES

  static size_t const QUEUE_SIZE = 1000;

  // .............................................................................
  // sets up the statistics
  // .............................................................................

  TRI_ConnectionTimeDistributionVectorStatistics << (0.1) << (1.0) << (60.0);

  TRI_BytesSentDistributionVectorStatistics << (250) << (1000) << (2 * 1000) << (5 * 1000) << (10 * 1000);
  TRI_BytesReceivedDistributionVectorStatistics << (250) << (1000) << (2 * 1000) << (5 * 1000) << (10 * 1000);

#ifdef TRI_ENABLE_HIRES_FIGURES
  TRI_RequestTimeDistributionVectorStatistics << (0.0001) << (0.05) << (0.1) << (0.2) << (0.5) << (1.0);
#else
  TRI_RequestTimeDistributionVectorStatistics << (0.01) << (0.05) << (0.1) << (0.2) << (0.5) << (1.0);
#endif

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
    TRI_MethodRequestsStatistics.push_back(c);
  }

  // .............................................................................
  // generate the request statistics queue
  // .............................................................................

  RequestFreeList._first = RequestFreeList._last = nullptr;

  FillStatisticsList(&RequestFreeList, sizeof(TRI_request_statistics_t), QUEUE_SIZE);

  STATISTICS_INIT(&RequestListLock);

  // .............................................................................
  // generate the connection statistics queue
  // .............................................................................

  ConnectionFreeList._first = ConnectionFreeList._last = nullptr;

  FillStatisticsList(&ConnectionFreeList, sizeof(TRI_connection_statistics_t), QUEUE_SIZE);

  STATISTICS_INIT(&ConnectionListLock);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut down statistics
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownStatistics (void) {
#if TRI_ENABLE_FIGURES
  delete TRI_ConnectionTimeDistributionStatistics;
  delete TRI_TotalTimeDistributionStatistics;
  delete TRI_RequestTimeDistributionStatistics;
  delete TRI_QueueTimeDistributionStatistics;
  delete TRI_IoTimeDistributionStatistics;
  delete TRI_BytesSentDistributionStatistics;
  delete TRI_BytesReceivedDistributionStatistics;

  DestroyStatisticsList(&RequestFreeList);
  DestroyStatisticsList(&ConnectionFreeList);
#endif
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
