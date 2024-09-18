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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace arangodb {
namespace graph {

using VertexRef = arangodb::velocypack::HashedStringRef;
using VertexSet = arangodb::containers::HashSet<VertexRef, std::hash<VertexRef>,
                                                std::equal_to<VertexRef>>;

template<typename T>
concept HasForbidden = requires(T t) {
  {
    t.setForbiddenVertices(std::make_shared<VertexSet>())
    } -> std::same_as<void>;
};

}  // namespace graph
}  // namespace arangodb
