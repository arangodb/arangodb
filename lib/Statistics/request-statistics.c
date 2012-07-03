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

#include "BasicsC/locks.h"

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
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Statistics
/// @{
////////////////////////////////////////////////////////////////////////////////

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

    printf("================================================================================\n");
    printf("read start:     %f\n", statistics->_readStart);
    printf("read end:       %f\n", statistics->_readEnd);
    printf("queue start:    %f\n", statistics->_queueStart);
    printf("queue end:      %f\n", statistics->_queueEnd);
    printf("request start:  %f\n", statistics->_requestStart);
    printf("request end:    %f\n", statistics->_requestEnd);
    printf("write start:    %f\n", statistics->_writeStart);
    printf("write end:      %f\n", statistics->_writeEnd);
    printf("bytes received: %f\n", statistics->_receivedBytes);
    printf("bytes sent:     %f\n", statistics->_sentBytes);
    printf("too large:      %s\n", statistics->_tooLarge ? "yes" : "no");
    printf("execute error:  %s\n", statistics->_executeError ? "yes" : "no");

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
