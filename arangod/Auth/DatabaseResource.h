////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Auth/Resource.h"

#include <string>

namespace arangodb {
namespace auth {
class DatabaseResource : public Resource {
 public:
  DatabaseResource() : _database() {}

  explicit DatabaseResource(std::string const& database)
      : _database(database) {}

  explicit DatabaseResource(char const* database)
      : _database(database) {}

  DatabaseResource(std::string&& database) : _database(std::move(database)) {}

  template <typename D>
  explicit DatabaseResource(D const& database) : _database(database.name()) {}

  bool equals(DatabaseResource const& other) const {
    return _database == other._database;
  }

  std::string const _database;
};
}  // namespace auth
}  // namespace arangodb
