////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log storage allocator thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_WAL_ALLOCATOR_THREAD_H
#define ARANGODB_WAL_ALLOCATOR_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/ReadLocker.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Thread.h"
#include "Basics/WriteLocker.h"

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
        AllocatorThread (AllocatorThread const&) = delete;
        AllocatorThread& operator= (AllocatorThread const&) = delete;

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

////////////////////////////////////////////////////////////////////////////////
/// @brief tell the thread that the recovery phase is over
////////////////////////////////////////////////////////////////////////////////

        void recoveryDone () {
          WRITE_LOCKER(_recoveryLock);
          _inRecovery = false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not we are in recovery
////////////////////////////////////////////////////////////////////////////////

        bool inRecovery () {
          READ_LOCKER(_recoveryLock);
          return _inRecovery;
        }

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
/// @brief lock for _inRecovery
////////////////////////////////////////////////////////////////////////////////

        basics::ReadWriteLock _recoveryLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief requested logfile size
////////////////////////////////////////////////////////////////////////////////

        uint32_t _requestedSize;

////////////////////////////////////////////////////////////////////////////////
/// @brief stop flag
////////////////////////////////////////////////////////////////////////////////

        volatile sig_atomic_t _stop;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not we are in the recovery mode
////////////////////////////////////////////////////////////////////////////////

        bool _inRecovery;

////////////////////////////////////////////////////////////////////////////////
/// @brief wait interval for the allocator thread when idle
////////////////////////////////////////////////////////////////////////////////

        static const uint64_t Interval;

    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
