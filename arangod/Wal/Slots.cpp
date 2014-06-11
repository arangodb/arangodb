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
    _slots(numberOfSlots),
    _numberOfSlots(numberOfSlots),
    _freeSlots(numberOfSlots),
    _waiting(0),
    _handoutIndex(0),
    _recycleIndex(0), 
    _logfile(nullptr),
    _lastAssignedTick(tick),
    _lastCommittedTick(0),
    _readOnly(false) {

  for (size_t i = 0; i < _numberOfSlots; ++i) {
    _slots[i] = new Slot();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the slots
////////////////////////////////////////////////////////////////////////////////

Slots::~Slots () {
  for (size_t i = 0; i < _numberOfSlots; ++i) {
    Slot* slot = _slots[i];

    if (slot != nullptr) {
      delete slot;
    }
  }

  _slots.clear();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the last assigned tick
////////////////////////////////////////////////////////////////////////////////

void Slots::setLastAssignedTick (Slot::TickType tick) {
  MUTEX_LOCKER(_lock);
  TRI_ASSERT(_lastAssignedTick == 0);
  
  _lastAssignedTick = tick;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the last assigned tick
////////////////////////////////////////////////////////////////////////////////

Slot::TickType Slots::lastAssignedTick () {
  MUTEX_LOCKER(_lock);
  return _lastAssignedTick;
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
  // a size of 0 indicates a logfile flush request
  bool isFlushRequest = (size == 0);
  
  bool hasWaited = false;

  while (true) {
    {
      MUTEX_LOCKER(_lock);

      if (_readOnly) {
        return SlotInfo(TRI_ERROR_ARANGO_READ_ONLY);
      }

      Slot* slot = _slots[_handoutIndex];
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
               isFlushRequest ||
               _logfile->freeSize() < static_cast<uint64_t>(alignedSize)) {

          if (_logfile != nullptr) {
            if (isFlushRequest && _logfile->status() == Logfile::StatusType::EMPTY) {
              // an empty logfile does not need to be flushed
              return SlotInfo(TRI_ERROR_ARANGO_DATAFILE_EMPTY);
            }

            // seal existing logfile by creating a footer marker
            int res = writeFooter(slot);

            if (res != TRI_ERROR_NO_ERROR) {
              return SlotInfo(res);
            }

            _logfileManager->setLogfileSealRequested(_logfile);

            // if this variable isn't reset, we'll spin forever in this loop
            isFlushRequest = false;

            // advance to next slot 
            slot = _slots[_handoutIndex];
          }

          // fetch the next free logfile (this may create a new one)
          Logfile::StatusType status = Logfile::StatusType::UNKNOWN;
          _logfile = _logfileManager->getWriteableLogfile(alignedSize, status);

          if (_logfile == nullptr) {
            LOG_WARNING("unable to acquire writeable wal logfile!");
            usleep(10 * 1000);
          }
          else if (status == Logfile::StatusType::EMPTY) {
            // inititialise the empty logfile by writing a header marker
            int res = writeHeader(slot);
            
            if (res != TRI_ERROR_NO_ERROR) {
              return SlotInfo(res);
            }

            _logfileManager->setLogfileOpen(_logfile);

            // advance to next slot 
            slot = _slots[_handoutIndex];
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
    // wait until data has been committed to disk
    while (true) {
      CONDITION_LOCKER(guard, _condition);
      
      if (lastCommittedTick() >= tick) {
        return;
      }
      
      guard.wait(10000000);
    }
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
    Slot* slot = _slots[slotIndex];
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
      Slot* slot = _slots[slotIndex];
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
/// @brief write a header marker
////////////////////////////////////////////////////////////////////////////////

int Slots::writeHeader (Slot* slot) {            
  TRI_df_header_marker_t header = _logfile->getHeaderMarker();
  size_t const size = header.base._size;
  
  TRI_df_marker_t* mem = (TRI_df_marker_t*) _logfile->reserve(size);
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
  TRI_df_footer_marker_t footer = _logfile->getFooterMarker();
  size_t const size = footer.base._size;
  
  TRI_df_marker_t* mem = (TRI_df_marker_t*) _logfile->reserve(size);
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

  return ++_lastAssignedTick;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
