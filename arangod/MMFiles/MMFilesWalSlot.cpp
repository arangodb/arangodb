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

#include "MMFilesWalSlot.h"
#include "Basics/hashes.h"
#include "MMFiles/MMFilesWalMarker.h"

using namespace arangodb;

/// @brief create a slot
MMFilesWalSlot::MMFilesWalSlot()
    : _tick(0),
      _logfile(nullptr),
      _mem(nullptr),
      _size(0),
      _status(StatusType::UNUSED) {}

/// @brief return the slot status as a string
std::string MMFilesWalSlot::statusText() const {
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

/// @brief calculate the CRC and length values for the slot and
/// store them in the marker
void MMFilesWalSlot::finalize(MMFilesWalMarker const* marker) {
  TRI_ASSERT(marker != nullptr);
  uint32_t const size = marker->size();

  TRI_ASSERT(_mem != nullptr);
  TRI_ASSERT(size == _size);
  TRI_ASSERT(size >= sizeof(MMFilesMarker));
  TRI_ASSERT(_logfile != nullptr);

  MMFilesMarker* dfm = static_cast<MMFilesMarker*>(_mem);

  // set type and tick
  dfm->setTypeAndTick(marker->type(), _tick);

  // set size
  dfm->setSize(static_cast<TRI_voc_size_t>(size));

  // calculate the crc
  dfm->setCrc(0);
  TRI_voc_crc_t crc = TRI_InitialCrc32();
  crc = TRI_BlockCrc32(crc, static_cast<char const*>(_mem),
                       static_cast<TRI_voc_size_t>(size));
  dfm->setCrc(TRI_FinalCrc32(crc));

  TRI_IF_FAILURE("WalSlotCrc") {
    // intentionally corrupt the marker
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "intentionally writing corrupt marker into datafile";
    dfm->setCrc(0xdeadbeef);
  }
}

/// @brief calculate the CRC value for the source region (this will modify
/// the source region) and copy the calculated marker data into the slot
/// note that marker type has to be set already in the src 
void MMFilesWalSlot::fill(void* src, size_t size) {
  TRI_ASSERT(size == _size);
  TRI_ASSERT(src != nullptr);
  TRI_ASSERT(_logfile != nullptr);

  MMFilesMarker* marker = static_cast<MMFilesMarker*>(src);

  // set tick
  marker->setTick(_tick);

  // set size
  marker->setSize(static_cast<TRI_voc_size_t>(size));

  // calculate the crc
  marker->setCrc(0);
  TRI_voc_crc_t crc = TRI_InitialCrc32();
  crc = TRI_BlockCrc32(crc, (char const*)marker,
                       static_cast<TRI_voc_size_t>(size));
  marker->setCrc(TRI_FinalCrc32(crc));

  TRI_IF_FAILURE("WalSlotCrc") {
    // intentionally corrupt the marker
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "intentionally writing corrupt marker into datafile";
    marker->setCrc(0xdeadbeef);
  }

  // copy data into marker
  memcpy(_mem, src, size);
}

/// @brief mark as slot as used
void MMFilesWalSlot::setUnused() {
  TRI_ASSERT(isReturned());
  TRI_ASSERT(_logfile != nullptr);
  _tick = 0;
  _logfile = nullptr;
  _mem = nullptr;
  _size = 0;
  _status = StatusType::UNUSED;
}

/// @brief mark as slot as used
void MMFilesWalSlot::setUsed(void* mem, uint32_t size, MMFilesWalLogfile* logfile,
                   MMFilesWalSlot::TickType tick) {
  TRI_ASSERT(isUnused());
  TRI_ASSERT(logfile != nullptr);
  _tick = tick;
  _logfile = logfile;
  _mem = mem;
  _size = size;
  _status = StatusType::USED;
}

/// @brief mark as slot as returned
void MMFilesWalSlot::setReturned(bool waitForSync) {
  TRI_ASSERT(_logfile != nullptr);
  TRI_ASSERT(isUsed());
  if (waitForSync) {
    _status = StatusType::RETURNED_WFS;
  } else {
    _status = StatusType::RETURNED;
  }
}
