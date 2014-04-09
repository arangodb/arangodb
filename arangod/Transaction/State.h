////////////////////////////////////////////////////////////////////////////////
/// @brief transaction state
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

#ifndef TRIAGENS_TRANSACTION_STATE_H
#define TRIAGENS_TRANSACTION_STATE_H 1

#include "Basics/Common.h"

namespace triagens {
  namespace transaction {

// -----------------------------------------------------------------------------
// --SECTION--                                                       class State
// -----------------------------------------------------------------------------

    class State {

// -----------------------------------------------------------------------------
// --SECTION--                                                          typedefs
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction state
////////////////////////////////////////////////////////////////////////////////

        enum class StateType {
          UNINITIALISED = 0,
          BEGUN         = 1,
          ABORTED       = 2,
          COMMITTED     = 3
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief State
////////////////////////////////////////////////////////////////////////////////

      private:
        State (State const&);
        State& operator= (State const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create a state
////////////////////////////////////////////////////////////////////////////////

        State ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a state
////////////////////////////////////////////////////////////////////////////////

        virtual ~State ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

///////////////////////////////////////////////////////////////////////////////
/// @brief get the transaction state
////////////////////////////////////////////////////////////////////////////////

        inline StateType state () const {
          return _state;
        }

///////////////////////////////////////////////////////////////////////////////
/// @brief set the transaction state
////////////////////////////////////////////////////////////////////////////////

        inline void setState (StateType state) {
          _state = state;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief begin a transaction
////////////////////////////////////////////////////////////////////////////////

        virtual int begin () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction
////////////////////////////////////////////////////////////////////////////////
        
        virtual int commit (bool) = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief rollback a transaction
////////////////////////////////////////////////////////////////////////////////
        
        virtual int rollback () = 0;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction state
////////////////////////////////////////////////////////////////////////////////

        StateType _state;

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
