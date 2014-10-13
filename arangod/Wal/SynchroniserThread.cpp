////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log synchroniser thread
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SynchroniserThread.h"
#include "Basics/logging.h"
#include "Basics/ConditionLocker.h"
#include "Utils/Exception.h"
#include "VocBase/server.h"
#include "Wal/LogfileManager.h"
#include "Wal/Slots.h"
#include "Wal/SyncRegion.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                          class SynchroniserThread
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the synchroniser thread
////////////////////////////////////////////////////////////////////////////////

SynchroniserThread::SynchroniserThread (LogfileManager* logfileManager,
                                        uint64_t syncInterval)
  : Thread("WalSynchroniser"),
    _logfileManager(logfileManager),
    _condition(),
    _waiting(0),
    _stop(0),
    _syncInterval(syncInterval),
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
    usleep(10000);
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
  uint64_t iterations = 0;

  while (true) {
    int stop = (int) _stop;
    uint32_t waiting = 0;

    {
      CONDITION_LOCKER(guard, _condition);
      waiting = _waiting;
    }

    // go on without the lock

    if (waiting > 0 || ++iterations == 10) {
      iterations = 0;

      try {
        // sync as much as we can in this loop
        bool checkMore = false;
        while (true) {
          int res = doSync(checkMore);

          if (res != TRI_ERROR_NO_ERROR || ! checkMore) {
            break;
          }
        }
      }
      catch (triagens::arango::Exception const& ex) {
        int res = ex.code();
        LOG_ERROR("got unexpected error in synchroniserThread: %s", TRI_errno_string(res));
      }
      catch (...) {
        LOG_ERROR("got unspecific error in synchroniserThread");
      }
    }

    // now wait until we are woken up or there is something to do
    CONDITION_LOCKER(guard, _condition);

    if (waiting > 0) {
      TRI_ASSERT(_waiting >= waiting);
      _waiting -= waiting;
    }

    if (_waiting == 0 && stop == 0) {
      // sleep if nothing to do
      guard.wait(_syncInterval);
    }

    if (stop > 0 && _waiting == 0) {
      // stop requested and all synced, we can exit
      break;
    }

    // next iteration
  }

  _stop = 2;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief synchronise an unsynchronized region
////////////////////////////////////////////////////////////////////////////////

int SynchroniserThread::doSync (bool& checkMore) {
  checkMore = false;

  // get region to sync
  SyncRegion region = _logfileManager->slots()->getSyncRegion();
  Logfile::IdType const id = region.logfileId;

  // an id of 0 means an empty region...
  if (id == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  // now perform the actual syncing
  auto status = region.logfileStatus;
  TRI_ASSERT(status == Logfile::StatusType::OPEN || status == Logfile::StatusType::SEAL_REQUESTED);

  // get the logfile's file descriptor
  int fd = getLogfileDescriptor(region.logfileId);
  TRI_ASSERT(fd >= 0);
  void** mmHandle = nullptr;
  bool result = TRI_MSync(fd, mmHandle, region.mem, region.mem + region.size);

  LOG_TRACE("syncing logfile %llu, region %p - %p, length: %lu, wfs: %s",
            (unsigned long long) id,
            region.mem,
            region.mem + region.size,
            (unsigned long) region.size,
            region.waitForSync ? "true" : "false");

  if (! result) {
    LOG_ERROR("unable to sync wal logfile region");

    return TRI_ERROR_ARANGO_MSYNC_FAILED;
  }

  // all ok

  if (status == Logfile::StatusType::SEAL_REQUESTED) {
    // additionally seal the logfile
    _logfileManager->setLogfileSealed(id);
  }
  
  checkMore = region.checkMore;

  _logfileManager->slots()->returnSyncRegion(region);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a logfile descriptor (it caches the descriptor for performance)
////////////////////////////////////////////////////////////////////////////////

int SynchroniserThread::getLogfileDescriptor (Logfile::IdType id) {
  if (id != _logfileCache.id ||
      _logfileCache.id == 0) {

    _logfileCache.id = id;
    _logfileCache.fd = _logfileManager->getLogfileDescriptor(id);
  }

  return _logfileCache.fd;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
