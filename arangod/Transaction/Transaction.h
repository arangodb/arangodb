////////////////////////////////////////////////////////////////////////////////
/// @brief transaction
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

#ifndef TRIAGENS_TRANSACTION_TRANSACTION_H
#define TRIAGENS_TRANSACTION_TRANSACTION_H 1

#include "Basics/Common.h"

extern "C" {
  struct TRI_vocbase_s;
}

namespace triagens {
  namespace transaction {

    class Manager;

// -----------------------------------------------------------------------------
// --SECTION--                                                 class Transaction
// -----------------------------------------------------------------------------

    class Transaction {

// -----------------------------------------------------------------------------
// --SECTION--                                                          typedefs
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction id type
////////////////////////////////////////////////////////////////////////////////

        typedef uint64_t IdType;

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction state
////////////////////////////////////////////////////////////////////////////////

        enum class StateType {
          STATE_UNINITIALISED = 0,
          STATE_BEGUN         = 1,
          STATE_ABORTED       = 2,
          STATE_COMMITTED     = 3
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief Transaction
////////////////////////////////////////////////////////////////////////////////

      private:
        Transaction (Transaction const&);
        Transaction& operator= (Transaction const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create a transaction
////////////////////////////////////////////////////////////////////////////////

        Transaction (Manager*,
                     IdType,
                     struct TRI_vocbase_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a transaction
////////////////////////////////////////////////////////////////////////////////

        ~Transaction ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief get the transaction id
////////////////////////////////////////////////////////////////////////////////

        inline IdType id () const {
          return _id;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the transaction state
////////////////////////////////////////////////////////////////////////////////

        inline StateType state () const {
          return _state;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief begin a transaction
////////////////////////////////////////////////////////////////////////////////

        int begin ();

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction
////////////////////////////////////////////////////////////////////////////////
        
        int commit ();

////////////////////////////////////////////////////////////////////////////////
/// @brief abort a transaction
////////////////////////////////////////////////////////////////////////////////
        
        int abort ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the transaction manager
////////////////////////////////////////////////////////////////////////////////
        
        Manager* _manager; 

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction id
////////////////////////////////////////////////////////////////////////////////

        IdType const _id;

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction state
////////////////////////////////////////////////////////////////////////////////

        StateType _state;

////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase for the transaction
////////////////////////////////////////////////////////////////////////////////

        struct TRI_vocbase_s* _vocbase;

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
