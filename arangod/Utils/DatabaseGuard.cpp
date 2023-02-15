////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "VocBase/vocbase.h"

namespace arangodb {

void VocbaseReleaser::operator()(TRI_vocbase_t* vocbase) const noexcept {
  if (vocbase) {
    TRI_ASSERT(!vocbase->isDangling());
    vocbase->release();
  }
}

DatabaseGuard::DatabaseGuard(VocbasePtr vocbase)
    : _vocbase{std::move(vocbase)} {
  if (!_vocbase) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  TRI_ASSERT(!_vocbase->isDangling());
}

/// @brief create guard on existing db
DatabaseGuard::DatabaseGuard(TRI_vocbase_t& vocbase)
    : DatabaseGuard{VocbasePtr{vocbase.use() ? &vocbase : nullptr}} {}

/// @brief create the guard, using a database id
DatabaseGuard::DatabaseGuard(DatabaseFeature& feature, TRI_voc_tick_t id)
    : DatabaseGuard{feature.useDatabase(id)} {}

/// @brief create the guard, using a database name
DatabaseGuard::DatabaseGuard(DatabaseFeature& feature, std::string_view name)
    : DatabaseGuard{feature.useDatabase(name)} {}

}  // namespace arangodb
