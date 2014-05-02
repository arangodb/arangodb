////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log garbage collection thread
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

#ifndef TRIAGENS_WAL_COLLECTOR_THREAD_H
#define TRIAGENS_WAL_COLLECTOR_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"

namespace triagens {
  namespace wal {

    class LogfileManager;
    
// -----------------------------------------------------------------------------
// --SECTION--                                             class CollectorThread
// -----------------------------------------------------------------------------

    class CollectorThread : public basics::Thread {

////////////////////////////////////////////////////////////////////////////////
/// @brief CollectorThread
////////////////////////////////////////////////////////////////////////////////

      private:
        CollectorThread (CollectorThread const&);
        CollectorThread& operator= (CollectorThread const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the collector thread
////////////////////////////////////////////////////////////////////////////////

        CollectorThread (LogfileManager*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the collector thread
////////////////////////////////////////////////////////////////////////////////

        ~CollectorThread ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the collector thread
////////////////////////////////////////////////////////////////////////////////

        void stop ();

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
/// @brief perform collection of a logfile (if any)
////////////////////////////////////////////////////////////////////////////////

        void collect ();

////////////////////////////////////////////////////////////////////////////////
/// @brief perform removal of a logfile (if any)
////////////////////////////////////////////////////////////////////////////////

        void remove ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the logfile manager
////////////////////////////////////////////////////////////////////////////////

        LogfileManager* _logfileManager;

////////////////////////////////////////////////////////////////////////////////
/// @brief condition variable for the collector thread
////////////////////////////////////////////////////////////////////////////////

        basics::ConditionVariable _condition;

////////////////////////////////////////////////////////////////////////////////
/// @brief stop flag
////////////////////////////////////////////////////////////////////////////////
        
        volatile sig_atomic_t _stop;

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
