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
/// @author Michael Hackstein
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "ClusterProviderStep.h"

using namespace arangodb::graph;

namespace arangodb::graph {
auto operator<<(std::ostream& out, ClusterProviderStep const& step)
    -> std::ostream& {
  out << step._vertex.getID();
  return out;
}
}  // namespace arangodb::graph

ClusterProviderStep::ClusterProviderStep(const VertexType& v)
    : _vertex(v), _edge(), _fetchedStatus(FetchedType::UNFETCHED) {}

ClusterProviderStep::ClusterProviderStep(const VertexType& v,
                                         const EdgeType& edge, size_t prev)
    : BaseStep(prev),
      _vertex(v),
      _edge(edge),
      _fetchedStatus(FetchedType::UNFETCHED) {}

ClusterProviderStep::ClusterProviderStep(VertexType v, EdgeType edge,
                                         size_t prev, FetchedType fetchedStatus)
    : BaseStep(prev),
      _vertex(std::move(v)),
      _edge(std::move(edge)),
      _fetchedStatus(fetchedStatus) {}

ClusterProviderStep::ClusterProviderStep(VertexType v, EdgeType edge,
                                         size_t prev, FetchedType fetchedStatus,
                                         size_t depth)
    : BaseStep(prev, depth),
      _vertex(std::move(v)),
      _edge(std::move(edge)),
      _fetchedStatus(fetchedStatus) {}

ClusterProviderStep::ClusterProviderStep(VertexType v, EdgeType edge,
                                         size_t prev, FetchedType fetched,
                                         size_t depth, double weight)
    : BaseStep(prev, depth, weight),
      _vertex(std::move(v)),
      _edge(std::move(edge)),
      _fetchedStatus(fetched) {}

ClusterProviderStep::ClusterProviderStep(VertexType v, size_t depth,
                                         double weight)
    : BaseStep(std::numeric_limits<size_t>::max(), depth, weight),
      _vertex(std::move(v)),
      _edge(),
      _fetchedStatus(FetchedType::UNFETCHED) {}

ClusterProviderStep::~ClusterProviderStep() = default;

VertexType const& ClusterProviderStep::Vertex::getID() const { return _vertex; }

ClusterProviderStep::EdgeType const& ClusterProviderStep::Edge::getID() const {
  return _edge;
}
bool ClusterProviderStep::Edge::isValid() const { return !_edge.empty(); };
