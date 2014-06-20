////////////////////////////////////////////////////////////////////////////////
/// @brief database usage guard
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_UTILS_DATABASE_GUARD_H
#define TRIAGENS_UTILS_DATABASE_GUARD_H 1

#include "Basics/Common.h"
#include "Utils/Exception.h"
#include "VocBase/server.h"

struct TRI_vocbase_s;

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                               class DatabaseGuard
// -----------------------------------------------------------------------------

    class DatabaseGuard {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:
         
        DatabaseGuard (DatabaseGuard const&) = delete;
        DatabaseGuard& operator= (DatabaseGuard const&) = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the guard, using a database id
////////////////////////////////////////////////////////////////////////////////

        DatabaseGuard (TRI_server_t* server,
                       TRI_voc_tick_t id)
          : _server(server),
            _database(nullptr) {

          _database = TRI_UseDatabaseByIdServer(server, id); 

          if (_database == nullptr) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create the guard, using a database name
////////////////////////////////////////////////////////////////////////////////

        DatabaseGuard (TRI_server_t* server,
                       char const* name)
          : _server(server),
            _database(nullptr) {

          _database = TRI_UseDatabaseServer(server, name); 

          if (_database == nullptr) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the guard
////////////////////////////////////////////////////////////////////////////////

        ~DatabaseGuard () {
          if (_database != nullptr) {
            TRI_ReleaseDatabaseServer(_server, _database);
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the database pointer
////////////////////////////////////////////////////////////////////////////////

        inline struct TRI_vocbase_s* database () const {
          return _database;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief server
////////////////////////////////////////////////////////////////////////////////

        TRI_server_t* _server;

////////////////////////////////////////////////////////////////////////////////
/// @brief pointer to database
////////////////////////////////////////////////////////////////////////////////

        struct TRI_vocbase_s* _database;

    };
  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
