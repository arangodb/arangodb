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

#ifndef ARANGOD_MMFILES_WAL_SLOTS_H
#define ARANGOD_MMFILES_WAL_SLOTS_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"
#include "MMFiles/MMFilesWalSlot.h"
#include "MMFiles/MMFilesWalSyncRegion.h"
#include "MMFiles/MMFilesWalLogfile.h"

namespace arangodb {
class MMFilesLogfileManager;

struct MMFilesWalSlotInfoCopy {
  explicit MMFilesWalSlotInfoCopy(MMFilesWalSlot const* slot)
      : mem(slot->mem()),
        size(slot->size()),
        logfileId(slot->logfileId()),
        tick(slot->tick()),
        errorCode(TRI_ERROR_NO_ERROR) {}

  explicit MMFilesWalSlotInfoCopy(int errorCode)
      : mem(nullptr), size(0), logfileId(0), tick(0), errorCode(errorCode) {}

  void const* mem;
  uint32_t const size;
  MMFilesWalLogfile::IdType const logfileId;
  MMFilesWalSlot::TickType const tick;
  int const errorCode;
};

struct MMFilesWalSlotInfo {
  explicit MMFilesWalSlotInfo(int errorCode)
      : slot(nullptr), logfile(nullptr), mem(nullptr), size(0), errorCode(errorCode) {}

  explicit MMFilesWalSlotInfo(MMFilesWalSlot* slot)
      : slot(slot),
        logfile(slot->logfile()),
        mem(slot->mem()),
        size(slot->size()),
        errorCode(TRI_ERROR_NO_ERROR) {}

  MMFilesWalSlotInfo() : MMFilesWalSlotInfo(TRI_ERROR_NO_ERROR) {}

  MMFilesWalSlot* slot;
  MMFilesWalLogfile* logfile;
  void const* mem;
  uint32_t size;
  int errorCode;
};

class MMFilesWalSlots {
 private:
  MMFilesWalSlots(MMFilesWalSlots const&) = delete;
  MMFilesWalSlots& operator=(MMFilesWalSlots const&) = delete;

 public:
  /// @brief create the slots
  MMFilesWalSlots(MMFilesLogfileManager*, size_t, MMFilesWalSlot::TickType);

  /// @brief destroy the slots
  ~MMFilesWalSlots();

 public:
  /// @brief sets a shutdown flag, disabling the request for new logfiles
  void shutdown();

  /// @brief get the statistics of the slots
  void statistics(MMFilesWalSlot::TickType&, MMFilesWalSlot::TickType&, MMFilesWalSlot::TickType&, uint64_t&, uint64_t&);

  /// @brief execute a flush operation
  int flush(bool);
  
  /// @brief initially set the last ticks on start
  void setLastTick(MMFilesWalSlot::TickType const&);

  /// @brief return the last committed tick
  MMFilesWalSlot::TickType lastCommittedTick();

  /// @brief return the next unused slot
  MMFilesWalSlotInfo nextUnused(uint32_t size);
  
  /// @brief return the next unused slot
  MMFilesWalSlotInfo nextUnused(TRI_voc_tick_t databaseId, 
                      TRI_voc_cid_t collectionId, uint32_t size);

  /// @brief return a used slot, allowing its synchronization
  int returnUsed(MMFilesWalSlotInfo&, bool wakeUpSynchronizer,
                 bool waitForSyncRequested, bool waitUntilSyncDone);

  /// @brief get the next synchronizable region
  MMFilesWalSyncRegion getSyncRegion();

  /// @brief return a region to the freelist
  void returnSyncRegion(MMFilesWalSyncRegion const&);

  /// @brief get the current open region of a logfile
  /// this uses the slots lock
  void getActiveLogfileRegion(MMFilesWalLogfile*, char const*&, char const*&);

  /// @brief get the current tick range of a logfile
  /// this uses the slots lock
  void getActiveTickRange(MMFilesWalLogfile*, TRI_voc_tick_t&, TRI_voc_tick_t&);

  /// @brief close a logfile
  int closeLogfile(MMFilesWalSlot::TickType&, bool&);

  /// @brief write a header marker
  int writeHeader(MMFilesWalSlot*);
  
  /// @brief writes a prologue for a document/remove marker
  int writePrologue(MMFilesWalSlot*, void*, TRI_voc_tick_t, TRI_voc_cid_t);

  /// @brief write a footer marker
  int writeFooter(MMFilesWalSlot*);

  /// @brief handout a region and advance the handout index
  MMFilesWalSlot::TickType handout();

  /// @brief return the next slots that would be handed out, without 
  /// actually handing it out
  size_t nextHandoutIndex() const;

  /// @brief wait until all data has been synced up to a certain marker
  bool waitForTick(MMFilesWalSlot::TickType);

  /// @brief request a new logfile which can satisfy a marker of the
  /// specified size
  int newLogfile(uint32_t, MMFilesWalLogfile::StatusType& status);

 private:
  /// @brief the logfile manager
  MMFilesLogfileManager* _logfileManager;

  /// @brief condition variable for slots
  basics::ConditionVariable _condition;

  /// @brief mutex protecting the slots interface
  Mutex _lock;

  /// @brief all slots
  MMFilesWalSlot* _slots;

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
  MMFilesWalLogfile* _logfile;

  /// @brief last assigned tick value
  MMFilesWalSlot::TickType _lastAssignedTick;

  /// @brief last committed tick value
  MMFilesWalSlot::TickType _lastCommittedTick;

  /// @brief last committed data tick value
  MMFilesWalSlot::TickType _lastCommittedDataTick;

  /// @brief number of log events handled
  uint64_t _numEvents;

  /// @brief number of sync log events handled
  uint64_t _numEventsSync;
  
  /// @brief last written database id (in prologue marker)
  TRI_voc_tick_t _lastDatabaseId;
  
  /// @brief last written collection id (in prologue marker)
  TRI_voc_cid_t _lastCollectionId;

  /// @brief shutdown flag, set by MMFilesLogfileManager on shutdown
  bool _shutdown;
};

}

#endif
