////////////////////////////////////////////////////////////////////////////////
/// @brief statistics basics
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "BasicsC/win-utils.h"
#endif

#include "statistics.h"

#include "Basics/MutexLocker.h"
#include "Basics/StringBuffer.h"
#include "BasicsC/locks.h"
#include "BasicsC/threads.h"
#include "ResultGenerator/JsonResultGenerator.h"
#include "Statistics/RoundRobinFigures.h"
#include "Variant/VariantArray.h"
#include "Variant/VariantVector.h"


using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

#ifdef TRI_USE_SPIN_LOCK_STATISTICS
#define STATISTICS_INIT TRI_InitSpin
#define STATISTICS_DESTROY TRI_DestroySpin
#define STATISTICS_LOCK TRI_LockSpin
#define STATISTICS_UNLOCK TRI_UnlockSpin
#else
#define STATISTICS_INIT TRI_InitMutex
#define STATISTICS_DESTROY TRI_DestroyMutex
#define STATISTICS_LOCK TRI_LockMutex
#define STATISTICS_UNLOCK TRI_UnlockMutex
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Statistics
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief request statistics description
////////////////////////////////////////////////////////////////////////////////


struct StatisticsDesc {

#ifdef TRI_ENABLE_HIRES_FIGURES
  RRF_DISTRIBUTION(StatisticsDesc, total, (0.0001) << (0.05) << (0.1) << (0.2) << (0.5) << (1.0));
#else
  RRF_DISTRIBUTION(StatisticsDesc, total, (0.01) << (0.05) << (0.1) << (0.2) << (0.5) << (1.0));
#endif


#ifdef TRI_ENABLE_HIRES_FIGURES
  RRF_DISTRIBUTION(StatisticsDesc, queue,(0.0001) << (0.05) << (0.1) << (0.2) << (0.5) << (1.0));
#else
  RRF_DISTRIBUTION(StatisticsDesc, queue, (0.01) << (0.05) << (0.1) << (0.2) << (0.5) << (1.0));
#endif


#ifdef TRI_ENABLE_HIRES_FIGURES
  RRF_DISTRIBUTION(StatisticsDesc, request, (0.0001) << (0.05) << (0.1) << (0.2) << (0.5) << (1.0));
#else
  RRF_DISTRIBUTION(StatisticsDesc, request, (0.01) << (0.05) << (0.1) << (0.2) << (0.5) << (1.0));
#endif


  RRF_DISTRIBUTION(StatisticsDesc, bytesSent, (250) << (1000) << (2 * 1000) << (5 * 1000) << (10 * 1000));

  RRF_DISTRIBUTION(StatisticsDesc, bytesReceived, (250) << (1000) << (2 * 1000) << (5 * 1000) << (10 * 1000));

  RRF_COUNTER(StatisticsDesc, httpConnections);

  RRF_DISTRIBUTION(StatisticsDesc, httpDuration, (0.1) << (1.0) << (60.0));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief request statistics figures
////////////////////////////////////////////////////////////////////////////////

struct StatisticsFigures {
  Mutex _lock;

  triagens::basics::RoundRobinFigures<60, 120, StatisticsDesc> _minute;
  triagens::basics::RoundRobinFigures<60 * 60, 48, StatisticsDesc> _hour;
  triagens::basics::RoundRobinFigures<24 * 60 * 60, 5, StatisticsDesc> _day;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Statistics
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics figures
////////////////////////////////////////////////////////////////////////////////

static StatisticsFigures Statistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics running
////////////////////////////////////////////////////////////////////////////////

static volatile sig_atomic_t StatisticsRunning = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics thread
////////////////////////////////////////////////////////////////////////////////

#if TRI_ENABLE_FIGURES
static TRI_thread_t StatisticsThread;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics time lock
////////////////////////////////////////////////////////////////////////////////

static TRI_spin_t StatisticsTimeLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics time
////////////////////////////////////////////////////////////////////////////////

static volatile double StatisticsTime;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                               private request statistics ariables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Statistics
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for lists
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_USE_SPIN_LOCK_STATISTICS
static TRI_spin_t RequestListLock;
#else
static TRI_mutex_t RequestListLock;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief free list
////////////////////////////////////////////////////////////////////////////////

static TRI_statistics_list_t RequestFreeList;

////////////////////////////////////////////////////////////////////////////////
/// @brief dirty list
////////////////////////////////////////////////////////////////////////////////

static TRI_statistics_list_t RequestDirtyList;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                               public request statistics functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Statistics
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a new statistics block
////////////////////////////////////////////////////////////////////////////////

TRI_request_statistics_t* TRI_AcquireRequestStatistics () {
  TRI_request_statistics_t* statistics = 0;

  STATISTICS_LOCK(&RequestListLock);

  if (RequestFreeList._first != NULL) {
    statistics = (TRI_request_statistics_t*) RequestFreeList._first;
    RequestFreeList._first = RequestFreeList._first->_next;
  }
  else if (RequestDirtyList._first != NULL) {
    statistics = (TRI_request_statistics_t*) RequestDirtyList._first;
    RequestDirtyList._first = RequestDirtyList._first->_next;

    memset(statistics, 0, sizeof(TRI_request_statistics_t));
  }

  STATISTICS_UNLOCK(&RequestListLock);

  return statistics;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a statistics block
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseRequestStatistics (TRI_request_statistics_t* statistics) {
  STATISTICS_LOCK(&RequestListLock);

  if (RequestDirtyList._first == NULL) {
    RequestDirtyList._first = (TRI_statistics_entry_t*) statistics;
    RequestDirtyList._last = (TRI_statistics_entry_t*) statistics;
  }
  else {
    RequestDirtyList._last->_next = (TRI_statistics_entry_t*) statistics;
    RequestDirtyList._last = (TRI_statistics_entry_t*) statistics;
  }

  RequestDirtyList._last->_next = 0;

  STATISTICS_UNLOCK(&RequestListLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                            private connection statistics ariables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Statistics
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for lists
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_USE_SPIN_LOCK_STATISTICS
static TRI_spin_t ConnectionListLock;
#else
static TRI_mutex_t ConnectionListLock;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief free list
////////////////////////////////////////////////////////////////////////////////

static TRI_statistics_list_t ConnectionFreeList;

////////////////////////////////////////////////////////////////////////////////
/// @brief dirty list
////////////////////////////////////////////////////////////////////////////////

static TRI_statistics_list_t ConnectionDirtyList;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                               public connection statistics functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Statistics
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a new statistics block
////////////////////////////////////////////////////////////////////////////////

TRI_connection_statistics_t* TRI_AcquireConnectionStatistics () {
  TRI_connection_statistics_t* statistics = 0;

  STATISTICS_LOCK(&ConnectionListLock);

  if (ConnectionFreeList._first != NULL) {
    statistics = (TRI_connection_statistics_t*) ConnectionFreeList._first;
    ConnectionFreeList._first = ConnectionFreeList._first->_next;
  }
  else if (ConnectionDirtyList._first != NULL) {
    statistics = (TRI_connection_statistics_t*) ConnectionDirtyList._first;
    ConnectionDirtyList._first = ConnectionDirtyList._first->_next;

    memset(statistics, 0, sizeof(TRI_connection_statistics_t));
  }

  STATISTICS_UNLOCK(&ConnectionListLock);

  return statistics;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a statistics block
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseConnectionStatistics (TRI_connection_statistics_t* statistics) {
  STATISTICS_LOCK(&ConnectionListLock);

  if (ConnectionDirtyList._first == NULL) {
    ConnectionDirtyList._first = (TRI_statistics_entry_t*) statistics;
    ConnectionDirtyList._last = (TRI_statistics_entry_t*) statistics;
  }
  else {
    ConnectionDirtyList._last->_next = (TRI_statistics_entry_t*) statistics;
    ConnectionDirtyList._last = (TRI_statistics_entry_t*) statistics;
  }

  ConnectionDirtyList._last->_next = 0;

  STATISTICS_UNLOCK(&ConnectionListLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Statistics
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the request statistics
////////////////////////////////////////////////////////////////////////////////

void UpdateRequestStatistics (double now) {
  TRI_request_statistics_t* statistics ;

  STATISTICS_LOCK(&RequestListLock);

  while (RequestDirtyList._first != NULL) {
    statistics = (TRI_request_statistics_t*) RequestDirtyList._first;
    RequestDirtyList._first = RequestDirtyList._first->_next;

    STATISTICS_UNLOCK(&RequestListLock);

    // check the request was completely received and transmitted
    if (statistics->_readStart != 0.0 && statistics->_writeEnd != 0.0) {
      MUTEX_LOCKER(Statistics._lock);

      double totalTime = statistics->_writeEnd - statistics->_readStart;

      Statistics._minute.addDistribution<StatisticsDesc::totalAccessor>(totalTime);
      Statistics._hour.addDistribution<StatisticsDesc::totalAccessor>(totalTime);
      Statistics._day.addDistribution<StatisticsDesc::totalAccessor>(totalTime);

      double requestTime = statistics->_requestEnd - statistics->_requestStart;

      Statistics._minute.addDistribution<StatisticsDesc::requestAccessor>(requestTime);
      Statistics._hour.addDistribution<StatisticsDesc::requestAccessor>(requestTime);
      Statistics._day.addDistribution<StatisticsDesc::requestAccessor>(requestTime);

      if (statistics->_queueStart != 0.0 && statistics->_queueEnd != 0.0) {
        double queueTime = statistics->_queueEnd - statistics->_queueStart;

        Statistics._minute.addDistribution<StatisticsDesc::queueAccessor>(queueTime);
        Statistics._hour.addDistribution<StatisticsDesc::queueAccessor>(queueTime);
        Statistics._day.addDistribution<StatisticsDesc::queueAccessor>(queueTime);
      }

      Statistics._minute.addDistribution<StatisticsDesc::bytesSentAccessor>(statistics->_sentBytes);
      Statistics._hour.addDistribution<StatisticsDesc::bytesSentAccessor>(statistics->_sentBytes);
      Statistics._day.addDistribution<StatisticsDesc::bytesSentAccessor>(statistics->_sentBytes);

      Statistics._minute.addDistribution<StatisticsDesc::bytesReceivedAccessor>(statistics->_receivedBytes);
      Statistics._hour.addDistribution<StatisticsDesc::bytesReceivedAccessor>(statistics->_receivedBytes);
      Statistics._day.addDistribution<StatisticsDesc::bytesReceivedAccessor>(statistics->_receivedBytes);
    }

    // clear the old data
    memset(statistics, 0, sizeof(TRI_request_statistics_t));

    // put statistic block back onto the free list
    STATISTICS_LOCK(&RequestListLock);

    if (RequestFreeList._first == NULL) {
      RequestFreeList._first = (TRI_statistics_entry_t*) statistics;
      RequestFreeList._last = (TRI_statistics_entry_t*) statistics;
    }
    else {
      RequestFreeList._last->_next = (TRI_statistics_entry_t*) statistics;
      RequestFreeList._last = (TRI_statistics_entry_t*) statistics;
    }
  }

  STATISTICS_UNLOCK(&RequestListLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the connection statistics
////////////////////////////////////////////////////////////////////////////////

void UpdateConnectionStatistics (double now) {
  TRI_connection_statistics_t* statistics ;

  STATISTICS_LOCK(&ConnectionListLock);

  while (ConnectionDirtyList._first != NULL) {
    statistics = (TRI_connection_statistics_t*) ConnectionDirtyList._first;
    ConnectionDirtyList._first = ConnectionDirtyList._first->_next;

    STATISTICS_UNLOCK(&ConnectionListLock);

    {
      MUTEX_LOCKER(Statistics._lock);

      if (statistics->_connStart != 0) {
        if (statistics->_connEnd == 0) {
          Statistics._minute.incCounter<StatisticsDesc::httpConnectionsAccessor>();
          Statistics._hour.incCounter<StatisticsDesc::httpConnectionsAccessor>();
          Statistics._day.incCounter<StatisticsDesc::httpConnectionsAccessor>();
        }
        else {
          double totalTime = statistics->_connEnd - statistics->_connStart;

          Statistics._minute.addDistribution<StatisticsDesc::httpDurationAccessor>(totalTime);
          Statistics._hour.addDistribution<StatisticsDesc::httpDurationAccessor>(totalTime);
          Statistics._day.addDistribution<StatisticsDesc::httpDurationAccessor>(totalTime);
        }
      }
    }

    // clear the old data
    memset(statistics, 0, sizeof(TRI_connection_statistics_t));

    // put statistic block back onto the free list
    STATISTICS_LOCK(&ConnectionListLock);

    if (ConnectionFreeList._first == NULL) {
      ConnectionFreeList._first = (TRI_statistics_entry_t*) statistics;
      ConnectionFreeList._last = (TRI_statistics_entry_t*) statistics;
    }
    else {
      ConnectionFreeList._last->_next = (TRI_statistics_entry_t*) statistics;
      ConnectionFreeList._last = (TRI_statistics_entry_t*) statistics;
    }
  }

  STATISTICS_UNLOCK(&ConnectionListLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief worker for statistics
////////////////////////////////////////////////////////////////////////////////

#if TRI_ENABLE_FIGURES

static void StatisticsLoop (void* data) {
  while (StatisticsRunning != 0) {
    double t = TRI_microtime();

    TRI_LockSpin(&StatisticsTimeLock);
    StatisticsTime = t;
    TRI_UnlockSpin(&StatisticsTimeLock);

    UpdateRequestStatistics(StatisticsTime);
    UpdateConnectionStatistics(StatisticsTime);

    usleep(500);
  }
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Statistics
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a statistics list
////////////////////////////////////////////////////////////////////////////////

VariantArray* TRI_StatisticsInfo (TRI_statistics_granularity_e granularity,
                                  size_t limit,
                                  bool showTotalTime,
                                  bool showQueueTime,
                                  bool showRequestTime,
                                  bool showBytesSent,
                                  bool showBytesReceived,
                                  bool showHttp) {
  vector<time_t> t;
  vector<StatisticsDesc> blocks;

  // .............................................................................
  // extract the statistics blocks for the given granularity
  // .............................................................................

  size_t resolution = 0;
  size_t total = 0;

  {
    MUTEX_LOCKER(Statistics._lock);

    switch (granularity) {
      case TRI_STATISTICS_SECONDS:
        // not used/implemented, yet
        break;

      case TRI_STATISTICS_MINUTES:
        resolution = Statistics._minute.getResolution();
        total = Statistics._minute.getLength();

        if (limit == (size_t) -1) {
          blocks = Statistics._minute.values(t);
        }
        else if (limit == 0) {
          blocks = Statistics._minute.values(t, 1);
        }
        else {
          blocks = Statistics._minute.values(t, limit);
        }

        break;

      case TRI_STATISTICS_HOURS:
        resolution = Statistics._hour.getResolution();
        total = Statistics._hour.getLength();
        
        if (limit == (size_t) -1) {
          blocks = Statistics._hour.values(t);
        }
        else if (limit == 0) {
          blocks = Statistics._hour.values(t, 1);
        }
        else {
          blocks = Statistics._hour.values(t, limit);
        }

        break;

      case TRI_STATISTICS_DAYS:
        resolution = Statistics._day.getResolution();
        total = Statistics._day.getLength();
        
        if (limit == (size_t) -1) {
          blocks = Statistics._day.values(t);
        }
        else if (limit == 0) {
          blocks = Statistics._day.values(t, 1);
        }
        else {
          blocks = Statistics._day.values(t, limit);
        }
        
        break;
    }
  }

  // .............................................................................
  // CASE 1: generate the current aka last block only
  // .............................................................................

  VariantArray* result = new VariantArray();

  result->add("resolution", new VariantUInt64(resolution));

  if (blocks.empty()) {
    return result;
  }

  if (limit == 0) {
    result->add("start", new VariantUInt32((uint32_t) t[0]));

    if (showTotalTime) {
      RRF_GenerateVariantDistribution<StatisticsDesc::totalAccessor>(result, blocks[0], "totalTime", true, true, false);
    }

    if (showQueueTime) {
      RRF_GenerateVariantDistribution<StatisticsDesc::queueAccessor>(result, blocks[0], "queueTime", true, true, false);
    }

    if (showRequestTime) {
      RRF_GenerateVariantDistribution<StatisticsDesc::requestAccessor>(result, blocks[0], "requestTime", true, true, false);
    }

    if (showBytesSent) {
      RRF_GenerateVariantDistribution<StatisticsDesc::bytesSentAccessor>(result, blocks[0], "bytesSent", true, true, false);
    }

    if (showBytesReceived) {
      RRF_GenerateVariantDistribution<StatisticsDesc::bytesReceivedAccessor>(result, blocks[0], "bytesReceived", true, true, false);
    }

    if (showHttp) {
      RRF_GenerateVariantCounter<StatisticsDesc::httpConnectionsAccessor>(result, blocks[0], "httpConnections", resolution);
      RRF_GenerateVariantDistribution<StatisticsDesc::httpDurationAccessor>(result, blocks[0], "httpDuration", true, true, false);
    }
  }

  // .............................................................................
  // CASE 2: generate all blocks upto limit
  // .............................................................................

  else {
    VariantVector* start = new VariantVector();
    result->add("start", start);

    size_t offset = 0;
    std::vector<time_t>::const_iterator k = t.begin();

    for (;  k != t.end();  ++k, ++offset) {
      if (*k != 0) {
        break;
      }
    }

    for (;  k != t.end();  ++k) {
      start->add(new VariantUInt64(*k));
    }

    if (0 < offset) {
      vector<StatisticsDesc> tmp;

      tmp.insert(tmp.begin(), blocks.begin() + offset, blocks.end());
      blocks.swap(tmp);
    }

    result->add("length", new VariantUInt64(blocks.size()));;
    result->add("totalLength", new VariantUInt64(total));

    if (showTotalTime) {
      RRF_GenerateVariantDistribution<StatisticsDesc::totalAccessor>(result, blocks, "totalTime", true, true, false);
    }

    if (showQueueTime) {
      RRF_GenerateVariantDistribution<StatisticsDesc::queueAccessor>(result, blocks, "queueTime", true, true, false);
    }

    if (showRequestTime) {
      RRF_GenerateVariantDistribution<StatisticsDesc::requestAccessor>(result, blocks, "requestTime", true, true, false);
    }

    if (showBytesSent) {
      RRF_GenerateVariantDistribution<StatisticsDesc::bytesSentAccessor>(result, blocks, "bytesSent", true, true, false);
    }

    if (showBytesReceived) {
      RRF_GenerateVariantDistribution<StatisticsDesc::bytesReceivedAccessor>(result, blocks, "bytesReceived", true, true, false);
    }

    if (showHttp) {
      RRF_GenerateVariantCounter<StatisticsDesc::httpConnectionsAccessor>(result, blocks, "httpConnections", resolution);
      RRF_GenerateVariantDistribution<StatisticsDesc::httpDurationAccessor>(result, blocks, "httpDuration", true, true, false);
    }
  }

  return result;
}

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
#ifdef TRI_USE_TIME_FIGURES

double TRI_StatisticsTime () {
  double result;

  TRI_LockSpin(&StatisticsTimeLock);
  result = StatisticsTime;
  TRI_UnlockSpin(&StatisticsTimeLock);

  return result;
}

#else

double TRI_StatisticsTime () { 
  return (double)(time(0));
}

#endif
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief fills a linked list
////////////////////////////////////////////////////////////////////////////////

void TRI_FillStatisticsList (TRI_statistics_list_t* list, size_t element, size_t count) {
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
/// @brief disable the statistics gathering
////////////////////////////////////////////////////////////////////////////////

void TRI_DisableStatistics () {
  StatisticsRunning = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief module init function
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseStatistics () {
#if TRI_ENABLE_FIGURES

  static size_t const QUEUE_SIZE = 1000;

  // .............................................................................
  // generate the request statistics queue
  // .............................................................................

  RequestFreeList._first = RequestFreeList._last = NULL;
  RequestDirtyList._first = RequestDirtyList._last = NULL;

  TRI_FillStatisticsList(&RequestFreeList, sizeof(TRI_request_statistics_t), QUEUE_SIZE);

  STATISTICS_INIT(&RequestListLock);

  // .............................................................................
  // generate the connection statistics queue
  // .............................................................................

  ConnectionFreeList._first = ConnectionFreeList._last = NULL;
  ConnectionDirtyList._first = ConnectionDirtyList._last = NULL;

  TRI_FillStatisticsList(&ConnectionFreeList, sizeof(TRI_connection_statistics_t), QUEUE_SIZE);

  STATISTICS_INIT(&ConnectionListLock);

  // .............................................................................
  // start the statistics thread
  // .............................................................................

  TRI_InitSpin(&StatisticsTimeLock);

  TRI_InitThread(&StatisticsThread);
  TRI_StartThread(&StatisticsThread, "[statistics]", StatisticsLoop, 0);

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
