////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log synchroniser thread
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

#include "SynchroniserThread.h"
#include "BasicsC/logging.h"
#include "Basics/ConditionLocker.h"
#include "Wal/LogfileManager.h"
#include "Wal/Slots.h"
#include "Wal/SyncRegion.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the synchroniser thread
////////////////////////////////////////////////////////////////////////////////

SynchroniserThread::SynchroniserThread (LogfileManager* logfileManager)
  : Thread("WalSynchroniser"),
    _logfileManager(logfileManager),
    _condition(),
    _waiting(0),
    _stop(0) {
  
  allowAsynchronousCancelation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the synchroniser thread
////////////////////////////////////////////////////////////////////////////////

SynchroniserThread::~SynchroniserThread () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the synchroniser thread
////////////////////////////////////////////////////////////////////////////////

void SynchroniserThread::stop () {
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
/// @brief signal that we need a sync
////////////////////////////////////////////////////////////////////////////////

void SynchroniserThread::signalSync () {
  CONDITION_LOCKER(guard, _condition);
  ++_waiting;
  _condition.signal();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief main loop
////////////////////////////////////////////////////////////////////////////////

void SynchroniserThread::run () {
  while (_stop == 0) {
    uint32_t waiting = 0;

    {
      CONDITION_LOCKER(guard, _condition);
      waiting = _waiting;
    }

    if (waiting > 0) {
      SyncRegion region = _logfileManager->slots()->getReturned();

      if (region.logfileId != 0) {
        // TODO: perform the actual syncing
        _logfileManager->slots()->unuse(region);
    
        // TODO: seal logfile here if required
      }
    }

    CONDITION_LOCKER(guard, _condition);
    if (waiting > 0) {
      assert(_waiting >= waiting);
      _waiting -= waiting;
    }
    if (_waiting == 0) {
      guard.wait(100000000);
    }
  }

  _stop = 2;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
