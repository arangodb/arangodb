////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/Exceptions.h"
#include "RestServer/DatabaseFeature.h"

namespace {

template <typename T>
TRI_vocbase_t& vocbase(arangodb::DatabaseFeature& feature, T& id) {
  auto* vocbase = feature.useDatabase(id);

  if (!vocbase) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  return *vocbase;
}

}  // namespace

namespace arangodb {
  
/// @brief create guard on existing db
DatabaseGuard::DatabaseGuard(TRI_vocbase_t& vocbase) 
    : _vocbase(vocbase) {
  if (!_vocbase.use()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
}

/// @brief create the guard, using a database id
DatabaseGuard::DatabaseGuard(DatabaseFeature& feature, TRI_voc_tick_t id)
    : _vocbase(vocbase(feature, id)) {
  TRI_ASSERT(!_vocbase.isDangling());
}

/// @brief create the guard, using a database name
DatabaseGuard::DatabaseGuard(DatabaseFeature& feature, std::string const& name)
    : _vocbase(vocbase(feature, name)) {
  TRI_ASSERT(!_vocbase.isDangling());
}

}  // namespace arangodb
