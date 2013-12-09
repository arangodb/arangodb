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
/// @brief an enum describing the roles a server can have 
////////////////////////////////////////////////////////////////////////////////

        typedef enum {
          ROLE_UNDEFINED = 0,  // initial value
          ROLE_PRIMARY,
          ROLE_SECONDARY,
          ROLE_COORDINATOR
        }
        RoleEnum;

////////////////////////////////////////////////////////////////////////////////
/// @brief an enum describing the possible states a server can have 
////////////////////////////////////////////////////////////////////////////////

        typedef enum {
          STATE_UNDEFINED = 0,         // initial value
          STATE_STARTUP,               // used by all roles
          STATE_SERVINGASYNC,          // primary only
          STATE_SERVINGSYNC,           // primary only
          STATE_STOPPING,              // primary only
          STATE_STOPPED,               // primary only 
          STATE_SYNCING,               // secondary only
          STATE_INSYNC,                // secondary only
          STATE_LOSTPRIMARY,           // secondary only
          STATE_SERVING,               // coordinator only
          STATE_SHUTDOWN               // used by all roles
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

////////////////////////////////////////////////////////////////////////////////
/// @brief create the (sole) instance
////////////////////////////////////////////////////////////////////////////////

        static ServerState* instance ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get the string representation of a role
////////////////////////////////////////////////////////////////////////////////
        
        static std::string roleToString (RoleEnum);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the string representation of a state
////////////////////////////////////////////////////////////////////////////////

        static std::string stateToString (StateEnum);


// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server role
////////////////////////////////////////////////////////////////////////////////

        inline RoleEnum getRole () const {
          return _role;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the server role
////////////////////////////////////////////////////////////////////////////////
        
        void setRole (RoleEnum role) {
          assert(_role == ROLE_UNDEFINED);
          _role = role;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current state
////////////////////////////////////////////////////////////////////////////////

        StateEnum getState ();

////////////////////////////////////////////////////////////////////////////////
/// @brief set the current state
////////////////////////////////////////////////////////////////////////////////
        
        void setState (StateEnum); 

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

        bool checkPrimaryState (StateEnum);

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a state transition for a secondary server
////////////////////////////////////////////////////////////////////////////////

        bool checkSecondaryState (StateEnum);

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a state transition for a coordinator server
////////////////////////////////////////////////////////////////////////////////

        bool checkCoordinatorState (StateEnum);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief r/w lock for state
////////////////////////////////////////////////////////////////////////////////
      
        triagens::basics::ReadWriteLock _lock;

////////////////////////////////////////////////////////////////////////////////
/// @brief the server role
////////////////////////////////////////////////////////////////////////////////

        RoleEnum _role;

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

