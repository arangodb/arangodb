////////////////////////////////////////////////////////////////////////////////
/// @brief resource holder for AQL queries with auto-free functionality
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_UTILS_AHUACATL_GUARD_H
#define ARANGODB_UTILS_AHUACATL_GUARD_H 1

#include "Basics/Common.h"

#include "Ahuacatl/ahuacatl-context.h"
#include "Basics/json.h"
#include "Basics/logging.h"
#include "VocBase/vocbase.h"
#include "Cluster/ServerState.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                               class AhuacatlGuard
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief scope guard for a TRI_aql_context_t*
////////////////////////////////////////////////////////////////////////////////

    class AhuacatlGuard {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the guard
////////////////////////////////////////////////////////////////////////////////

        AhuacatlGuard (TRI_vocbase_t* vocbase,
                       const std::string& query,
                       TRI_json_t* userOptions) :
          _context(0) {
            const bool isCoordinator = ServerState::instance()->isCoordinator();
            _context = TRI_CreateContextAql(vocbase, query.c_str(), query.size(), isCoordinator, userOptions);

            if (_context == 0) {
              LOG_DEBUG("failed to create context for query '%s'", query.c_str());
            }
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the guard
////////////////////////////////////////////////////////////////////////////////

        ~AhuacatlGuard () {
          this->free();
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief free the context
////////////////////////////////////////////////////////////////////////////////

        void free () {
          if (_context != 0) {
            TRI_FreeContextAql(_context);
            _context = 0;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief access the context
////////////////////////////////////////////////////////////////////////////////

        inline TRI_aql_context_t* ptr () const {
          return _context;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether context is valid
////////////////////////////////////////////////////////////////////////////////

        inline bool valid () const {
          return _context != 0;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the AQL context C struct
////////////////////////////////////////////////////////////////////////////////

        TRI_aql_context_t* _context;

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
