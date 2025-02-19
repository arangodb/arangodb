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
/// @author Michael Hackstein
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "./PathResult.h"
#include "Basics/StaticStrings.h"

#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Providers/SingleServerProvider.h"

#include "Graph/Steps/SingleServerProviderStep.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#include "Enterprise/Graph/Providers/SmartGraphProvider.h"
#endif

#include <velocypack/Builder.h>

using namespace arangodb;
using namespace arangodb::graph;

template<class ProviderType, class Step>
PathResult<ProviderType, Step>::PathResult(ProviderType& sourceProvider,
                                           ProviderType& targetProvider)
    : _numVerticesFromSourceProvider(0),
      _numEdgesFromSourceProvider(0),
      _pathWeight(0.0),
      _sourceProvider(sourceProvider),
      _targetProvider(targetProvider) {}

template<class ProviderType, class Step>
auto PathResult<ProviderType, Step>::clear() -> void {
  _numVerticesFromSourceProvider = 0;
  _numEdgesFromSourceProvider = 0;
  _vertices.clear();
  _edges.clear();
  _weights.clear();
  _pathWeight = 0.0;
}

template<class ProviderType, class Step>
auto PathResult<ProviderType, Step>::appendVertex(typename Step::Vertex v)
    -> void {
  _vertices.push_back(std::move(v));
}

template<class ProviderType, class Step>
auto PathResult<ProviderType, Step>::prependVertex(typename Step::Vertex v)
    -> void {
  _numVerticesFromSourceProvider++;
  _vertices.insert(_vertices.begin(), std::move(v));
}

template<class ProviderType, class Step>
auto PathResult<ProviderType, Step>::appendEdge(typename Step::Edge e,
                                                double weight) -> void {
  _edges.push_back(std::move(e));
  _weights.push_back(weight);
}

template<class ProviderType, class Step>
auto PathResult<ProviderType, Step>::prependEdge(typename Step::Edge e,
                                                 double weight) -> void {
  _numEdgesFromSourceProvider++;
  _edges.insert(_edges.begin(), std::move(e));
  _weights.insert(_weights.begin(), weight);
}

template<class ProviderType, class Step>
auto PathResult<ProviderType, Step>::addWeight(double weight) -> void {
  _pathWeight += weight;
}

template<class ProviderType, class Step>
auto PathResult<ProviderType, Step>::toVelocyPack(
    arangodb::velocypack::Builder& builder, WeightType weightType) -> void {
  TRI_ASSERT(_numVerticesFromSourceProvider <= _vertices.size());
  VPackObjectBuilder path{&builder};
  {
    builder.add(VPackValue(StaticStrings::GraphQueryVertices));
    VPackArrayBuilder vertices{&builder};
    // Write first part of the Path
    for (size_t i = 0; i < _numVerticesFromSourceProvider; i++) {
      _sourceProvider.addVertexToBuilder(_vertices[i], builder);
    }
    // Write second part of the Path
    for (size_t i = _numVerticesFromSourceProvider; i < _vertices.size(); i++) {
      _targetProvider.addVertexToBuilder(_vertices[i], builder);
    }
  }

  {
    builder.add(VPackValue(StaticStrings::GraphQueryEdges));
    VPackArrayBuilder edges(&builder);
    // Write first part of the Path
    for (size_t i = 0; i < _numEdgesFromSourceProvider; i++) {
      _sourceProvider.addEdgeToBuilder(_edges[i], builder);
    }
    // Write second part of the Path
    for (size_t i = _numEdgesFromSourceProvider; i < _edges.size(); i++) {
      _targetProvider.addEdgeToBuilder(_edges[i], builder);
    }
  }

  if (weightType != WeightType::NONE) {
    // We need to handle two different cases here. In case no weight callback
    // has been set, we need to write the number of edges here. In case a weight
    // callback is set, we need to set the calculated weight.
    if (weightType == WeightType::AMOUNT_EDGES) {
      // Case 1) Amount of edges (as will be seen as weight=1 per edge)
      builder.add(StaticStrings::GraphQueryWeight, VPackValue(_edges.size()));
    } else if (weightType == WeightType::ACTUAL_WEIGHT) {
      // Case 2) Calculated weight (currently passed as parameter)
      builder.add(StaticStrings::GraphQueryWeight, VPackValue(_pathWeight));
    }
  }
}

template<class ProviderType, class Step>
auto PathResult<ProviderType, Step>::isEqualEdgeRepresentation(
    PathResult<ProviderType, Step> const& other) -> bool {
  if (_edges.size() == other._edges.size()) {
    for (size_t i = 0; i < _edges.size(); i++) {
      if (!_edges[i].getID().equals(other._edges[i].getID())) {
        return false;
      }
    }
    return true;
  }
  return false;
}

template<class ProviderType, class Step>
auto PathResult<ProviderType, Step>::lastVertexToVelocyPack(
    arangodb::velocypack::Builder& builder) -> void {
  _sourceProvider.addVertexToBuilder(_vertices.back(), builder);
}

template<class ProviderType, class Step>
auto PathResult<ProviderType, Step>::lastEdgeToVelocyPack(
    arangodb::velocypack::Builder& builder) -> void {
  _sourceProvider.addEdgeToBuilder(_edges.back(), builder);
}

template<class ProviderType, class Step>
auto PathResult<ProviderType, Step>::isEmpty() const -> bool {
  return _vertices.empty();
}

template<class ProviderType, class Step>
auto PathResult<ProviderType, Step>::getMemoryUsage() const -> size_t {
  size_t mem = 0;
  mem += sizeof(PathResult<ProviderType, Step>);
  mem += sizeof(typename Step::Vertex) * _vertices.size();
  mem += sizeof(typename Step::Edge) * _edges.size();
  mem += sizeof(double) * _weights.size();
  return mem;
}

template<class ProviderType, class Step>
auto PathResult<ProviderType, Step>::compare(
    PathResult<ProviderType, Step> const& other) -> std::strong_ordering {
  if (_pathWeight > other._pathWeight) {
    // The ">" here is intentional! We want descending weight!
    return std::strong_ordering::less;
  }
  if (_pathWeight < other._pathWeight) {
    return std::strong_ordering::greater;
  }
  // Now let's take the len-lexicographic ordering of the edges. We do not
  // have to consider the vertices, since if the edges are equal, then
  // their end vertices are equal:
  if (_edges.size() != other._edges.size()) {
    return _edges.size() <=> other._edges.size();
  }
  for (size_t i = 0; i < _edges.size(); ++i) {
    auto c = _edges[i].getID().compare(other._edges[i].getID());
    if (c != 0) {
      return c < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
    }
  }
  // Equality:
  return std::strong_ordering::equal;
}

/* SingleServerProvider Section */

using SingleServerProviderStep = ::arangodb::graph::SingleServerProviderStep;

template class ::arangodb::graph::PathResult<
    ::arangodb::graph::SingleServerProvider<SingleServerProviderStep>,
    SingleServerProviderStep>;
template class ::arangodb::graph::PathResult<
    ::arangodb::graph::ProviderTracer<
        SingleServerProvider<SingleServerProviderStep>>,
    SingleServerProviderStep>;

#ifdef USE_ENTERPRISE
template class ::arangodb::graph::PathResult<
    ::arangodb::graph::SingleServerProvider<enterprise::SmartGraphStep>,
    enterprise::SmartGraphStep>;
template class ::arangodb::graph::PathResult<
    ::arangodb::graph::ProviderTracer<
        SingleServerProvider<enterprise::SmartGraphStep>>,
    enterprise::SmartGraphStep>;
#endif

/* ClusterProvider Section */

template class ::arangodb::graph::PathResult<
    ::arangodb::graph::ClusterProvider<ClusterProviderStep>,
    ::arangodb::graph::ClusterProviderStep>;

template class ::arangodb::graph::PathResult<
    ::arangodb::graph::ProviderTracer<
        ::arangodb::graph::ClusterProvider<ClusterProviderStep>>,
    ::arangodb::graph::ClusterProviderStep>;

#ifdef USE_ENTERPRISE
template class ::arangodb::graph::PathResult<
    ::arangodb::graph::enterprise::SmartGraphProvider<ClusterProviderStep>,
    ClusterProviderStep>;
template class ::arangodb::graph::PathResult<
    ::arangodb::graph::ProviderTracer<
        enterprise::SmartGraphProvider<ClusterProviderStep>>,
    ClusterProviderStep>;
#endif
