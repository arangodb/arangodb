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

#include "statistics.h"

#include "BasicsC/locks.h"

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
/// @addtogroup Statistics
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for lists
////////////////////////////////////////////////////////////////////////////////

static STATISTICS_TYPE RequestListLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief free list
////////////////////////////////////////////////////////////////////////////////

static TRI_statistics_list_t RequestFreeList;

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
  TRI_request_statistics_t* statistics = NULL;

  STATISTICS_LOCK(&RequestListLock);

  if (RequestFreeList._first != NULL) {
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

  // check the request was completely received and transmitted
  if (statistics->_readStart != 0.0 && statistics->_writeEnd != 0.0) {
    double totalTime = statistics->_writeEnd - statistics->_readStart;
    TotalTimeDistribution->addFigure(totalTime);

    double requestTime = statistics->_requestEnd - statistics->_requestStart;
    RequestTimeDistribution->addFigure(requestTime);

    if (statistics->_queueStart != 0.0 && statistics->_queueEnd != 0.0) {
      double queueTime = statistics->_queueEnd - statistics->_queueStart;
      QueueTimeDistribution->addFigure(queueTime);
    }

    BytesSentDistribution->addFigure(statistics->_sentBytes);
    BytesReceivedDistribution->addFigure(statistics->_receivedBytes);
  }

  // clear statistics and put back an the free list
  memset(statistics, 0, sizeof(TRI_request_statistics_t));

  if (RequestFreeList._first == NULL) {
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
                                StatisticsDistribution& bytesSent,
                                StatisticsDistribution& bytesReceived) {
  STATISTICS_LOCK(&RequestListLock);

  totalTime = *TotalTimeDistribution;
  requestTime = *RequestTimeDistribution;
  queueTime = *QueueTimeDistribution;
  bytesSent = *BytesSentDistribution;
  bytesReceived = *BytesReceivedDistribution;

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

static STATISTICS_TYPE ConnectionListLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief free list
////////////////////////////////////////////////////////////////////////////////

static TRI_statistics_list_t ConnectionFreeList;

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

  STATISTICS_UNLOCK(&ConnectionListLock);

  return statistics;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a statistics block
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseConnectionStatistics (TRI_connection_statistics_t* statistics) {
  STATISTICS_LOCK(&ConnectionListLock);

  if (statistics->_http) {
    if (statistics->_connStart != 0) {
      if (statistics->_connEnd == 0) {
        HttpConnections.incCounter();
      }
      else {
        HttpConnections.decCounter();

        double totalTime = statistics->_connEnd - statistics->_connStart;
        ConnectionTimeDistribution->addFigure(totalTime);
      }
    }
  }

  // clear statistics and put back an the free list
  memset(statistics, 0, sizeof(TRI_connection_statistics_t));

  if (ConnectionFreeList._first == NULL) {
    ConnectionFreeList._first = (TRI_statistics_entry_t*) statistics;
    ConnectionFreeList._last = (TRI_statistics_entry_t*) statistics;
  }
  else {
    ConnectionFreeList._last->_next = (TRI_statistics_entry_t*) statistics;
    ConnectionFreeList._last = (TRI_statistics_entry_t*) statistics;
  }

  ConnectionFreeList._last->_next = 0;

  STATISTICS_UNLOCK(&ConnectionListLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the current statistics
////////////////////////////////////////////////////////////////////////////////

void TRI_FillConnectionStatistics (StatisticsCounter& httpConnections,
                                   StatisticsDistribution& connectionTime) {
  STATISTICS_LOCK(&ConnectionListLock);

  httpConnections = HttpConnections;
  connectionTime = *ConnectionTimeDistribution;

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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   public variable
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Statistics
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics enabled flags
////////////////////////////////////////////////////////////////////////////////

bool TRI_ENABLE_STATISTICS = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of http conntections
////////////////////////////////////////////////////////////////////////////////

StatisticsCounter HttpConnections;

////////////////////////////////////////////////////////////////////////////////
/// @brief connection time distribution vector
////////////////////////////////////////////////////////////////////////////////

StatisticsVector ConnectionTimeDistributionVector;

////////////////////////////////////////////////////////////////////////////////
/// @brief total time distribution
////////////////////////////////////////////////////////////////////////////////

StatisticsDistribution* ConnectionTimeDistribution;

////////////////////////////////////////////////////////////////////////////////
/// @brief request time distribution vector
////////////////////////////////////////////////////////////////////////////////

StatisticsVector RequestTimeDistributionVector;

////////////////////////////////////////////////////////////////////////////////
/// @brief total time distribution
////////////////////////////////////////////////////////////////////////////////

StatisticsDistribution* TotalTimeDistribution;

////////////////////////////////////////////////////////////////////////////////
/// @brief request time distribution
////////////////////////////////////////////////////////////////////////////////

StatisticsDistribution* RequestTimeDistribution;

////////////////////////////////////////////////////////////////////////////////
/// @brief queue time distribution
////////////////////////////////////////////////////////////////////////////////

StatisticsDistribution* QueueTimeDistribution;

////////////////////////////////////////////////////////////////////////////////
/// @brief bytes sent distribution vector
////////////////////////////////////////////////////////////////////////////////

StatisticsVector BytesSentDistributionVector;

////////////////////////////////////////////////////////////////////////////////
/// @brief bytes sent distribution
////////////////////////////////////////////////////////////////////////////////

StatisticsDistribution* BytesSentDistribution;

////////////////////////////////////////////////////////////////////////////////
/// @brief bytes received distribution vector
////////////////////////////////////////////////////////////////////////////////

StatisticsVector BytesReceivedDistributionVector;

////////////////////////////////////////////////////////////////////////////////
/// @brief bytes received distribution
////////////////////////////////////////////////////////////////////////////////

StatisticsDistribution* BytesReceivedDistribution;

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
  return (double)(time(0));
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief module init function
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseStatistics () {
#if TRI_ENABLE_FIGURES

  static size_t const QUEUE_SIZE = 1000;

  // .............................................................................
  // sets up the statistics
  // .............................................................................

  ConnectionTimeDistributionVector << (0.1) << (1.0) << (60.0);

  BytesSentDistributionVector << (250) << (1000) << (2 * 1000) << (5 * 1000) << (10 * 1000);
  BytesReceivedDistributionVector << (250) << (1000) << (2 * 1000) << (5 * 1000) << (10 * 1000);

#ifdef TRI_ENABLE_HIRES_FIGURES
  RequestTimeDistributionVector << (0.0001) << (0.05) << (0.1) << (0.2) << (0.5) << (1.0);
#else
  RequestTimeDistributionVector << (0.01) << (0.05) << (0.1) << (0.2) << (0.5) << (1.0);
#endif

  ConnectionTimeDistribution = new StatisticsDistribution(ConnectionTimeDistributionVector);
  TotalTimeDistribution = new StatisticsDistribution(RequestTimeDistributionVector);
  RequestTimeDistribution = new StatisticsDistribution(RequestTimeDistributionVector);
  QueueTimeDistribution = new StatisticsDistribution(RequestTimeDistributionVector);
  BytesSentDistribution = new StatisticsDistribution(BytesSentDistributionVector);
  BytesReceivedDistribution = new StatisticsDistribution(BytesReceivedDistributionVector); 

  // .............................................................................
  // generate the request statistics queue
  // .............................................................................

  RequestFreeList._first = RequestFreeList._last = NULL;

  FillStatisticsList(&RequestFreeList, sizeof(TRI_request_statistics_t), QUEUE_SIZE);

  STATISTICS_INIT(&RequestListLock);

  // .............................................................................
  // generate the connection statistics queue
  // .............................................................................

  ConnectionFreeList._first = ConnectionFreeList._last = NULL;

  FillStatisticsList(&ConnectionFreeList, sizeof(TRI_connection_statistics_t), QUEUE_SIZE);

  STATISTICS_INIT(&ConnectionListLock);

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
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
