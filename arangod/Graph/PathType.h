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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string_view>

namespace arangodb::graph {

struct PathType {
  enum class Type {
    KShortestPaths = 0,
    KPaths = 1,
    AllShortestPaths = 2,
    ShortestPath = 3
  };

  // no need to create an object of it
  PathType() = delete;

  /// @brief get the type from a string
  static Type fromString(std::string_view value);

  /// @brief return the type as a string
  static std::string_view toString(Type value);
};

}  // namespace arangodb::graph
