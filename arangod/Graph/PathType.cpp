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

#include "Graph/PathType.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

namespace arangodb::graph {

constexpr std::string_view KShortestPathsName = "K_SHORTEST_PATHS";
constexpr std::string_view KPathsName = "K_PATHS";
constexpr std::string_view AllShortestPathsName = "ALL_SHORTEST_PATHS";
constexpr std::string_view ShortestPathName = "SHORTEST_PATH";

/// @brief get the type from a string
/*static*/ PathType::Type PathType::fromString(std::string_view value) {
  if (value == KShortestPathsName) {
    return Type::KShortestPaths;
  }
  if (value == KPathsName) {
    return Type::KPaths;
  }
  if (value == AllShortestPathsName) {
    return Type::AllShortestPaths;
  }
  if (value == ShortestPathName) {
    return Type::ShortestPath;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "invalid shortest path type");
}

/// @brief return the type as a string
/*static*/ std::string_view PathType::toString(PathType::Type value) {
  switch (value) {
    case Type::KShortestPaths:
      return KShortestPathsName;
    case Type::KPaths:
      return KPathsName;
    case Type::AllShortestPaths:
      return AllShortestPathsName;
    case Type::ShortestPath:
      return ShortestPathName;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid path type");
}

}  // namespace arangodb::graph
