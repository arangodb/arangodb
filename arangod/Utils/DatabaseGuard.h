////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_UTILS_DATABASE_GUARD_H
#define ARANGOD_UTILS_DATABASE_GUARD_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "VocBase/server.h"

struct TRI_vocbase_t;

namespace arangodb {

class DatabaseGuard {
 public:
  DatabaseGuard(DatabaseGuard const&) = delete;
  DatabaseGuard& operator=(DatabaseGuard const&) = delete;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the guard, using a database id
  //////////////////////////////////////////////////////////////////////////////

  DatabaseGuard(TRI_server_t* server, TRI_voc_tick_t id)
      : _server(server), _database(nullptr) {
    _database = TRI_UseDatabaseByIdServer(server, id);

    if (_database == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the guard, using a database name
  //////////////////////////////////////////////////////////////////////////////

  DatabaseGuard(TRI_server_t* server, char const* name)
      : _server(server), _database(nullptr) {
    _database = TRI_UseDatabaseServer(server, name);

    if (_database == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destroy the guard
  //////////////////////////////////////////////////////////////////////////////

  ~DatabaseGuard() {
    if (_database != nullptr) {
      TRI_ReleaseDatabaseServer(_server, _database);
    }
  }

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the database pointer
  //////////////////////////////////////////////////////////////////////////////

  inline TRI_vocbase_t* database() const { return _database; }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief server
  //////////////////////////////////////////////////////////////////////////////

  TRI_server_t* _server;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief pointer to database
  //////////////////////////////////////////////////////////////////////////////

  TRI_vocbase_t* _database;
};
}

#endif
