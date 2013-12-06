////////////////////////////////////////////////////////////////////////////////
/// @brief single-server state
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

#ifndef TRIAGENS_SHARDING_SERVER_STATE_H
#define TRIAGENS_SHARDING_SERVER_STATE_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"

#ifdef __cplusplus
extern "C" {
#endif


namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                                       ServerState
// -----------------------------------------------------------------------------

    class ServerState {

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief an enum describing the possible states a server can have 
////////////////////////////////////////////////////////////////////////////////

        typedef enum {
          STATE_OFFLINE    = 0,
          STATE_STARTUP    = 1,
          STATE_CONNECTED  = 2,
          STATE_STOPPING   = 3,
          STATE_STOPPED    = 4,
          STATE_PROBLEM    = 5,
          STATE_RECOVERING = 6,
          STATE_RECOVERED  = 7,
          STATE_SHUTDOWN   = 8
        }
        StateEnum; 

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------
      
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ServerState ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~ServerState ();

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

      public:

        static ServerState* instance ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get the string representation of a state
////////////////////////////////////////////////////////////////////////////////

        static std::string stateToString (StateEnum);

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current state
////////////////////////////////////////////////////////////////////////////////

        StateEnum getCurrent ();

////////////////////////////////////////////////////////////////////////////////
/// @brief set the current state
////////////////////////////////////////////////////////////////////////////////
        
        void setCurrent (StateEnum); 

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief r/w lock for state
////////////////////////////////////////////////////////////////////////////////
      
        triagens::basics::ReadWriteLock _lock;

////////////////////////////////////////////////////////////////////////////////
/// @brief the current state
////////////////////////////////////////////////////////////////////////////////

        StateEnum _state;

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

