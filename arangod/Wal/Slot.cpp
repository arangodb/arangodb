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

#include "Wal/Slot.h"
#include "Basics/hashes.h"

using namespace arangodb::wal;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a slot
////////////////////////////////////////////////////////////////////////////////

Slot::Slot()
    : _tick(0),
      _logfileId(0),
      _mem(nullptr),
      _size(0),
      _status(StatusType::UNUSED) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the slot status as a string
////////////////////////////////////////////////////////////////////////////////

std::string Slot::statusText() const {
  switch (_status) {
    case StatusType::UNUSED:
      return "unused";
    case StatusType::USED:
      return "used";
    case StatusType::RETURNED:
      return "returned";
    case StatusType::RETURNED_WFS:
      return "returned (wfs)";
  }

  // listen, damn compilers!!
  // _status is an enum class so it has a fixed amount of possible values,
  // which are all covered in the above switch statement
  // stop stelling me that the control flow will reach the end of a non-void
  // function. this cannot happen!!!!!
  TRI_ASSERT(false);
  return "unknown";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief calculate the CRC value for the source region (this will modify
/// the source region) and copy the calculated marker data into the slot
////////////////////////////////////////////////////////////////////////////////

void Slot::fill(void* src, size_t size) {
  TRI_ASSERT(size == _size);
  TRI_ASSERT(src != nullptr);

  TRI_df_marker_t* marker = static_cast<TRI_df_marker_t*>(src);

  // set tick
  marker->_tick = _tick;

  // set size
  marker->_size = static_cast<TRI_voc_size_t>(size);

  // calculate the crc
  marker->_crc = 0;
  TRI_voc_crc_t crc = TRI_InitialCrc32();
  crc = TRI_BlockCrc32(crc, (char const*)marker,
                       static_cast<TRI_voc_size_t>(size));
  marker->_crc = TRI_FinalCrc32(crc);

  TRI_IF_FAILURE("WalSlotCrc") {
    // intentionally corrupt the marker
    LOG(WARN) << "intentionally writing corrupt marker into datafile";
    marker->_crc = 0xdeadbeef;
  }

  // copy data into marker
  memcpy(_mem, src, size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mark as slot as used
////////////////////////////////////////////////////////////////////////////////

void Slot::setUnused() {
  TRI_ASSERT(isReturned());
  _tick = 0;
  _logfileId = 0;
  _mem = nullptr;
  _size = 0;
  _status = StatusType::UNUSED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mark as slot as used
////////////////////////////////////////////////////////////////////////////////

void Slot::setUsed(void* mem, uint32_t size, Logfile::IdType logfileId,
                   Slot::TickType tick) {
  TRI_ASSERT(isUnused());
  _tick = tick;
  _logfileId = logfileId;
  _mem = mem;
  _size = size;
  _status = StatusType::USED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mark as slot as returned
////////////////////////////////////////////////////////////////////////////////

void Slot::setReturned(bool waitForSync) {
  TRI_ASSERT(isUsed());
  if (waitForSync) {
    _status = StatusType::RETURNED_WFS;
  } else {
    _status = StatusType::RETURNED;
  }
}
