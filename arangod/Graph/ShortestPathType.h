////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_GRAPH_SHORTEST_PATH_TYPE_H
#define ARANGOD_GRAPH_SHORTEST_PATH_TYPE_H 1

namespace arangodb {
namespace graph {

struct ShortestPathType {
  enum class Type { KShortestPaths = 0, KPaths = 1 };

  // no need to create an object of it
  ShortestPathType() = delete;

  /// @brief get the type from a string
  static Type fromString(char const* value);

  /// @brief return the type as a string
  static char const* toString(Type value);
};

}  // namespace graph
}  // namespace arangodb

#endif
