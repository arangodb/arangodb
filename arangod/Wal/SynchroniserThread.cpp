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
#include "VocBase/server.h"
#include "Wal/LogfileManager.h"
#include "Wal/Slots.h"
#include "Wal/SyncRegion.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                          class SynchroniserThread
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief wait interval for the synchroniser thread when idle
////////////////////////////////////////////////////////////////////////////////

const uint64_t SynchroniserThread::Interval = 500000;

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
    _stop(0),
    _logfileCache() {

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

    // go on without the lock
    if (waiting > 0) {
      // get region to sync
      SyncRegion region = _logfileManager->slots()->getSyncRegion();
      Logfile::IdType const id = region.logfileId;

      if (id != 0) {
        // now perform the actual syncing
        Logfile::StatusType status = _logfileManager->getLogfileStatus(id);
        assert(status == Logfile::StatusType::OPEN || status == Logfile::StatusType::SEAL_REQUESTED);

        // get the logfile's file descriptor
        int fd = getLogfileDescriptor(region);

        if (fd < 0) {
          // invalid file descriptor
          LOG_FATAL_AND_EXIT("invalid wal logfile file descriptor");
        }
        else {
          void** mmHandle = NULL;
          bool res = TRI_MSync(fd, mmHandle, region.mem, region.mem + region.size);
          
          LOG_TRACE("syncing logfile %llu, region %p - %p, length: %lu, wfs: %s",
                    (unsigned long long) id,
                    region.mem, 
                    region.mem + region.size, 
                    (unsigned long) region.size,
                    region.waitForSync ? "true" : "false");

          if (! res) {
            LOG_ERROR("unable to sync wal logfile region");
            // TODO: how to recover from this state?
          }

          if (status == Logfile::StatusType::SEAL_REQUESTED) {
            // additionally seal the logfile
            _logfileManager->sealLogfile(id);
          }
        }
        
        _logfileManager->slots()->returnSyncRegion(region);
      }
    }

    // now wait until we are woken up or there is something to do
    CONDITION_LOCKER(guard, _condition);

    if (waiting > 0) {
      assert(_waiting >= waiting);
      _waiting -= waiting;
    }

    if (_waiting == 0) {
      // sleep if nothing to do
      guard.wait(Interval);
    }
  }

  _stop = 2;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get a logfile descriptor (it caches the descriptor for performance)
////////////////////////////////////////////////////////////////////////////////

int SynchroniserThread::getLogfileDescriptor (SyncRegion const& region) {
  if (region.logfileId != _logfileCache.id || 
      _logfileCache.id == 0) {

    _logfileCache.id = region.logfileId;
    _logfileCache.fd = _logfileManager->getLogfileDescriptor(region.logfileId);
  }

  return _logfileCache.fd;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
