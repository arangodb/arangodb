////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#include "DatabaseGuard.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FeatureCacheFeature.h"

using namespace arangodb;

/// @brief create the guard, using a database id
DatabaseGuard::DatabaseGuard(TRI_voc_tick_t id) : _vocbase(nullptr) {
  auto databaseFeature = FeatureCacheFeature::instance()->databaseFeature();
  _vocbase = databaseFeature->useDatabase(id);

  if (_vocbase == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_ASSERT(!_vocbase->isDangling());
}

/// @brief create the guard, using a database name
DatabaseGuard::DatabaseGuard(std::string const& name) : _vocbase(nullptr) {
  auto databaseFeature = FeatureCacheFeature::instance()->databaseFeature();
  _vocbase = databaseFeature->useDatabase(name);

  if (_vocbase == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_ASSERT(!_vocbase->isDangling());
}

DatabaseGuard::DatabaseGuard(DatabaseGuard&& other) : _vocbase(other._vocbase) {
  other._vocbase = nullptr;
}
