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

#ifndef TRIAGENS_WAL_LOG_SLOT_H
#define TRIAGENS_WAL_LOG_SLOT_H 1

#include "Basics/Common.h"
#include "Wal/Logfile.h"

namespace triagens {
  namespace wal {
    class Slots;

    class Slot {
      friend class Slots;

      public: 

        typedef TRI_voc_tick_t TickType;

      public:

        enum class StatusType {
          UNUSED   = 0,
          USED     = 1,
          RETURNED = 2
        };

      private: 
        Slot (size_t slotIndex) 
          : _status(StatusType::UNUSED),
            _tick(0),
            _logfileId(0),
            _mem(nullptr),
            _size(0) {
        }

        ~Slot () {
        }

      public:

        std::string statusText () const {
          switch (_status) {
            case StatusType::UNUSED:
              return "unused";
            case StatusType::USED:
              return "used";
            case StatusType::RETURNED:
              return "returned";
          }
        }

        Slot::TickType tick () const {
          return _tick;
        }
        
        Logfile::IdType logfileId () const {
          return _logfileId;
        }
        
        void* mem () const {
          return _mem;
        }
        
        uint32_t size () const {
          return _size;
        }

      private:
        bool isUnused () const {
          return _status == StatusType::UNUSED;
        }
        
        bool isUsed () const {
          return _status == StatusType::USED;
        }
      
        bool isReturned () const {
          return _status == StatusType::RETURNED;
        }
      
        void setUnused () {
          assert(isReturned());
          _status    = StatusType::UNUSED;
          _tick      = 0;
          _logfileId = 0;
          _mem       = nullptr;
          _size      = 0;
        }

        void setUsed (void* mem,
                      uint32_t size,
                      Logfile::IdType logfileId,
                      Slot::TickType tick) {
          assert(isUnused());
          _status = StatusType::USED;
          _tick = tick;
          _logfileId = logfileId;
          _mem = mem;
          _size = size;
        }

        void setReturned () {
          assert(isUsed());
          _status = StatusType::RETURNED;
        }

      private:
        
        StatusType _status;

        Slot::TickType _tick;

        Logfile::IdType _logfileId;

        void* _mem;

        uint32_t _size;
    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
