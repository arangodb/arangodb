////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log slot
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

#include "Wal/Slot.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a slot
////////////////////////////////////////////////////////////////////////////////

Slot::Slot ()
  : _tick(0),
    _logfileId(0),
    _mem(nullptr),
    _size(0),
    _status(StatusType::UNUSED) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a slot
////////////////////////////////////////////////////////////////////////////////

Slot::~Slot () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the slot status as a string
////////////////////////////////////////////////////////////////////////////////

std::string Slot::statusText () const {
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
  assert(false); 
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief mark as slot as used
////////////////////////////////////////////////////////////////////////////////

void Slot::setUnused () {
  assert(isReturned());
  _tick        = 0;
  _logfileId   = 0;
  _mem         = nullptr;
  _size        = 0;
  _status      = StatusType::UNUSED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mark as slot as used
////////////////////////////////////////////////////////////////////////////////

void Slot::setUsed (void* mem,
                    uint32_t size,
                    Logfile::IdType logfileId,
                    Slot::TickType tick) {
  assert(isUnused());
  _tick = tick;
  _logfileId = logfileId;
  _mem = mem;
  _size = size;
  _status = StatusType::USED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mark as slot as returned
////////////////////////////////////////////////////////////////////////////////

void Slot::setReturned (bool waitForSync) {
  assert(isUsed());
  if (waitForSync) {
    _status = StatusType::RETURNED_WFS;
  }
  else {
    _status = StatusType::RETURNED;
  }
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
