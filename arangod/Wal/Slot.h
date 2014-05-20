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
#include "ShapedJson/Legends.h"
#include "Wal/Logfile.h"

extern "C" {
  struct TRI_df_marker_s;
}

namespace triagens {
  namespace wal {

    class Slots;

// -----------------------------------------------------------------------------
// --SECTION--                                                        class Slot
// -----------------------------------------------------------------------------

    class Slot {

      friend class Slots;

// -----------------------------------------------------------------------------
// --SECTION--                                                          typedefs
// -----------------------------------------------------------------------------

      public: 

////////////////////////////////////////////////////////////////////////////////
/// @brief tick typedef
////////////////////////////////////////////////////////////////////////////////

        typedef TRI_voc_tick_t TickType;

////////////////////////////////////////////////////////////////////////////////
/// @brief slot status typedef
////////////////////////////////////////////////////////////////////////////////

        enum class StatusType : uint32_t {
          UNUSED        = 0,
          USED          = 1,
          RETURNED      = 2,
          RETURNED_WFS  = 3
        };

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a slot
////////////////////////////////////////////////////////////////////////////////

      private:
       
        Slot ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a slot
////////////////////////////////////////////////////////////////////////////////

        ~Slot ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the tick assigned to the slot
////////////////////////////////////////////////////////////////////////////////

        inline Slot::TickType tick () const {
          return _tick;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the logfile id assigned to the slot
////////////////////////////////////////////////////////////////////////////////
        
        inline Logfile::IdType logfileId () const {
          return _logfileId;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the raw memory pointer assigned to the slot
////////////////////////////////////////////////////////////////////////////////
        
        inline void* mem () const {
          return _mem;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the memory size assigned to the slot
////////////////////////////////////////////////////////////////////////////////
        
        inline uint32_t size () const {
          return _size;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the slot status as a string
////////////////////////////////////////////////////////////////////////////////
        
        std::string statusText () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief calculate the CRC value for the source region (this will modify
/// the source region) and copy the calculated marker data into the slot
////////////////////////////////////////////////////////////////////////////////

        void fill (void*,
                   size_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the slot is unused
////////////////////////////////////////////////////////////////////////////////

        inline bool isUnused () const {
          return _status == StatusType::UNUSED;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the slot is used
////////////////////////////////////////////////////////////////////////////////
        
        inline bool isUsed () const {
          return _status == StatusType::USED;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the slot is returned
////////////////////////////////////////////////////////////////////////////////
      
        inline bool isReturned () const {
          return (_status == StatusType::RETURNED ||
                  _status == StatusType::RETURNED_WFS);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a sync was requested for the slot
////////////////////////////////////////////////////////////////////////////////
      
        inline bool waitForSync () const {
          return (_status == StatusType::RETURNED_WFS);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief mark as slot as unused
////////////////////////////////////////////////////////////////////////////////
      
        void setUnused ();

////////////////////////////////////////////////////////////////////////////////
/// @brief mark as slot as used
////////////////////////////////////////////////////////////////////////////////

        void setUsed (void*,
                      uint32_t,
                      Logfile::IdType,
                      Slot::TickType);

////////////////////////////////////////////////////////////////////////////////
/// @brief mark as slot as returned
////////////////////////////////////////////////////////////////////////////////

        void setReturned (bool);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief slot tick
////////////////////////////////////////////////////////////////////////////////

        Slot::TickType _tick;

////////////////////////////////////////////////////////////////////////////////
/// @brief slot logfile id
////////////////////////////////////////////////////////////////////////////////

        Logfile::IdType _logfileId;

////////////////////////////////////////////////////////////////////////////////
/// @brief slot raw memory pointer
////////////////////////////////////////////////////////////////////////////////

        void* _mem;

////////////////////////////////////////////////////////////////////////////////
/// @brief slot raw memory size
////////////////////////////////////////////////////////////////////////////////

        uint32_t _size;

////////////////////////////////////////////////////////////////////////////////
/// @brief slot status
////////////////////////////////////////////////////////////////////////////////
        
        StatusType _status;

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
