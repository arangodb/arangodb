////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication2/Version.h"
#include "Utils/DatabaseGuard.h"

#include <functional>
#include <string_view>

namespace arangodb {
class LogicalCollection;

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

struct IDatabaseProvider {
  virtual ~IDatabaseProvider() = default;

  /// @brief record a DDL change so the global schema version is bumped
  /// (the version is used to notify listeners, e.g. the agency, about DDL
  /// changes). The reason is used for tracing only.
  virtual void notifyDdlChange(char const* reason) = 0;

  virtual VocbasePtr useDatabase(std::string_view name) const = 0;
  virtual VocbasePtr useDatabase(TRI_voc_tick_t id) const = 0;

  virtual void enumerateDatabases(
      std::function<void(Database& vocbase)> const& func) = 0;

  virtual void inventory(
      velocypack::Builder& result, TRI_voc_tick_t,
      std::function<bool(LogicalCollection const*)> const& nameFilter) = 0;
  virtual replication::Version defaultReplicationVersion() const noexcept = 0;

  virtual bool extendedNames() const noexcept = 0;
  virtual void extendedNames(bool value) noexcept = 0;

  virtual void recoveryDone() = 0;

  // materializes databases from an engine inventory; called by the engine
  // itself once it's open, not by anything external.
  virtual void bootstrapDatabases(velocypack::Slice databases) = 0;
};

}  // namespace arangodb
