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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Auth/Common.h"

#include <string>
#include <variant>

namespace arangodb {

enum class AccessLevel { None = 0, Read, WriteData, WriteMeta };

std::string_view convertFromAccessLevel(AccessLevel level);

struct Permission {
  struct Database {
    std::string name;
    // TODO Database permissions don't differentiate between WriteData & WriteMeta:
    //      use a different enum!
    AccessLevel level;
  };
  struct DataSource {
    std::string database;
    std::string name;
    AccessLevel level;
  };
  struct Admin {
    // TODO
  };

  using Any = std::variant<Database, DataSource, Admin>;

  Any permission;
};

}  // namespace arangodb
