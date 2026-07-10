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
}

struct IDatabaseProvider {
  virtual ~IDatabaseProvider() = default;

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
};

}  // namespace arangodb
