////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Graph/PathType.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

#include <cstring>

namespace arangodb {
namespace graph {

constexpr char const* KShortestPathsName = "K_SHORTEST_PATHS";
constexpr char const* KPathsName = "K_PATHS";
constexpr char const* AllShortestPathsName = "ALL_SHORTEST_PATHS";
constexpr char const* ShortestPathName = "SHORTEST_PATH";

/// @brief get the type from a string
/*static*/ PathType::Type PathType::fromString(char const* value) {
  if (strcmp(value, KShortestPathsName) == 0) {
    return Type::KShortestPaths;
  }
  if (strcmp(value, KPathsName) == 0) {
    return Type::KPaths;
  }
  if (strcmp(value, AllShortestPathsName) == 0) {
    return Type::AllShortestPaths;
  }
  if (strcmp(value, ShortestPathName) == 0) {
    return Type::ShortestPath;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "invalid shortest path type");
}

/// @brief return the type as a string
/*static*/ char const* PathType::toString(PathType::Type value) {
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

}  // namespace graph
}  // namespace arangodb
