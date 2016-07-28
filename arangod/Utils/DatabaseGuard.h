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

#ifndef ARANGOD_UTILS_vocbase_GUARD_H
#define ARANGOD_UTILS_vocbase_GUARD_H 1

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "RestServer/DatabaseFeature.h"

struct TRI_vocbase_t;

namespace arangodb {

class DatabaseGuard {
 public:
  DatabaseGuard(DatabaseGuard const&) = delete;
  DatabaseGuard& operator=(DatabaseGuard const&) = delete;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the guard, using a database id
  //////////////////////////////////////////////////////////////////////////////

  explicit DatabaseGuard(TRI_voc_tick_t id)
      : _vocbase(nullptr) {
    
    auto databaseFeature = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");
    _vocbase = databaseFeature->useDatabase(id);

    if (_vocbase == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the guard, using a database name
  //////////////////////////////////////////////////////////////////////////////

  explicit DatabaseGuard(std::string const& name)
      : _vocbase(nullptr) {
      
    auto databaseFeature = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");
    _vocbase = databaseFeature->useDatabase(name);

    if (_vocbase == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destroy the guard
  //////////////////////////////////////////////////////////////////////////////

  ~DatabaseGuard() {
    TRI_ASSERT(_vocbase != nullptr);
    _vocbase->release();
  }

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the database pointer
  //////////////////////////////////////////////////////////////////////////////

  inline TRI_vocbase_t* database() const { return _vocbase; }

 private:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief pointer to database
  //////////////////////////////////////////////////////////////////////////////

  TRI_vocbase_t* _vocbase;
};
}

#endif
