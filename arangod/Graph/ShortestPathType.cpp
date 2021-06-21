////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Graph/ShortestPathType.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

#include <cstring>

namespace arangodb {
namespace graph {

constexpr char const* KShortestPathsName = "K_SHORTEST_PATHS";
constexpr char const* KPathsName = "K_PATHS";

/// @brief get the type from a string
/*static*/ ShortestPathType::Type ShortestPathType::fromString(char const* value) {
  if (strcmp(value, KShortestPathsName) == 0) {
    return Type::KShortestPaths;
  }
  if (strcmp(value, KPathsName) == 0) {
    return Type::KPaths;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid shortest path type");
}

/// @brief return the type as a string
/*static*/ char const* ShortestPathType::toString(ShortestPathType::Type value) {
  switch (value) {
    case Type::KShortestPaths:
      return KShortestPathsName;
    case Type::KPaths:
      return KPathsName;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid shortest path type");
}

}  // namespace graph
}  // namespace arangodb
