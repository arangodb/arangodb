////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log synchroniser thread
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

#ifndef TRIAGENS_WAL_SYNCHRONISER_THREAD_H
#define TRIAGENS_WAL_SYNCHRONISER_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Wal/Logfile.h"
#include "Wal/SyncRegion.h"

namespace triagens {
  namespace wal {

    class LogfileManager;

// -----------------------------------------------------------------------------
// --SECTION--                                          class SynchroniserThread
// -----------------------------------------------------------------------------

    class SynchroniserThread : public basics::Thread {

////////////////////////////////////////////////////////////////////////////////
/// @brief SynchroniserThread
////////////////////////////////////////////////////////////////////////////////

      private:
        SynchroniserThread (SynchroniserThread const&);
        SynchroniserThread& operator= (SynchroniserThread const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the synchroniser thread
////////////////////////////////////////////////////////////////////////////////

        SynchroniserThread (LogfileManager*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the synchroniser thread
////////////////////////////////////////////////////////////////////////////////

        ~SynchroniserThread ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the synchroniser thread
////////////////////////////////////////////////////////////////////////////////

        void stop ();

////////////////////////////////////////////////////////////////////////////////
/// @brief signal that a sync is needed
////////////////////////////////////////////////////////////////////////////////

        void signalSync ();

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

////////////////////////////////////////////////////////////////////////////////
/// @brief get a logfile descriptor (it caches the descriptor for performance)
////////////////////////////////////////////////////////////////////////////////

        int getLogfileDescriptor (Logfile::IdType);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the logfile manager
////////////////////////////////////////////////////////////////////////////////

        LogfileManager* _logfileManager;

////////////////////////////////////////////////////////////////////////////////
/// @brief condition variable for the thread
////////////////////////////////////////////////////////////////////////////////

        basics::ConditionVariable _condition;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of requests waiting 
////////////////////////////////////////////////////////////////////////////////

        uint32_t _waiting;

////////////////////////////////////////////////////////////////////////////////
/// @brief stop flag
////////////////////////////////////////////////////////////////////////////////
        
        volatile sig_atomic_t _stop;

////////////////////////////////////////////////////////////////////////////////
/// @brief logfile descriptor cache
////////////////////////////////////////////////////////////////////////////////

        struct {
          Logfile::IdType  id;
          int              fd;
        }
        _logfileCache;

////////////////////////////////////////////////////////////////////////////////
/// @brief wait interval for the synchroniser thread when idle
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
