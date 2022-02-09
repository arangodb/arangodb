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

#include "./TwoSidedPathResult.h"
#include "Basics/StaticStrings.h"

#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Providers/SingleServerProvider.h"

#include "Graph/Steps/SingleServerProviderStep.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#endif

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

template<class ProviderType, class Step>
TwoSidedPathResult<ProviderType, Step>::TwoSidedPathResult(
    ProviderType& sourceProvider, ProviderType& targetProvider)
    : _sourceProvider(sourceProvider), _targetProvider(targetProvider) {}

template<class ProviderType, class Step>
auto TwoSidedPathResult<ProviderType, Step>::clear() -> void {
  _sourceVertices.clear();
  _targetVertices.clear();
  _sourceEdges.clear();
  _targetEdges.clear();
}

template<class ProviderType, class Step>
auto TwoSidedPathResult<ProviderType, Step>::appendVertex(
    typename Step::Vertex v) -> void {
  _targetVertices.push_back(std::move(v));
}

template<class ProviderType, class Step>
auto TwoSidedPathResult<ProviderType, Step>::prependVertex(
    typename Step::Vertex v) -> void {
  _sourceVertices.push_back(std::move(v));
}

template<class ProviderType, class Step>
auto TwoSidedPathResult<ProviderType, Step>::appendEdge(typename Step::Edge e)
    -> void {
  _targetEdges.push_back(std::move(e));
}

template<class ProviderType, class Step>
auto TwoSidedPathResult<ProviderType, Step>::prependEdge(typename Step::Edge e)
    -> void {
  _sourceEdges.push_back(std::move(e));
}

template<class ProviderType, class Step>
auto TwoSidedPathResult<ProviderType, Step>::toVelocyPack(
    arangodb::velocypack::Builder& builder) -> void {
  VPackObjectBuilder path{&builder};
  {
    builder.add(VPackValue(StaticStrings::GraphQueryVertices));
    VPackArrayBuilder vertices{&builder};
    // Write the source (first) part of the Path. The vertices have been written
    // in the inverse order, so read now from the end.
    for (auto rit = std::rbegin(_sourceVertices);
         rit != std::rend(_sourceVertices); ++rit) {
      _sourceProvider.addVertexToBuilder(*rit, builder);
    }
    // Write the target (second) part of the Path. The vertices have been
    // written in the order as they appear on the path, so read them from the
    // beginning.
    for (auto const& v : _targetVertices) {
      _targetProvider.addVertexToBuilder(v, builder);
    }
  }

  {
    builder.add(VPackValue(StaticStrings::GraphQueryEdges));
    VPackArrayBuilder edges(&builder);
    // similar to the vertices
    for (auto rit = std::rbegin(_sourceEdges); rit != std::rend(_sourceEdges);
         ++rit) {
      _sourceProvider.addEdgeToBuilder(*rit, builder);
    }
    // similar to the vertices
    for (auto const& e : _targetEdges) {
      _targetProvider.addEdgeToBuilder(e, builder);
    }
  }
}

template<class ProviderType, class Step>
auto TwoSidedPathResult<ProviderType, Step>::lastVertexToVelocyPack(
    arangodb::velocypack::Builder& builder) -> void {
  TRI_ASSERT(!(_sourceEdges.empty() && _targetVertices.empty()));
  if (ADB_UNLIKELY(_targetVertices.empty())) {
    // source vertices are stored in the inverse order of the path order,
    // so, take the first vertex
    _sourceProvider.addVertexToBuilder(_sourceVertices.front(), builder);
  } else {
    _sourceProvider.addVertexToBuilder(_targetVertices.back(), builder);
  }
}

template<class ProviderType, class Step>
auto TwoSidedPathResult<ProviderType, Step>::lastEdgeToVelocyPack(
    arangodb::velocypack::Builder& builder) -> void {
  TRI_ASSERT(!(_sourceEdges.empty() && _targetEdges.empty()));
  if (ADB_UNLIKELY(_targetEdges.empty())) {
    // source edges are stored in the inverse order of the path order,
    // so, take the first edge
    _sourceProvider.addEdgeToBuilder(_sourceEdges.front(), builder);
  } else {
    _sourceProvider.addEdgeToBuilder(_targetEdges.back(), builder);
  }
}

template<class ProviderType, class Step>
auto TwoSidedPathResult<ProviderType, Step>::isEmpty() const -> bool {
  return _sourceVertices.empty() && _targetVertices.empty();
}

/* SingleServerProvider Section */

using SingleServerProviderStep = ::arangodb::graph::SingleServerProviderStep;

template class ::arangodb::graph::TwoSidedPathResult<
    ::arangodb::graph::SingleServerProvider<SingleServerProviderStep>,
    SingleServerProviderStep>;
template class ::arangodb::graph::TwoSidedPathResult<
    ::arangodb::graph::ProviderTracer<
        SingleServerProvider<SingleServerProviderStep>>,
    SingleServerProviderStep>;

#ifdef USE_ENTERPRISE
template class ::arangodb::graph::TwoSidedPathResult<
    ::arangodb::graph::SingleServerProvider<enterprise::SmartGraphStep>,
    enterprise::SmartGraphStep>;
template class ::arangodb::graph::TwoSidedPathResult<
    ::arangodb::graph::ProviderTracer<
        SingleServerProvider<enterprise::SmartGraphStep>>,
    enterprise::SmartGraphStep>;
#endif

/* ClusterProvider Section */

template class ::arangodb::graph::TwoSidedPathResult<
    ::arangodb::graph::ClusterProvider<ClusterProviderStep>,
    ::arangodb::graph::ClusterProviderStep>;

template class ::arangodb::graph::TwoSidedPathResult<
    ::arangodb::graph::ProviderTracer<
        ::arangodb::graph::ClusterProvider<ClusterProviderStep>>,
    ::arangodb::graph::ClusterProviderStep>;