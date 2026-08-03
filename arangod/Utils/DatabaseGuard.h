////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

#include "VocBase/voc-types.h"

namespace arangodb {
struct Database;
class DatabaseFeature;

struct IDatabaseGuard {
  virtual ~IDatabaseGuard() = default;
  [[nodiscard]] virtual Database& database() const noexcept = 0;
};

struct VocbaseReleaser {
  void operator()(Database* vocbase) const noexcept;
};

using VocbasePtr = std::unique_ptr<Database, VocbaseReleaser>;

/// @brief Scope guard for a database, ensures that it is not
///        dropped while still using it.
class DatabaseGuard final : public IDatabaseGuard {
 public:
  /// @brief create guard on existing db
  explicit DatabaseGuard(Database& vocbase);

  /// @brief create guard from existing VocbasePtr
  explicit DatabaseGuard(VocbasePtr vocbase);

  /// @brief create the guard, using a database id
  DatabaseGuard(DatabaseFeature& feature, TRI_voc_tick_t id);

  /// @brief create the guard, using a database name
  DatabaseGuard(DatabaseFeature& feature, std::string_view name);

  /// @brief return the database pointer
  Database& database() const noexcept final { return *_vocbase; }
  Database const* operator->() const noexcept { return _vocbase.get(); }
  Database* operator->() noexcept { return _vocbase.get(); }

 private:
  VocbasePtr _vocbase;
};

}  // namespace arangodb
