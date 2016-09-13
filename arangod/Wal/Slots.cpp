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

#include "Slots.h"

#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Logger/Logger.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/datafile.h"
#include "VocBase/ticks.h"
#include "Wal/LogfileManager.h"

using namespace arangodb;
using namespace arangodb::wal;
  
static uint32_t const PrologueSize = DatafileHelper::AlignedSize<uint32_t>(sizeof(TRI_df_prologue_marker_t));

/// @brief create the slots
Slots::Slots(LogfileManager* logfileManager, size_t numberOfSlots,
             Slot::TickType tick)
    : _logfileManager(logfileManager),
      _condition(),
      _lock(),
      _slots(nullptr),
      _numberOfSlots(numberOfSlots),
      _freeSlots(numberOfSlots),
      _waiting(0),
      _handoutIndex(0),
      _recycleIndex(0),
      _logfile(nullptr),
      _lastAssignedTick(0),
      _lastCommittedTick(0),
      _lastCommittedDataTick(0),
      _numEvents(0),
      _numEventsSync(0),
      _lastDatabaseId(0),
      _lastCollectionId(0), 
      _shutdown(false) {
  _slots = new Slot[numberOfSlots];
}

/// @brief destroy the slots
Slots::~Slots() { delete[] _slots; }

/// @brief sets a shutdown flag, disabling the request for new logfiles
void Slots::shutdown() {
  MUTEX_LOCKER(mutexLocker, _lock);
  _shutdown = true;
}

/// @brief get the statistics of the slots
void Slots::statistics(Slot::TickType& lastAssignedTick, 
                       Slot::TickType& lastCommittedTick,
                       Slot::TickType& lastCommittedDataTick,
                       uint64_t& numEvents,
                       uint64_t& numEventsSync) {
  MUTEX_LOCKER(mutexLocker, _lock);
  lastAssignedTick = _lastAssignedTick;
  lastCommittedTick = _lastCommittedTick;
  lastCommittedDataTick = _lastCommittedDataTick;
  numEvents = _numEvents;
  numEventsSync = _numEventsSync;
}

/// @brief execute a flush operation
int Slots::flush(bool waitForSync) {
  Slot::TickType lastTick = 0;
  bool worked;

  int res = closeLogfile(lastTick, worked);

  if (res == TRI_ERROR_REQUEST_CANCELED) {
    // only happens during shutdown
    res = TRI_ERROR_NO_ERROR;
  }

  if (res == TRI_ERROR_NO_ERROR) {
    _logfileManager->signalSync(true);

    if (waitForSync) {
      // wait until data has been committed to disk
      if (!waitForTick(lastTick)) {
        res = TRI_ERROR_ARANGO_SYNC_TIMEOUT;
      } else if (!worked) {
        res = TRI_ERROR_ARANGO_DATAFILE_EMPTY;
      }
    } else if (!worked) {
      // logfile to flush was still empty and thus not flushed

      // not really an error, but used to indicate the specific condition
      res = TRI_ERROR_ARANGO_DATAFILE_EMPTY;
    }
  }

  return res;
}

/// @brief return the last committed tick
Slot::TickType Slots::lastCommittedTick() {
  MUTEX_LOCKER(mutexLocker, _lock);
  return _lastCommittedTick;
}

/// @brief return the next unused slot
SlotInfo Slots::nextUnused(uint32_t size) {
  return nextUnused(0, 0, size);
}

/// @brief return the next unused slot
SlotInfo Slots::nextUnused(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                           uint32_t size) {

  // we need to use the aligned size for writing
  uint32_t alignedSize = DatafileHelper::AlignedSize<uint32_t>(size);
  int iterations = 0;
  bool hasWaited = false;
  bool mustWritePrologue = false; 

  TRI_ASSERT(size > 0);

  while (++iterations < 1000) {
    {
      MUTEX_LOCKER(mutexLocker, _lock);

      Slot* slot = &_slots[_handoutIndex];
      TRI_ASSERT(slot != nullptr);

      // check if next slot is free for writing
      // and also is the slot following it is also free for writing
      // this is required because in some cases we need two free slots
      // to write a WAL entry: the first slot for the prologue marker
      // and the second slot for the actual marker
      if (slot->isUnused() && 
          _slots[nextHandoutIndex()].isUnused()) {
        if (hasWaited) {
          CONDITION_LOCKER(guard, _condition);
          TRI_ASSERT(_waiting > 0);
          --_waiting;
        }

        if (databaseId == 0 && collectionId == 0) {
          _lastDatabaseId = 0;
          _lastCollectionId = 0;
        }
        else if (!mustWritePrologue &&
                 databaseId > 0 && 
                 collectionId > 0 && 
                 (_lastDatabaseId != databaseId || 
                  _lastCollectionId != collectionId)) {
          // write a prologue
          alignedSize = size + PrologueSize;
          mustWritePrologue = true;
        }

        // cycle until we have a valid logfile
        while (_logfile == nullptr ||
               _logfile->freeSize() < static_cast<uint64_t>(alignedSize)) {
          if (_logfile != nullptr) {
            // seal existing logfile by creating a footer marker
            int res = writeFooter(slot);

            if (res != TRI_ERROR_NO_ERROR) {
              return SlotInfo(res);
            }
         
            // new datafile. must write a prologue 
            if (databaseId > 0 && collectionId > 0 && !mustWritePrologue) {
              alignedSize = size + PrologueSize;
              mustWritePrologue = true;
            }

            // advance to next slot
            slot = &_slots[_handoutIndex];
            _logfileManager->setLogfileSealRequested(_logfile);

            _logfile = nullptr;
          }

          TRI_IF_FAILURE("LogfileManagerGetWriteableLogfile") {
            return SlotInfo(TRI_ERROR_ARANGO_NO_JOURNAL);
          }

          // fetch the next free logfile (this may create a new one)
          Logfile::StatusType status;
          int res = newLogfile(alignedSize, status);

          if (res != TRI_ERROR_NO_ERROR) {
            if (res != TRI_ERROR_ARANGO_NO_JOURNAL) {
              return SlotInfo(res);
            }

            usleep(10 * 1000);
            // try again in next iteration
          } else {
            TRI_ASSERT(_logfile != nullptr);

            if (status == Logfile::StatusType::EMPTY) {
              // initialize the empty logfile by writing a header marker
              int res = writeHeader(slot);

              if (res != TRI_ERROR_NO_ERROR) {
                return SlotInfo(res);
              }
            
              // new datafile. must write a prologue 
              if (databaseId > 0 && collectionId > 0 && !mustWritePrologue) {
                alignedSize = size + PrologueSize;
                mustWritePrologue = true;
              }

              // advance to next slot
              slot = &_slots[_handoutIndex];
              _logfileManager->setLogfileOpen(_logfile);
            } else {
              TRI_ASSERT(status == Logfile::StatusType::OPEN);
            }
          }
        }

        // if we get here, we got a free slot for the actual data...

        char* mem = _logfile->reserve(alignedSize);

        if (mem == nullptr) {
          return SlotInfo(TRI_ERROR_INTERNAL);
        }

        TRI_ASSERT(reinterpret_cast<uintptr_t>(mem) % 8 == 0);

        if (mustWritePrologue) {
          // write prologue...

          // hand out the prologue slot and directly fill it
          int res = writePrologue(slot, mem, databaseId, collectionId);

          if (res != TRI_ERROR_NO_ERROR) {
            return SlotInfo(res);
          }

          // now return the slot
          mem += PrologueSize; // advance memory pointer
          TRI_ASSERT(reinterpret_cast<uintptr_t>(mem) % 8 == 0);
          
          // use following slot for the actual data
          slot = &_slots[_handoutIndex];

          // note database and collection id for next time
          _lastDatabaseId = databaseId;
          _lastCollectionId = collectionId;
        }

        // only in this case we return a valid slot
        slot->setUsed(static_cast<void*>(mem), size, _logfile->id(), handout());

        return SlotInfo(slot);
      }
    }

    // if we get here, all slots are busy
    CONDITION_LOCKER(guard, _condition);
    if (!hasWaited) {
      ++_waiting;
      _logfileManager->signalSync(true);
      hasWaited = true;
    }

    bool mustWait;
    {
      MUTEX_LOCKER(mutexLocker, _lock);
      mustWait = (_freeSlots < 2);
    }

    if (mustWait) {
      guard.wait(10 * 1000);
    }
  }

  return SlotInfo(TRI_ERROR_ARANGO_NO_JOURNAL);
}

/// @brief return a used slot, allowing its synchronization
void Slots::returnUsed(SlotInfo& slotInfo, bool wakeUpSynchronizer,
                       bool waitForSyncRequested, bool waitUntilSyncDone) {
  TRI_ASSERT(slotInfo.slot != nullptr);
  // waitUntilSyncDone does not make sense without waitForSyncRequested
  TRI_ASSERT(!waitUntilSyncDone || waitForSyncRequested);

  Slot::TickType tick = slotInfo.slot->tick();

  TRI_ASSERT(tick > 0);

  {
    MUTEX_LOCKER(mutexLocker, _lock);
    slotInfo.slot->setReturned(waitForSyncRequested);
    if (waitForSyncRequested) {
      ++_numEventsSync;
    } else {
      ++_numEvents;
    }
  }

  wakeUpSynchronizer |= waitForSyncRequested;
  wakeUpSynchronizer |= waitUntilSyncDone;

  if (wakeUpSynchronizer) {
    _logfileManager->signalSync(waitForSyncRequested);
  }
  
  if (waitUntilSyncDone) {
    waitForTick(tick);
  }
}

/// @brief get the next synchronisable region
SyncRegion Slots::getSyncRegion() {
  bool sealRequested = false;
  SyncRegion region;

  MUTEX_LOCKER(mutexLocker, _lock);

  size_t slotIndex = _recycleIndex;

  while (true) {
    Slot const* slot = &_slots[slotIndex];
    TRI_ASSERT(slot != nullptr);

    if (sealRequested && slot->isUnused()) {
      region.canSeal = true;
    }

    if (!slot->isReturned()) {
      // found a slot that is not yet returned
      // if it belongs to another logfile, we can seal the logfile we created
      // the region for
      auto otherId = slot->logfileId();

      if (region.logfileId != 0 && otherId != 0 &&
          otherId != region.logfileId) {
        region.canSeal = true;
      }
      break;
    }

    if (region.logfileId == 0) {
      // first member
      Logfile::StatusType status;

      region.logfileId = slot->logfileId();
      // the following call also updates status
      region.logfile = _logfileManager->getLogfile(slot->logfileId(), status);
      region.mem = static_cast<char*>(slot->mem());
      region.size = slot->size();
      region.logfileStatus = status;
      region.firstSlotIndex = slotIndex;
      region.lastSlotIndex = slotIndex;
      region.waitForSync = slot->waitForSync();

      if (status == Logfile::StatusType::SEAL_REQUESTED) {
        sealRequested = true;
      }
    } else {
      if (slot->logfileId() != region.logfileId) {
        // got a different logfile
        region.checkMore = true;
        region.canSeal = true;
        break;
      }

      // this is a group commit!!

      // update the region
      region.size += (uint32_t)(static_cast<char*>(slot->mem()) -
                                (region.mem + region.size) + slot->size());
      region.lastSlotIndex = slotIndex;
      region.waitForSync |= slot->waitForSync();
    }

    if (++slotIndex >= _numberOfSlots) {
      slotIndex = 0;
    }

    if (slotIndex == _recycleIndex) {
      // one full loop
      break;
    }
  }

  return region;
}

/// @brief return a region to the freelist
void Slots::returnSyncRegion(SyncRegion const& region) {
  TRI_ASSERT(region.logfileId != 0);

  size_t slotIndex = region.firstSlotIndex;

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    while (true) {
      Slot* slot = &_slots[slotIndex];
      TRI_ASSERT(slot != nullptr);

      // note last tick
      Slot::TickType tick = slot->tick();
      TRI_ASSERT(tick >= _lastCommittedTick);
      _lastCommittedTick = tick;

      // update the data tick
      TRI_df_marker_t const* m =
          static_cast<TRI_df_marker_t const*>(slot->mem());
      if (m->getType() != TRI_DF_MARKER_HEADER &&
          m->getType() != TRI_DF_MARKER_FOOTER) {
        _lastCommittedDataTick = tick;
      }

      region.logfile->update(m);

      slot->setUnused();
      ++_freeSlots;

      // update recycle index, too
      if (++_recycleIndex >= _numberOfSlots) {
        _recycleIndex = 0;
      }

      if (slotIndex == region.lastSlotIndex) {
        break;
      }

      if (++slotIndex >= _numberOfSlots) {
        slotIndex = 0;
      }
    }
  }

  // signal that we have done something
  CONDITION_LOCKER(guard, _condition);

  if (_waiting > 0 || region.waitForSync) {
    _condition.broadcast();
  }
}

/// @brief get the current open region of a logfile
/// this uses the slots lock
void Slots::getActiveLogfileRegion(Logfile* logfile, char const*& begin,
                                   char const*& end) {
  MUTEX_LOCKER(mutexLocker, _lock);

  TRI_datafile_t* datafile = logfile->df();

  begin = datafile->_data;
  end = begin + datafile->_currentSize;
}

/// @brief get the current tick range of a logfile
/// this uses the slots lock
void Slots::getActiveTickRange(Logfile* logfile, TRI_voc_tick_t& tickMin,
                               TRI_voc_tick_t& tickMax) {
  MUTEX_LOCKER(mutexLocker, _lock);

  TRI_datafile_t* datafile = logfile->df();

  tickMin = datafile->_tickMin;
  tickMax = datafile->_tickMax;
}

/// @brief close a logfile
int Slots::closeLogfile(Slot::TickType& lastCommittedTick, bool& worked) {
  int iterations = 0;
  bool hasWaited = false;
  worked = false;

  while (++iterations < 1000) {
    {
      MUTEX_LOCKER(mutexLocker, _lock);

      lastCommittedTick = _lastCommittedTick;

      Slot* slot = &_slots[_handoutIndex];
      TRI_ASSERT(slot != nullptr);

      if (slot->isUnused()) {
        if (hasWaited) {
          CONDITION_LOCKER(guard, _condition);
          TRI_ASSERT(_waiting > 0);
          --_waiting;
          hasWaited = false;
        }

        if (_logfile != nullptr) {
          if (_logfile->status() == Logfile::StatusType::EMPTY) {
            // no need to seal a still-empty logfile
            return TRI_ERROR_NO_ERROR;
          }

          // seal existing logfile by creating a footer marker
          int res = writeFooter(slot);

          if (res != TRI_ERROR_NO_ERROR) {
            LOG(ERR) << "could not write logfile footer: " << TRI_errno_string(res);
            return res;
          }

          _logfileManager->setLogfileSealRequested(_logfile);

          // advance to next slot
          slot = &_slots[_handoutIndex];

          // invalidate the logfile so for the next write we'll use a
          // new one
          _logfile = nullptr;

          // fall-through intentional
        }

        TRI_IF_FAILURE("LogfileManagerGetWriteableLogfile") {
          return TRI_ERROR_ARANGO_NO_JOURNAL;
        }

        TRI_ASSERT(_logfile == nullptr);
        // fetch the next free logfile (this may create a new one)
        // note: as we don't have a real marker to write the size does
        // not matter (we use a size of 1 as it must be > 0)
        Logfile::StatusType status;
        int res = newLogfile(1, status);

        if (res != TRI_ERROR_NO_ERROR) {
          if (res != TRI_ERROR_ARANGO_NO_JOURNAL) {
            return res;
          }

          usleep(10 * 1000);
          // try again in next iteration
        } else {
          TRI_ASSERT(_logfile != nullptr);

          if (status == Logfile::StatusType::EMPTY) {
            // initialize the empty logfile by writing a header marker
            res = writeHeader(slot);

            if (res != TRI_ERROR_NO_ERROR) {
              LOG(ERR) << "could not write logfile header: " << TRI_errno_string(res);
              return res;
            }

            _logfileManager->setLogfileOpen(_logfile);
            worked = true;
          } else {
            TRI_ASSERT(status == Logfile::StatusType::OPEN);
            worked = false;
          }

          return TRI_ERROR_NO_ERROR;
        }
      }
    }

    // if we get here, all slots are busy
    CONDITION_LOCKER(guard, _condition);
    if (!hasWaited) {
      ++_waiting;
      hasWaited = true;
    }

    bool mustWait;
    {
      MUTEX_LOCKER(mutexLocker, _lock);
      mustWait = (_freeSlots < 2);
    }

    if (mustWait) {
      guard.wait(10 * 1000);
    }
  }

  return TRI_ERROR_ARANGO_NO_JOURNAL;
}

/// @brief write a header marker
int Slots::writeHeader(Slot* slot) {
  TRI_ASSERT(_logfile != nullptr);

  TRI_df_header_marker_t header = DatafileHelper::CreateHeaderMarker(
    static_cast<TRI_voc_size_t>(_logfile->allocatedSize()), 
    static_cast<TRI_voc_fid_t>(_logfile->id())
  );
  size_t const size = header.base.getSize();

  auto* mem = static_cast<void*>(_logfile->reserve(size));
  TRI_ASSERT(mem != nullptr);

  slot->setUsed(mem, static_cast<uint32_t>(size), _logfile->id(), handout());
  slot->fill(&header.base, size);
  slot->setReturned(false);  // no sync

  // reset values for next write
  _lastDatabaseId = 0;
  _lastCollectionId = 0;

  return TRI_ERROR_NO_ERROR;
}

/// @brief write a prologue marker
int Slots::writePrologue(Slot* slot, void* mem, TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId) {
  TRI_df_prologue_marker_t header = DatafileHelper::CreatePrologueMarker(databaseId, collectionId);
  size_t const size = header.base.getSize();

  TRI_ASSERT(size == PrologueSize);

  TRI_ASSERT(mem != nullptr);

  slot->setUsed(mem, static_cast<uint32_t>(size), _logfile->id(), handout());
  slot->fill(&header.base, size);
  slot->setReturned(false);  // no sync

  return TRI_ERROR_NO_ERROR;
}

/// @brief write a footer marker
int Slots::writeFooter(Slot* slot) {
  TRI_ASSERT(_logfile != nullptr);

  TRI_df_footer_marker_t footer = DatafileHelper::CreateFooterMarker();
  size_t const size = footer.base.getSize();

  auto* mem = static_cast<void*>(_logfile->reserve(size));
  TRI_ASSERT(mem != nullptr);

  slot->setUsed(mem, static_cast<uint32_t>(size), _logfile->id(), handout());
  slot->fill(&footer.base, size);
  slot->setReturned(true);  // sync
  
  // reset values for next write
  _lastDatabaseId = 0;
  _lastCollectionId = 0;

  return TRI_ERROR_NO_ERROR;
}

/// @brief handout a region and advance the handout index
Slot::TickType Slots::handout() {
  TRI_ASSERT(_freeSlots > 0);
  _freeSlots--;

  if (++_handoutIndex == _numberOfSlots) {
    // wrap around
    _handoutIndex = 0;
  }

  _lastAssignedTick = static_cast<Slot::TickType>(TRI_NewTickServer());
  return _lastAssignedTick;
}

/// @brief return the next slots that would be handed out, without 
/// actually handing it out
size_t Slots::nextHandoutIndex() const {
  size_t handoutIndex = _handoutIndex;
  if (++handoutIndex == _numberOfSlots) {
    handoutIndex = 0;
  }
  return handoutIndex;
}

/// @brief wait until all data has been synced up to a certain marker
bool Slots::waitForTick(Slot::TickType tick) {
  static uint64_t const SleepTime = 10000;
  static int const MaxIterations = 30 * 1000 * 1000 / SleepTime;
  int iterations = 0;

  // wait until data has been committed to disk
  do {
    if (lastCommittedTick() >= tick) {
      return true;
    }

    CONDITION_LOCKER(guard, _condition);
    guard.wait(SleepTime);
  } while (++iterations < MaxIterations);

  return false;
}

/// @brief request a new logfile which can satisfy a marker of the
/// specified size
int Slots::newLogfile(uint32_t size, Logfile::StatusType& status) {
  TRI_ASSERT(size > 0);

  if (_shutdown) {
    return TRI_ERROR_REQUEST_CANCELED;
  }

  status = Logfile::StatusType::UNKNOWN;
  Logfile* logfile = nullptr;
  int res = _logfileManager->getWriteableLogfile(size, status, logfile);

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_ASSERT(logfile != nullptr);
    _logfile = logfile;
  }

  return res;
}
