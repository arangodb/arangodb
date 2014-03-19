////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log entry
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

#ifndef TRIAGENS_WAL_LOG_ENTRY_H
#define TRIAGENS_WAL_LOG_ENTRY_H 1

#include "Basics/Common.h"
#include "VocBase/voc-types.h"

namespace triagens {
  namespace wal {

// -----------------------------------------------------------------------------
// --SECTION--                                                   struct LogEntry
// -----------------------------------------------------------------------------

    struct LogEntry {

// -----------------------------------------------------------------------------
// --SECTION--                                                          typedefs
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the datatype for tick values
////////////////////////////////////////////////////////////////////////////////

        typedef TRI_voc_tick_t TickType;

////////////////////////////////////////////////////////////////////////////////
/// @brief the datatype for crc values
////////////////////////////////////////////////////////////////////////////////

        typedef TRI_voc_crc_t CrcType;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------
      
////////////////////////////////////////////////////////////////////////////////
/// @brief create a log entry
////////////////////////////////////////////////////////////////////////////////

        LogEntry (void* mem,
                  size_t size, 
                  TickType tick)
          : _mem(mem),
            _crc(0),
            _size(size),
            _tick(tick) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a log entry
////////////////////////////////////////////////////////////////////////////////

        ~LogEntry () {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief check if the entry is valid
////////////////////////////////////////////////////////////////////////////////

        bool isValid () const {
          return _mem != nullptr;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the position in memory where the log entry resides
////////////////////////////////////////////////////////////////////////////////

      void* const _mem;

////////////////////////////////////////////////////////////////////////////////
/// @brief the crc value of the log entry
////////////////////////////////////////////////////////////////////////////////

      CrcType _crc;

////////////////////////////////////////////////////////////////////////////////
/// @brief the size of the log entry
////////////////////////////////////////////////////////////////////////////////

      size_t const _size;

////////////////////////////////////////////////////////////////////////////////
/// @brief the id (sequence number) of the log entry
////////////////////////////////////////////////////////////////////////////////

      TickType _tick;

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
