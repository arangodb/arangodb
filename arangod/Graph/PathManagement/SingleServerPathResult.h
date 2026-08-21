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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Graph/PathManagement/IPathResult.h"
#include "Basics/StaticStrings.h"
#include "Inspection/VPack.h"

#include <vector>

namespace arangodb {
namespace velocypack {
class HashedStringRef;
class Builder;
}  // namespace velocypack

namespace graph {
using VertexId = std::string;

struct Edge {
  VertexId _from;
  VertexId _to;
  double _weight;
  bool operator==(Edge const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, Edge& x) {
  return f.object(x).fields(f.field("from", x._from), f.field("to", x._to),
                            f.field("weight", x._weight));
}

class SingleServerPathResult : public IPathResult {
 public:
  SingleServerPathResult() {}
  SingleServerPathResult(std::vector<std::optional<VertexId>> vertices,
                         std::vector<Edge> edges)
      : _vertices{std::move(vertices)}, _edges{std::move(edges)} {}
  ~SingleServerPathResult() = default;
  bool operator==(SingleServerPathResult const& other) const {
    if (other._vertices == _vertices && other._edges == _edges &&
        other._weights == _weights) {
      return true;
    }
    return false;
  }

  auto toVelocyPack(velocypack::Builder& builder) -> void override {
    arangodb::velocypack::serialize(builder, *this);
  };
  auto lastVertexToVelocyPack(velocypack::Builder& builder) -> void override{};
  auto lastEdgeToVelocyPack(velocypack::Builder& builder) -> void override{};

  std::vector<std::optional<VertexId>> _vertices;
  std::vector<Edge> _edges;
  std::vector<double> _weights;
};
template<typename Inspector>
auto inspect(Inspector& f, SingleServerPathResult& x) {
  return f.object(x).fields(
      f.field(StaticStrings::GraphQueryVertices, x._vertices),
      f.field(StaticStrings::GraphQueryEdges, x._edges),
      f.field(StaticStrings::GraphQueryWeights, x._weights));
}

}  // namespace graph
}  // namespace arangodb
