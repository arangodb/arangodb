////////////////////////////////////////////////////////////////////////////////
/// @brief transaction unit of work
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

#ifndef TRIAGENS_TRANSACTION_WORK_UNIT_H
#define TRIAGENS_TRANSACTION_WORK_UNIT_H 1

#include "Basics/Common.h"
#include "Transaction/Collection.h"
#include "VocBase/vocbase.h"

namespace triagens {
  namespace transaction {

    class Context;

// -----------------------------------------------------------------------------
// --SECTION--                                                    class WorkUnit
// -----------------------------------------------------------------------------

    class WorkUnit {

////////////////////////////////////////////////////////////////////////////////
/// @brief Context
////////////////////////////////////////////////////////////////////////////////

      private:
        WorkUnit (WorkUnit const&);
        WorkUnit& operator= (WorkUnit const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create a transaction work unit
////////////////////////////////////////////////////////////////////////////////

        WorkUnit (Context*,
                  TRI_vocbase_t*,
                  bool); 

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a transaction work unit
////////////////////////////////////////////////////////////////////////////////

        ~WorkUnit ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to the unit
////////////////////////////////////////////////////////////////////////////////

        int addCollection (std::string const&,
                           Collection::AccessType);

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to the unit
////////////////////////////////////////////////////////////////////////////////
        
        int addCollection (TRI_voc_cid_t,
                           Collection::AccessType);

////////////////////////////////////////////////////////////////////////////////
/// @brief begin a unit of work
////////////////////////////////////////////////////////////////////////////////

        int begin ();

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a unit of work
////////////////////////////////////////////////////////////////////////////////

        int commit (bool = false);

////////////////////////////////////////////////////////////////////////////////
/// @brief rollback a unit of work
////////////////////////////////////////////////////////////////////////////////

        int rollback ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not we are on the top level
////////////////////////////////////////////////////////////////////////////////

        inline bool isTopLevel () const {
          return _level == 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the unit can be used
////////////////////////////////////////////////////////////////////////////////

        inline bool active () const {
          return _active;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief deactive the unit of work
////////////////////////////////////////////////////////////////////////////////

        void deactivate ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the transaction context
////////////////////////////////////////////////////////////////////////////////

        Context* _context;

////////////////////////////////////////////////////////////////////////////////
/// @brief the current level we are in
////////////////////////////////////////////////////////////////////////////////

        int _level;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the unit is active
////////////////////////////////////////////////////////////////////////////////

        bool _active;

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
