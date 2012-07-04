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
#include "BasicsC/threads.h"
#include "Statistics/request-statistics.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Statistics
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics running
////////////////////////////////////////////////////////////////////////////////

static volatile sig_atomic_t StatisticsRunning = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics thread
////////////////////////////////////////////////////////////////////////////////

static TRI_thread_t StatisticsThread;

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
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Statistics
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief worker for statistics
////////////////////////////////////////////////////////////////////////////////

static void StatisticsLoop (void* data) {
  while (StatisticsRunning != 0) {
    double t = TRI_microtime();

    TRI_LockSpin(&StatisticsTimeLock);
    StatisticsTime = t;
    TRI_UnlockSpin(&StatisticsTimeLock);

    TRI_UpdateRequestStatistics(StatisticsTime);

    t = TRI_microtime();

    TRI_LockSpin(&StatisticsTimeLock);
    StatisticsTime = t;
    TRI_UnlockSpin(&StatisticsTimeLock);

    usleep(10);
  }
}

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
  return time(NULL);
}

#endif
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief fills a linked list
////////////////////////////////////////////////////////////////////////////////

void TRI_FillStatisticsList (TRI_statistics_list_t* list, size_t element, size_t count) {
  size_t i;
  TRI_statistics_entry_t* entry;

  entry = TRI_Allocate(TRI_CORE_MEM_ZONE, element, true);

  list->_first = entry;
  list->_last = entry;

  for (i = 1;  i < count;  ++i) {
    entry = TRI_Allocate(TRI_CORE_MEM_ZONE, element, true);

    list->_last->_next = entry;
    list->_last = entry;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief module init function
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseStatistics () {
#if TRI_ENABLE_FIGURES

  TRI_InitialiseRequestStatistics();

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
