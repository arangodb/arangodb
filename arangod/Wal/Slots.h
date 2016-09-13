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

#ifndef ARANGOD_WAL_SLOTS_H
#define ARANGOD_WAL_SLOTS_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"
#include "Wal/Logfile.h"
#include "Wal/Slot.h"
#include "Wal/SyncRegion.h"

namespace arangodb {
namespace wal {

class LogfileManager;

struct SlotInfoCopy {
  explicit SlotInfoCopy(Slot const* slot)
      : mem(slot->mem()),
        size(slot->size()),
        logfileId(slot->logfileId()),
        tick(slot->tick()),
        errorCode(TRI_ERROR_NO_ERROR) {}

  explicit SlotInfoCopy(int errorCode)
      : mem(nullptr), size(0), logfileId(0), tick(0), errorCode(errorCode) {}

  void const* mem;
  uint32_t const size;
  Logfile::IdType const logfileId;
  Slot::TickType const tick;
  int const errorCode;
};

struct SlotInfo {
  explicit SlotInfo(int errorCode)
      : slot(nullptr), mem(nullptr), size(0), errorCode(errorCode) {}

  explicit SlotInfo(Slot* slot)
      : slot(slot),
        mem(slot->mem()),
        size(slot->size()),
        errorCode(TRI_ERROR_NO_ERROR) {}

  SlotInfo() : SlotInfo(TRI_ERROR_NO_ERROR) {}

  Slot* slot;
  void const* mem;
  uint32_t size;
  int errorCode;
};

class Slots {
 private:
  Slots(Slots const&) = delete;
  Slots& operator=(Slots const&) = delete;

 public:
  /// @brief create the slots
  Slots(LogfileManager*, size_t, Slot::TickType);

  /// @brief destroy the slots
  ~Slots();

 public:
  /// @brief sets a shutdown flag, disabling the request for new logfiles
  void shutdown();

  /// @brief get the statistics of the slots
  void statistics(Slot::TickType&, Slot::TickType&, Slot::TickType&, uint64_t&, uint64_t&);

  /// @brief execute a flush operation
  int flush(bool);

  /// @brief return the last committed tick
  Slot::TickType lastCommittedTick();

  /// @brief return the next unused slot
  SlotInfo nextUnused(uint32_t size);
  
  /// @brief return the next unused slot
  SlotInfo nextUnused(TRI_voc_tick_t databaseId, 
                      TRI_voc_cid_t collectionId, uint32_t size);

  /// @brief return a used slot, allowing its synchronization
  void returnUsed(SlotInfo&, bool wakeUpSynchronizer,
                  bool waitForSyncRequested, bool waitUntilSyncDone);

  /// @brief get the next synchronizable region
  SyncRegion getSyncRegion();

  /// @brief return a region to the freelist
  void returnSyncRegion(SyncRegion const&);

  /// @brief get the current open region of a logfile
  /// this uses the slots lock
  void getActiveLogfileRegion(Logfile*, char const*&, char const*&);

  /// @brief get the current tick range of a logfile
  /// this uses the slots lock
  void getActiveTickRange(Logfile*, TRI_voc_tick_t&, TRI_voc_tick_t&);

  /// @brief close a logfile
  int closeLogfile(Slot::TickType&, bool&);

  /// @brief write a header marker
  int writeHeader(Slot*);
  
  /// @brief writes a prologue for a document/remove marker
  int writePrologue(Slot*, void*, TRI_voc_tick_t, TRI_voc_cid_t);

  /// @brief write a footer marker
  int writeFooter(Slot*);

  /// @brief handout a region and advance the handout index
  Slot::TickType handout();

  /// @brief return the next slots that would be handed out, without 
  /// actually handing it out
  size_t nextHandoutIndex() const;

  /// @brief wait until all data has been synced up to a certain marker
  bool waitForTick(Slot::TickType);

  /// @brief request a new logfile which can satisfy a marker of the
  /// specified size
  int newLogfile(uint32_t, Logfile::StatusType& status);

 private:
  /// @brief the logfile manager
  LogfileManager* _logfileManager;

  /// @brief condition variable for slots
  basics::ConditionVariable _condition;

  /// @brief mutex protecting the slots interface
  Mutex _lock;

  /// @brief all slots
  Slot* _slots;

  /// @brief the total number of slots
  size_t const _numberOfSlots;

  /// @brief the number of currently free slots
  size_t _freeSlots;

  /// @brief whether or not someone is waiting for a slot
  uint32_t _waiting;

  /// @brief the index of the slot to hand out next
  size_t _handoutIndex;

  /// @brief the index of the slot to recycle
  size_t _recycleIndex;

  /// @brief the current logfile to write into
  Logfile* _logfile;

  /// @brief last assigned tick value
  Slot::TickType _lastAssignedTick;

  /// @brief last committed tick value
  Slot::TickType _lastCommittedTick;

  /// @brief last committed data tick value
  Slot::TickType _lastCommittedDataTick;

  /// @brief number of log events handled
  uint64_t _numEvents;

  /// @brief number of sync log events handled
  uint64_t _numEventsSync;
  
  /// @brief last written database id (in prologue marker)
  TRI_voc_tick_t _lastDatabaseId;
  
  /// @brief last written collection id (in prologue marker)
  TRI_voc_cid_t _lastCollectionId;

  /// @brief shutdown flag, set by LogfileManager on shutdown
  bool _shutdown;
};
}
}

#endif
