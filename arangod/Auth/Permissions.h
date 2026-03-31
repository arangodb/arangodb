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

enum class CollectionAccessLevel { None = 0, Read, WriteData, WriteMeta };
enum class DatabaseAccessLevel { None = 0, Read, Write };
enum class ViewAccessLevel { None = 0, Read, Modify };
enum class AnalyzerAccessLevel { None = 0, Read, Modify };

using AccessLevel = CollectionAccessLevel;

auto to_string(CollectionAccessLevel level) -> std::string_view;
auto to_string(DatabaseAccessLevel level) -> std::string_view;
auto to_string(ViewAccessLevel level) -> std::string_view;
auto to_string(AnalyzerAccessLevel level) -> std::string_view;

struct Permission {
  struct Database {
    std::string name;
    DatabaseAccessLevel level;
  };
  struct Collection {
    std::string database;
    std::string name;
    CollectionAccessLevel level;
  };
  struct View {
    std::string database;
    std::string name;
    ViewAccessLevel level;
  };
  struct Analyzer {
    std::string database;
    std::string name;
    AnalyzerAccessLevel level;
  };
  struct Admin {
    // TODO
  };

  using Any = std::variant<Database, Collection, View, Analyzer, Admin>;

  Any permission;
};

}  // namespace arangodb
