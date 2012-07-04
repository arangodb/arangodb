////////////////////////////////////////////////////////////////////////////////
/// @brief request statistics
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

#include "request-statistics.h"

#include "Basics/MutexLocker.h"
#include "Basics/RoundRobinFigures.h"
#include "Basics/StringBuffer.h"
#include "BasicsC/locks.h"
#include "ResultGenerator/JsonResultGenerator.h"
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

struct RequestStatisticsDesc {
  RRF_DISTRIBUTION(RequestStatisticsDesc,
                   total,
                   (0.0001) << (0.01) << (0.05) << (0.1) << (0.2) << (0.5) << (1.0));

  RRF_DISTRIBUTION(RequestStatisticsDesc,
                   queue,
                   (0.0001) << (0.01) << (0.05) << (0.1) << (0.2) << (0.5) << (1.0));

  RRF_DISTRIBUTION(RequestStatisticsDesc,
                   request,
                   (0.0001) << (0.01) << (0.05) << (0.1) << (0.2) << (0.5) << (1.0));

  RRF_DISTRIBUTION(RequestStatisticsDesc,
                   bytesSent,
                   (250) << (1000) << (2 * 1000) << (5 * 1000) << (10 * 1000));

  RRF_DISTRIBUTION(RequestStatisticsDesc,
                   bytesReceived,
                   (250) << (1000) << (2 * 1000) << (5 * 1000) << (10 * 1000));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief request statistics figures
////////////////////////////////////////////////////////////////////////////////

struct RequestStatisticsFigures {
  Mutex _lock;

  triagens::basics::RoundRobinFigures<60, 120, RequestStatisticsDesc> _minute;
  triagens::basics::RoundRobinFigures<60 * 60, 48, RequestStatisticsDesc> _hour;
  triagens::basics::RoundRobinFigures<24 * 60 * 60, 5, RequestStatisticsDesc> _day;
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
/// @brief request statistics figures
////////////////////////////////////////////////////////////////////////////////

static RequestStatisticsFigures RequestStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for lists
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_USE_SPIN_LOCK_STATISTICS
static TRI_spin_t ListLock;
#else
static TRI_mutex_t ListLock;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief free list
////////////////////////////////////////////////////////////////////////////////

static TRI_statistics_list_t FreeList;

////////////////////////////////////////////////////////////////////////////////
/// @brief dirty list
////////////////////////////////////////////////////////////////////////////////

static TRI_statistics_list_t DirtyList;

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
/// @brief gets a new statistics block
////////////////////////////////////////////////////////////////////////////////

TRI_request_statistics_t* TRI_AcquireRequestStatistics () {
  TRI_request_statistics_t* statistics = 0;

  STATISTICS_LOCK(&ListLock);

  if (FreeList._first != NULL) {
    statistics = (TRI_request_statistics_t*) FreeList._first;
    FreeList._first = FreeList._first->_next;
  }
  else if (DirtyList._first != NULL) {
    statistics = (TRI_request_statistics_t*) DirtyList._first;
    DirtyList._first = DirtyList._first->_next;

    memset(statistics, 0, sizeof(TRI_request_statistics_t));
  }

  STATISTICS_UNLOCK(&ListLock);

  return statistics;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a statistics block
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseRequestStatistics (TRI_request_statistics_t* statistics) {
  STATISTICS_LOCK(&ListLock);

  if (DirtyList._first == NULL) {
    DirtyList._first = (TRI_statistics_entry_t*) statistics;
    DirtyList._last = (TRI_statistics_entry_t*) statistics;
  }
  else {
    DirtyList._last->_next = (TRI_statistics_entry_t*) statistics;
    DirtyList._last = (TRI_statistics_entry_t*) statistics;
  }

  DirtyList._last->_next = 0;

  STATISTICS_UNLOCK(&ListLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the request statistics
////////////////////////////////////////////////////////////////////////////////

void TRI_UpdateRequestStatistics (double now) {
  TRI_request_statistics_t* statistics ;

  STATISTICS_LOCK(&ListLock);

  while (DirtyList._first != NULL) {
    statistics = (TRI_request_statistics_t*) DirtyList._first;
    DirtyList._first = DirtyList._first->_next;

    STATISTICS_UNLOCK(&ListLock);

    // check the request was completely received and transmitted
    if (statistics->_readStart != 0.0 && statistics->_writeEnd != 0.0) {
      MUTEX_LOCKER(RequestStatistics._lock);

      double totalTime = statistics->_writeEnd - statistics->_readStart;

      RequestStatistics._minute.addDistribution<RequestStatisticsDesc::totalAccessor>(totalTime);
      RequestStatistics._hour.addDistribution<RequestStatisticsDesc::totalAccessor>(totalTime);
      RequestStatistics._day.addDistribution<RequestStatisticsDesc::totalAccessor>(totalTime);

      double requestTime = statistics->_requestEnd - statistics->_requestStart;

      RequestStatistics._minute.addDistribution<RequestStatisticsDesc::requestAccessor>(requestTime);
      RequestStatistics._hour.addDistribution<RequestStatisticsDesc::requestAccessor>(requestTime);
      RequestStatistics._day.addDistribution<RequestStatisticsDesc::requestAccessor>(requestTime);

      if (statistics->_queueStart != 0.0 && statistics->_queueEnd != 0.0) {
        double queueTime = statistics->_queueEnd - statistics->_queueStart;

        RequestStatistics._minute.addDistribution<RequestStatisticsDesc::queueAccessor>(queueTime);
        RequestStatistics._hour.addDistribution<RequestStatisticsDesc::queueAccessor>(queueTime);
        RequestStatistics._day.addDistribution<RequestStatisticsDesc::queueAccessor>(queueTime);
      }

      RequestStatistics._minute.addDistribution<RequestStatisticsDesc::bytesSentAccessor>(statistics->_sentBytes);
      RequestStatistics._hour.addDistribution<RequestStatisticsDesc::bytesSentAccessor>(statistics->_sentBytes);
      RequestStatistics._day.addDistribution<RequestStatisticsDesc::bytesSentAccessor>(statistics->_sentBytes);

      RequestStatistics._minute.addDistribution<RequestStatisticsDesc::bytesReceivedAccessor>(statistics->_receivedBytes);
      RequestStatistics._hour.addDistribution<RequestStatisticsDesc::bytesReceivedAccessor>(statistics->_receivedBytes);
      RequestStatistics._day.addDistribution<RequestStatisticsDesc::bytesReceivedAccessor>(statistics->_receivedBytes);
    }

    // clear the old data
    memset(statistics, 0, sizeof(TRI_request_statistics_t));

    // put statistic block back onto the free list
    STATISTICS_LOCK(&ListLock);

    if (FreeList._first == NULL) {
      FreeList._first = (TRI_statistics_entry_t*) statistics;
      FreeList._last = (TRI_statistics_entry_t*) statistics;
    }
    else {
      FreeList._last->_next = (TRI_statistics_entry_t*) statistics;
      FreeList._last = (TRI_statistics_entry_t*) statistics;
    }
  }

  STATISTICS_UNLOCK(&ListLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a statistics list
////////////////////////////////////////////////////////////////////////////////

VariantArray* TRI_RequestStatistics (TRI_request_statistics_granularity_e granularity,
                                     size_t limit,
                                     bool showTotalTime,
                                     bool showQueueTime,
                                     bool showRequestTime,
                                     bool showBytesSent,
                                     bool showBytesReceived) {
  vector<time_t> t;
  vector<RequestStatisticsDesc> blocks;

  // .............................................................................
  // extract the statistics blocks for the given granularity
  // .............................................................................

  size_t resolution = 0;
  size_t total = 0;

  {
    MUTEX_LOCKER(RequestStatistics._lock);

    switch (granularity) {
      case TRI_REQUEST_STATISTICS_SECONDS:
        // not used/implemented, yet
        break;

      case TRI_REQUEST_STATISTICS_MINUTES:
        resolution = RequestStatistics._minute.getResolution();
        total = RequestStatistics._minute.getLength();

        if (limit == (size_t) -1) {
          blocks = RequestStatistics._minute.values(t);
        }
        else if (limit == 0) {
          blocks = RequestStatistics._minute.values(t, 1);
        }
        else {
          blocks = RequestStatistics._minute.values(t, limit);
        }

        break;

      case TRI_REQUEST_STATISTICS_HOURS:
        resolution = RequestStatistics._hour.getResolution();
        total = RequestStatistics._hour.getLength();
        
        if (limit == (size_t) -1) {
          blocks = RequestStatistics._hour.values(t);
        }
        else if (limit == 0) {
          blocks = RequestStatistics._hour.values(t, 1);
        }
        else {
          blocks = RequestStatistics._hour.values(t, limit);
        }

        break;

      case TRI_REQUEST_STATISTICS_DAYS:
        resolution = RequestStatistics._day.getResolution();
        total = RequestStatistics._day.getLength();
        
        if (limit == (size_t) -1) {
          blocks = RequestStatistics._day.values(t);
        }
        else if (limit == 0) {
          blocks = RequestStatistics._day.values(t, 1);
        }
        else {
          blocks = RequestStatistics._day.values(t, limit);
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
      RRF_GenerateVariantDistribution<RequestStatisticsDesc::totalAccessor>(result, blocks[0], "totalTime");
    }

    if (showQueueTime) {
      RRF_GenerateVariantDistribution<RequestStatisticsDesc::queueAccessor>(result, blocks[0], "queueTime");
    }

    if (showRequestTime) {
      RRF_GenerateVariantDistribution<RequestStatisticsDesc::requestAccessor>(result, blocks[0], "requestTime");
    }

    if (showBytesSent) {
      RRF_GenerateVariantDistribution<RequestStatisticsDesc::bytesSentAccessor>(result, blocks[0], "bytesSent");
    }

    if (showBytesReceived) {
      RRF_GenerateVariantDistribution<RequestStatisticsDesc::bytesReceivedAccessor>(result, blocks[0], "bytesReceived");
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
      start->add(new VariantUInt32(*k));
    }

    if (0 < offset) {
      vector<RequestStatisticsDesc> tmp;

      tmp.insert(tmp.begin(), blocks.begin() + offset, blocks.end());
      blocks.swap(tmp);
    }

    result->add("length", new VariantUInt64(blocks.size()));;
    result->add("totalLength", new VariantUInt64(total));

    if (showTotalTime) {
      RRF_GenerateVariantDistributions<RequestStatisticsDesc::totalAccessor>(result, blocks, "totalTime");
    }

    if (showQueueTime) {
      RRF_GenerateVariantDistributions<RequestStatisticsDesc::queueAccessor>(result, blocks, "queueTime");
    }

    if (showRequestTime) {
      RRF_GenerateVariantDistributions<RequestStatisticsDesc::requestAccessor>(result, blocks, "requestTime");
    }

    if (showBytesSent) {
      RRF_GenerateVariantDistributions<RequestStatisticsDesc::bytesSentAccessor>(result, blocks, "bytesSent");
    }

    if (showBytesReceived) {
      RRF_GenerateVariantDistributions<RequestStatisticsDesc::bytesReceivedAccessor>(result, blocks, "bytesReceived");
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief module init function
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseRequestStatistics () {
  static size_t const QUEUE_SIZE = 1000;

  FreeList._first = FreeList._last = NULL;
  DirtyList._first = DirtyList._last = NULL;

  TRI_FillStatisticsList(&FreeList, sizeof(TRI_request_statistics_t), QUEUE_SIZE);

  STATISTICS_INIT(&ListLock);
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
