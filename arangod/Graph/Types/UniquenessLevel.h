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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GRAPH_UNIQUENESSLEVEL_H
#define ARANGOD_GRAPH_UNIQUENESSLEVEL_H 1

#include <iosfwd>

namespace arangodb {
namespace graph {

// Note: These two enums are identical.
// We just use different enums here to make the usage code
// easier to reason about, and to avoid accidential missuse of
// any of them.
enum class VertexUniquenessLevel { NONE, PATH, GLOBAL };

enum class EdgeUniquenessLevel { NONE, PATH, GLOBAL };

std::ostream& operator<<(std::ostream& stream, VertexUniquenessLevel const& level);
std::ostream& operator<<(std::ostream& stream, EdgeUniquenessLevel const& level);

}  // namespace graph
}  // namespace arangodb

#endif