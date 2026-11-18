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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>

#include <velocypack/Slice.h>

namespace arangodb {
namespace velocypack {
class Builder;
}  // namespace velocypack

namespace graph {

/// @brief Parse a weightAttribute option from a VelocyPack options object.
/// Accepts either a string (single attribute / dotted key name) or an array of
/// strings (nested attribute path). Returns an empty vector if unset.
std::vector<std::string> parseWeightAttribute(velocypack::Slice obj);

/// @brief Serialize a weightAttribute path as a VelocyPack array.
void addWeightAttribute(velocypack::Builder& builder,
                        std::vector<std::string> const& weightAttribute);

/// @brief Resolve the numeric weight of an edge using an attribute path.
/// Missing / non-numeric attributes yield defaultWeight.
double getEdgeWeight(velocypack::Slice edge,
                     std::vector<std::string> const& weightAttribute,
                     double defaultWeight);

}  // namespace graph
}  // namespace arangodb
