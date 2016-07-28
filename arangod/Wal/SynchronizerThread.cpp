////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "SynchronizerThread.h"

#include "Basics/memory-map.h"
#include "Logger/Logger.h"
#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "VocBase/ticks.h"
#include "Wal/LogfileManager.h"
#include "Wal/Slots.h"
#include "Wal/SyncRegion.h"

using namespace arangodb::wal;
  
/// @brief returns the bitmask for the synchronous waiters
/// for use in _waiters only
static constexpr inline uint64_t syncWaitersMask() {
  return static_cast<uint64_t>(0xffffffffULL); 
}
  
/// @brief returns the numbers of bits to shift to get the
/// number of asynchronous waiters 
/// for use in _waiters only
static constexpr inline int asyncWaitersBits() { return 32; }

SynchronizerThread::SynchronizerThread(LogfileManager* logfileManager,
                                       uint64_t syncInterval)
    : Thread("WalSynchronizer"),
      _logfileManager(logfileManager),
      _condition(),
      _syncInterval(syncInterval),
      _logfileCache({0, -1}),
      _waiting(0) {}

/// @brief begin shutdown sequence
void SynchronizerThread::beginShutdown() {
  Thread::beginShutdown();

  CONDITION_LOCKER(guard, _condition);
  guard.signal();
}

/// @brief signal that we need a sync
void SynchronizerThread::signalSync(bool waitForSync) {
  if (waitForSync) {
    uint64_t previous = _waiting.fetch_add(1);
    if ((previous & syncWaitersMask()) == 0) {
      // only signal once, but don't care if we signal a bit too often
      CONDITION_LOCKER(guard, _condition);
      _condition.signal();
    }
  } else {
    uint64_t updateValue = 1ULL << asyncWaitersBits();
    _waiting.fetch_add(updateValue);
  }
}

/// @brief main loop
void SynchronizerThread::run() {
  // fetch initial value for waiting
  uint64_t waitingValue = _waiting;
  uint64_t waitingWithoutSync = waitingValue >> asyncWaitersBits();
  uint64_t waitingWithSync = (waitingValue & syncWaitersMask());

  uint64_t iterations = 0;
  while (true) {
    if (waitingWithoutSync > 0 || waitingWithSync > 0 || ++iterations == 10) {
      iterations = 0;

      try {
        // sync as much as we can in this loop
        bool checkMore = false;

        while (true) {
          int res = doSync(checkMore);

          if (res != TRI_ERROR_NO_ERROR || !checkMore) {
            break;
          }
        }
      } catch (arangodb::basics::Exception const& ex) {
        int res = ex.code();
        LOG(ERR) << "got unexpected error in synchronizerThread: "
                 << TRI_errno_string(res);
      } catch (...) {
        LOG(ERR) << "got unspecific error in synchronizerThread";
      }
    }

    // update value of waiting
    uint64_t updateValue = waitingWithSync + (waitingWithoutSync << asyncWaitersBits());

    if (updateValue > 0) {
      // subtract and fetch previous value in one atomic operation
      waitingValue = _waiting.fetch_sub(updateValue);
      waitingValue -= updateValue; // subtract from previous value
    } else {
      // re-fetch current value
      waitingValue = _waiting;
    }

    waitingWithoutSync = waitingValue >> asyncWaitersBits();
    waitingWithSync = (waitingValue & syncWaitersMask());

    // now wait until we are woken up or there is something to do

    if (waitingWithSync == 0) {
      if (isStopping()) {
        // stop requested and all synced, we can exit
        break;
      }

      // sleep if nothing to do
      CONDITION_LOCKER(guard, _condition);
      guard.wait(_syncInterval);
    }
  }
}

/// @brief synchronize an unsynchronized region
int SynchronizerThread::doSync(bool& checkMore) {
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
  TRI_ASSERT(status == Logfile::StatusType::OPEN ||
             status == Logfile::StatusType::SEAL_REQUESTED);

  // get the logfile's file descriptor
  int fd = getLogfileDescriptor(region.logfileId);
  TRI_ASSERT(fd >= 0);

  bool result = TRI_MSync(fd, region.mem, region.mem + region.size);

  LOG(TRACE) << "syncing logfile " << id << ", region " << (void*) region.mem << " - "
             << (void*)(region.mem + region.size) << ", length: " << region.size
             << ", wfs: " << (region.waitForSync ? "true" : "false");

  if (!result) {
    LOG(ERR) << "unable to sync wal logfile region";

    return TRI_ERROR_ARANGO_MSYNC_FAILED;
  }

  // all ok

  if (status == Logfile::StatusType::SEAL_REQUESTED) {
    // we might not yet be able to seal the logfile yet, for example in
    // the following situation when multi-threading:
    //
    //   // borrow 3 slots from the logfile manager
    //   auto slot1 = logfileManager->allocate(1);
    //   auto slot2 = logfileManager->allocate(1);
    //   auto slot3 = logfileManager->allocate(1);
    //
    //   // return slot 3
    //   logfileManager->finalize(slot3, false);
    //   // return slot 1
    //   logfileManager->finalize(slot1, false);
    //
    //   // some thread now requests flushing logs. this will produce a
    //   // sync region from slot 1..slot 1.
    //   logfileManager->flush(false, false, false);
    //
    //   // if we now return slot2, it would produce a sync region from
    //   // slot2..slot3. this is fine but won't work if the logfile is
    //   // already sealed.
    //   logfileManager->finalize(slot2, false);

    if (region.canSeal) {
      // only seal the logfile if it is safe to do so
      _logfileManager->setLogfileSealed(id);
    }
  }

  checkMore = region.checkMore;

  _logfileManager->slots()->returnSyncRegion(region);
  return TRI_ERROR_NO_ERROR;
}

/// @brief get a logfile descriptor (it caches the descriptor for performance)
int SynchronizerThread::getLogfileDescriptor(Logfile::IdType id) {
  if (id != _logfileCache.id || _logfileCache.id == 0) {
    _logfileCache.id = id;
    _logfileCache.fd = _logfileManager->getLogfileDescriptor(id);
  }

  return _logfileCache.fd;
}
