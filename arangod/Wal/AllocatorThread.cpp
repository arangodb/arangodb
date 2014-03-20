////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log storage allocator thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "AllocatorThread.h"
#include "BasicsC/logging.h"
#include "Basics/ConditionLocker.h"
#include "Wal/LogfileManager.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the allocator thread
////////////////////////////////////////////////////////////////////////////////

AllocatorThread::AllocatorThread (LogfileManager* logfileManager) 
  : Thread("WalAllocator"),
    _logfileManager(logfileManager),
    _condition(),
    _createRequests(0),
    _stop(0) {
  
  allowAsynchronousCancelation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the allocator thread
////////////////////////////////////////////////////////////////////////////////

AllocatorThread::~AllocatorThread () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the allocator thread
////////////////////////////////////////////////////////////////////////////////

void AllocatorThread::stop () {
  if (_stop > 0) {
    return;
  }

  _stop = 1;
  _condition.signal();

  while (_stop != 2) {
    usleep(1000);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief signal the creation of a new logfile
////////////////////////////////////////////////////////////////////////////////

void AllocatorThread::signalLogfileCreation () {
  CONDITION_LOCKER(guard, _condition);
  if (_createRequests == 0) {
    ++_createRequests;
    guard.signal();
    
    LOG_INFO("got logfile creation signal");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new logfile
////////////////////////////////////////////////////////////////////////////////

bool AllocatorThread::createLogfile () {
  LOG_INFO("creating new logfile"); 
  int res = _logfileManager->allocateDatafile();

  return (res == TRI_ERROR_NO_ERROR);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief main loop
////////////////////////////////////////////////////////////////////////////////

void AllocatorThread::run () {
  while (_stop == 0) {
    uint32_t createRequests = 0;
    bool reserveRequested = false;

    {
      CONDITION_LOCKER(guard, _condition);
      createRequests = _createRequests;
    }

    if (createRequests == 0 && 
        ! _logfileManager->hasReserveLogfiles()) {
      reserveRequested = true;

      if (! createLogfile()) {
        LOG_ERROR("unable to create new spare logfile");
      }
    }
    else if (createRequests > 0) {
      if (createLogfile()) {
        CONDITION_LOCKER(guard, _condition);
        --_createRequests;

        continue;
      }
      
      LOG_ERROR("unable to create new wal logfile");
    }
    
    {  
      CONDITION_LOCKER(guard, _condition);
      guard.wait(1000000);
    }
  }

  _stop = 2;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
