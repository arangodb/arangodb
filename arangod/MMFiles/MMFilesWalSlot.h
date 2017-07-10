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

#ifndef ARANGOD_MMFILES_WAL_SLOT_H
#define ARANGOD_MMFILES_WAL_SLOT_H 1

#include "Basics/Common.h"
#include "MMFiles/MMFilesWalLogfile.h"

namespace arangodb {
class MMFilesWalSlots;
class MMFilesWalMarker;

class MMFilesWalSlot {
  friend class MMFilesWalSlots;

 public:
  /// @brief tick typedef
  typedef TRI_voc_tick_t TickType;

  /// @brief slot status typedef
  enum class StatusType : uint32_t {
    UNUSED = 0,
    USED = 1,
    RETURNED = 2,
    RETURNED_WFS = 3
  };

  /// @brief create a slot
 private:
  MMFilesWalSlot();

 public:
  /// @brief return the tick assigned to the slot
  inline MMFilesWalSlot::TickType tick() const { return _tick; }

  /// @brief return the logfile id assigned to the slot
  inline MMFilesWalLogfile::IdType logfileId() const { 
    if (_logfile != nullptr) {
      return _logfile->id(); 
    }
    return 0;
  }
  
  /// @brief return the logfile assigned to the slot
  inline MMFilesWalLogfile* logfile() const { return _logfile; } 

  /// @brief return the raw memory pointer assigned to the slot
  inline void* mem() const { return _mem; }

  /// @brief return the memory size assigned to the slot
  inline uint32_t size() const { return _size; }

  /// @brief return the slot status as a string
  std::string statusText() const;

  /// @brief calculate the CRC and length values for the slot and
  /// store them in the marker
  void finalize(MMFilesWalMarker const*);
  
  /// @brief calculate the CRC value for the source region (this will modify
  /// the source region) and copy the calculated marker data into the slot
  void fill(void*, size_t);

 private:
  /// @brief whether or not the slot is unused
  inline bool isUnused() const { return _status == StatusType::UNUSED; }

  /// @brief whether or not the slot is used
  inline bool isUsed() const { return _status == StatusType::USED; }

  /// @brief whether or not the slot is returned
  inline bool isReturned() const {
    return (_status == StatusType::RETURNED ||
            _status == StatusType::RETURNED_WFS);
  }

  /// @brief whether or not a sync was requested for the slot
  inline bool waitForSync() const {
    return (_status == StatusType::RETURNED_WFS);
  }

  /// @brief mark as slot as unused
  void setUnused();

  /// @brief mark as slot as used
  void setUsed(void*, uint32_t, MMFilesWalLogfile*, MMFilesWalSlot::TickType);

  /// @brief mark as slot as returned
  void setReturned(bool waitForSync);

 private:
  /// @brief slot tick
  MMFilesWalSlot::TickType _tick;

  /// @brief slot logfile
  MMFilesWalLogfile* _logfile;

  /// @brief slot raw memory pointer
  void* _mem;

#ifdef TRI_PADDING_32
  char _padding[4];
#endif

  /// @brief slot raw memory size
  uint32_t _size;

  /// @brief slot status
  StatusType _status; 
};

static_assert(sizeof(MMFilesWalSlot) == 32, "invalid slot size");
}

#endif
