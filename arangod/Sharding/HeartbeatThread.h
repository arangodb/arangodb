////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_SHARDING_HEARTBEAT_THREAD_H
#define TRIAGENS_SHARDING_HEARTBEAT_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Sharding/AgencyComm.h"

#ifdef __cplusplus
extern "C" {
#endif


namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                                   HeartbeatThread
// -----------------------------------------------------------------------------

    class HeartbeatThread : public basics::Thread {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------
      
      private:
        HeartbeatThread (HeartbeatThread const&);
        HeartbeatThread& operator= (HeartbeatThread const&);

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

        HeartbeatThread (std::string const&,
                         uint64_t,
                         uint64_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

        ~HeartbeatThread ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------
      
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the heartbeat
////////////////////////////////////////////////////////////////////////////////

        bool init ();

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the heartbeat
////////////////////////////////////////////////////////////////////////////////

        void stop () {
          _stop = 1;
          _condition.signal();
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop
////////////////////////////////////////////////////////////////////////////////

        void run ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------
      
      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief sends the current server's state to the agency
////////////////////////////////////////////////////////////////////////////////

        bool sendState ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief AgencyComm instance
////////////////////////////////////////////////////////////////////////////////

        AgencyComm _agency;

////////////////////////////////////////////////////////////////////////////////
/// @brief condition variable for heartbeat
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::ConditionVariable _condition;

////////////////////////////////////////////////////////////////////////////////
/// @brief this server's id
////////////////////////////////////////////////////////////////////////////////

        const std::string _myId;

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat interval
////////////////////////////////////////////////////////////////////////////////

        uint64_t _interval;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of fails in a row before a warning is issued
////////////////////////////////////////////////////////////////////////////////

        uint64_t _maxFailsBeforeWarning;

////////////////////////////////////////////////////////////////////////////////
/// @brief current number of fails in a row
////////////////////////////////////////////////////////////////////////////////

        uint64_t _numFails;

////////////////////////////////////////////////////////////////////////////////
/// @brief stop flag
////////////////////////////////////////////////////////////////////////////////
        
        volatile sig_atomic_t _stop;

    };

  }
}


#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

