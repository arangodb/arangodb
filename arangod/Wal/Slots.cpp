////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log slots
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

#include "Slots.h"
#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "BasicsC/hashes.h"
#include "BasicsC/logging.h"
#include "VocBase/datafile.h"
#include "VocBase/server.h"
#include "Wal/LogfileManager.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the slots
////////////////////////////////////////////////////////////////////////////////

Slots::Slots (LogfileManager* logfileManager,
              size_t numberOfSlots,
              Slot::TickType tick)
  : _logfileManager(logfileManager),
    _condition(),
    _lock(),
    _slots(new Slot[numberOfSlots]),
    _numberOfSlots(numberOfSlots),
    _freeSlots(numberOfSlots),
    _waiting(0),
    _handoutIndex(0),
    _recycleIndex(0), 
    _logfile(nullptr),
    _lastCommittedTick(0)  {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the slots
////////////////////////////////////////////////////////////////////////////////

Slots::~Slots () {
  if (_slots != nullptr) {
    delete[] _slots;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a flush operation
////////////////////////////////////////////////////////////////////////////////

int Slots::flush (bool waitForSync) {
  Slot::TickType lastTick = 0;
  bool worked;
  
  int res = closeLogfile(lastTick, worked);

  if (res == TRI_ERROR_NO_ERROR) {
    if (worked && waitForSync) {
      _logfileManager->signalSync();

      // wait until data has been committed to disk
      waitForTick(lastTick);
    }
    else if (! worked) {
      // logfile to flush was still empty and thus not flushed

      // not really an error, but used to indicate the specific condition
      res = TRI_ERROR_ARANGO_DATAFILE_EMPTY;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the last committed tick
////////////////////////////////////////////////////////////////////////////////

Slot::TickType Slots::lastCommittedTick () {
  MUTEX_LOCKER(_lock);
  return _lastCommittedTick;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next unused slot
////////////////////////////////////////////////////////////////////////////////

SlotInfo Slots::nextUnused (uint32_t size) {
  // we need to use the aligned size for writing
  uint32_t alignedSize = TRI_DF_ALIGN_BLOCK(size);
  bool hasWaited = false;

  TRI_ASSERT(size > 0);

  while (true) {
    {
      MUTEX_LOCKER(_lock);

      Slot* slot = &_slots[_handoutIndex];
      TRI_ASSERT(slot != nullptr);

      if (slot->isUnused()) {
        if (hasWaited) {
          CONDITION_LOCKER(guard, _condition);
          TRI_ASSERT(_waiting > 0);
          --_waiting;
          hasWaited = false;
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

            // advance to next slot 
            slot = &_slots[_handoutIndex];
            _logfileManager->setLogfileSealRequested(_logfile);
          }

          // fetch the next free logfile (this may create a new one)
          Logfile::StatusType status = newLogfile(alignedSize);

          if (_logfile == nullptr) {
            usleep(10 * 1000);
            // try again in next iteration
          }
          else if (status == Logfile::StatusType::EMPTY) {
            // inititialise the empty logfile by writing a header marker
            int res = writeHeader(slot);
            
            if (res != TRI_ERROR_NO_ERROR) {
              return SlotInfo(res);
            }

            // advance to next slot 
            slot = &_slots[_handoutIndex];
            _logfileManager->setLogfileOpen(_logfile);
          }
          else {
            TRI_ASSERT(status == Logfile::StatusType::OPEN);
          }
        }

        // if we get here, we got a free slot for the actual data...

        char* mem = _logfile->reserve(alignedSize);

        if (mem == nullptr) {
          return SlotInfo(TRI_ERROR_INTERNAL);
        }

        // only in this case we return a valid slot
        slot->setUsed(static_cast<void*>(mem), size, _logfile->id(), handout());

        return SlotInfo(slot);
      }
    }

    // if we get here, all slots are busy
    CONDITION_LOCKER(guard, _condition);
    if (! hasWaited) {
      ++_waiting;
      hasWaited = true;
    }

    bool mustWait;
    { 
      MUTEX_LOCKER(_lock);
      mustWait = (_freeSlots == 0);
    }
    
    if (mustWait) {
      guard.wait(10 * 1000);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a used slot, allowing its synchronisation
////////////////////////////////////////////////////////////////////////////////

void Slots::returnUsed (SlotInfo& slotInfo,
                        bool waitForSync) {
  TRI_ASSERT(slotInfo.slot != nullptr);
  Slot::TickType tick = slotInfo.slot->tick();

  {
    MUTEX_LOCKER(_lock);
    slotInfo.slot->setReturned(waitForSync);
  }
  
  _logfileManager->signalSync();

  if (waitForSync) {
    waitForTick(tick);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the next synchronisable region
////////////////////////////////////////////////////////////////////////////////

SyncRegion Slots::getSyncRegion () {
  SyncRegion region;

  MUTEX_LOCKER(_lock);

  size_t slotIndex = _recycleIndex;
   
  while (true) {
    Slot const* slot = &_slots[slotIndex];
    TRI_ASSERT(slot != nullptr);

    if (! slot->isReturned()) {
      break;
    }

    if (region.logfileId == 0) {
      // first member
      region.logfileId      = slot->logfileId();
      region.mem            = static_cast<char*>(slot->mem());
      region.size           = slot->size();
      region.firstSlotIndex = slotIndex;
      region.lastSlotIndex  = slotIndex;
      region.waitForSync    = slot->waitForSync();
    }
    else {
      if (slot->logfileId() != region.logfileId) {
        // got a different logfile
        break;
      }

      // LOG_INFO("group commit");

      // update the region
      region.size += (uint32_t) (static_cast<char*>(slot->mem()) - (region.mem + region.size) + slot->size());
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

////////////////////////////////////////////////////////////////////////////////
/// @brief return a region to the freelist
////////////////////////////////////////////////////////////////////////////////

void Slots::returnSyncRegion (SyncRegion const& region) {
  TRI_ASSERT(region.logfileId != 0);
    
  size_t slotIndex = region.firstSlotIndex;

  {
    MUTEX_LOCKER(_lock);

    while (true) {
      Slot* slot = &_slots[slotIndex];
      TRI_ASSERT(slot != nullptr);
      
      // note last tick        
      Slot::TickType tick = slot->tick();
      TRI_ASSERT(tick >= _lastCommittedTick);
      _lastCommittedTick = tick;

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

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief close a logfile
////////////////////////////////////////////////////////////////////////////////

int Slots::closeLogfile (Slot::TickType& lastCommittedTick,
                         bool& worked) {
  bool hasWaited = false;
  worked = false;

  while (true) {
    {
      MUTEX_LOCKER(_lock);

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
            return res;
          }
          
          _logfileManager->setLogfileSealRequested(_logfile);

          // invalidate the logfile so for the next write we'll use a
          // new one
          _logfile = nullptr; 
          worked = true;
          return TRI_ERROR_NO_ERROR;
        }

        TRI_ASSERT(_logfile == nullptr);
        // fetch the next free logfile (this may create a new one)
        // note: as we don't have a real marker to write the size does
        // not matter (we use a size of 1 as  it must be > 0)
        Logfile::StatusType status = newLogfile(1);

        if (_logfile == nullptr) {
          usleep(10 * 1000);
          // try again in next iteration
        }
        else if (status == Logfile::StatusType::EMPTY) {
          // inititialise the empty logfile by writing a header marker
          int res = writeHeader(slot);
          
          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          _logfileManager->setLogfileOpen(_logfile);
          worked = false;
          return TRI_ERROR_NO_ERROR;
        }
        else {
          TRI_ASSERT(status == Logfile::StatusType::OPEN);
          worked = false;
          return TRI_ERROR_NO_ERROR;
        }
      }
    }

    // if we get here, all slots are busy
    CONDITION_LOCKER(guard, _condition);
    if (! hasWaited) {
      ++_waiting;
      hasWaited = true;
    }

    bool mustWait;
    { 
      MUTEX_LOCKER(_lock);
      mustWait = (_freeSlots == 0);
    }
    
    if (mustWait) {
      guard.wait(10 * 1000);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write a header marker
////////////////////////////////////////////////////////////////////////////////

int Slots::writeHeader (Slot* slot) {            
  TRI_df_header_marker_t header = _logfile->getHeaderMarker();
  size_t const size = header.base._size;

  TRI_df_marker_t* mem = reinterpret_cast<TRI_df_marker_t*>(_logfile->reserve(size));
  TRI_ASSERT(mem != nullptr);

  slot->setUsed(static_cast<void*>(mem), static_cast<uint32_t>(size), _logfile->id(), handout());
  slot->fill(&header.base, size);
  slot->setReturned(false); // sync

  return TRI_ERROR_NO_ERROR;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief write a footer marker
////////////////////////////////////////////////////////////////////////////////

int Slots::writeFooter (Slot* slot) {
  TRI_ASSERT(_logfile != nullptr);

  TRI_df_footer_marker_t footer = _logfile->getFooterMarker();
  size_t const size = footer.base._size;
  
  TRI_df_marker_t* mem = reinterpret_cast<TRI_df_marker_t*>(_logfile->reserve(size));
  TRI_ASSERT(mem != nullptr);

  slot->setUsed(static_cast<void*>(mem), static_cast<uint32_t>(size), _logfile->id(), handout());
  slot->fill(&footer.base, size);
  slot->setReturned(true); // sync

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handout a region and advance the handout index
////////////////////////////////////////////////////////////////////////////////

Slot::TickType Slots::handout () { 
  TRI_ASSERT(_freeSlots > 0);
  _freeSlots--;

  if (++_handoutIndex ==_numberOfSlots) {
    // wrap around
    _handoutIndex = 0;
  }
    
  return static_cast<Slot::TickType>(TRI_NewTickServer());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait until all data has been synced up to a certain marker
////////////////////////////////////////////////////////////////////////////////

void Slots::waitForTick (Slot::TickType tick) {
  // wait until data has been committed to disk
  while (true) {
    CONDITION_LOCKER(guard, _condition);
      
    if (lastCommittedTick() >= tick) {
      return;
    }
      
    guard.wait(100 * 1000);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief request a new logfile which can satisfy a marker of the
/// specified size
////////////////////////////////////////////////////////////////////////////////

Logfile::StatusType Slots::newLogfile (uint32_t size) {
  TRI_ASSERT(size > 0);

  Logfile::StatusType status = Logfile::StatusType::UNKNOWN;
  _logfile = _logfileManager->getWriteableLogfile(size, status);

  return status;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
