////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log storage allocator thread
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

#ifndef TRIAGENS_WAL_ALLOCATOR_THREAD_H
#define TRIAGENS_WAL_ALLOCATOR_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"

namespace triagens {
  namespace wal {

    class LogfileManager;
    
// -----------------------------------------------------------------------------
// --SECTION--                                             class AllocatorThread
// -----------------------------------------------------------------------------

    class AllocatorThread : public basics::Thread {

////////////////////////////////////////////////////////////////////////////////
/// @brief AllocatorThread
////////////////////////////////////////////////////////////////////////////////

      private:
        AllocatorThread (AllocatorThread const&);
        AllocatorThread& operator= (AllocatorThread const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the allocator thread
////////////////////////////////////////////////////////////////////////////////

        AllocatorThread (LogfileManager*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the allocator thread
////////////////////////////////////////////////////////////////////////////////

        ~AllocatorThread ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the allocator thread
////////////////////////////////////////////////////////////////////////////////

        void stop ();

////////////////////////////////////////////////////////////////////////////////
/// @brief signal the creation of a new logfile
////////////////////////////////////////////////////////////////////////////////

        void signal (uint32_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new reserve logfile
////////////////////////////////////////////////////////////////////////////////

        bool createReserveLogfile (uint32_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief main loop
////////////////////////////////////////////////////////////////////////////////

        void run ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the logfile manager
////////////////////////////////////////////////////////////////////////////////

        LogfileManager* _logfileManager;

////////////////////////////////////////////////////////////////////////////////
/// @brief condition variable for the allocator thread
////////////////////////////////////////////////////////////////////////////////

        basics::ConditionVariable _condition;

////////////////////////////////////////////////////////////////////////////////
/// @brief requested logfile size
////////////////////////////////////////////////////////////////////////////////
        
        uint32_t _requestedSize;

////////////////////////////////////////////////////////////////////////////////
/// @brief stop flag
////////////////////////////////////////////////////////////////////////////////
        
        volatile sig_atomic_t _stop;

////////////////////////////////////////////////////////////////////////////////
/// @brief wait interval for the allocator thread when idle
////////////////////////////////////////////////////////////////////////////////

        static const uint64_t Interval;

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
