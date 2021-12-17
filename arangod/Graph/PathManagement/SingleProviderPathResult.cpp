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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "SingleProviderPathResult.h"
#include "Basics/StaticStrings.h"

#include "Graph/PathManagement/PathStore.h"
#include "Graph/PathManagement/PathStoreTracer.h"
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

namespace arangodb::graph::enterprise {
class SmartGraphStep;
}  // namespace arangodb::graph::enterprise

template <class ProviderType, class PathStoreType, class Step>
SingleProviderPathResult<ProviderType, PathStoreType, Step>::SingleProviderPathResult(
    Step step, ProviderType& provider, PathStoreType& store)
    : _step(std::move(step)), _provider(provider), _store(store) {}

template <class ProviderType, class PathStoreType, class Step>
auto SingleProviderPathResult<ProviderType, PathStoreType, Step>::clear() -> void {
  _vertices.clear();
  _edges.clear();
}

template <class ProviderType, class PathStoreType, class Step>
auto SingleProviderPathResult<ProviderType, PathStoreType, Step>::appendVertex(typename Step::Vertex v)
    -> void {
  _vertices.push_back(std::move(v));
}

template <class ProviderType, class PathStoreType, class Step>
auto SingleProviderPathResult<ProviderType, PathStoreType, Step>::prependVertex(typename Step::Vertex v)
    -> void {
  _vertices.insert(_vertices.begin(), std::move(v));
}

template <class ProviderType, class PathStoreType, class Step>
auto SingleProviderPathResult<ProviderType, PathStoreType, Step>::appendEdge(typename Step::Edge e)
    -> void {
  _edges.push_back(std::move(e));
}

template <class ProviderType, class PathStoreType, class Step>
auto SingleProviderPathResult<ProviderType, PathStoreType, Step>::prependEdge(typename Step::Edge e)
    -> void {
  _edges.insert(_edges.begin(), std::move(e));
}

template <class ProviderType, class PathStoreType, class Step>
auto SingleProviderPathResult<ProviderType, PathStoreType, Step>::populatePath() -> void {
  _store.visitReversePath(_step, [&](Step const& s) -> bool {
    prependVertex(s.getVertex());
    if (s.getEdge().isValid()) {
      prependEdge(s.getEdge());
    }
    return true;
  });
}

template <class ProviderType, class PathStoreType, class Step>
auto SingleProviderPathResult<ProviderType, PathStoreType, Step>::verticesToVelocyPack(
    arangodb::velocypack::Builder& builder) -> void {
  TRI_ASSERT(builder.isOpenObject());
  {
    builder.add(VPackValue(StaticStrings::GraphQueryVertices));
    VPackArrayBuilder vertices{&builder};
    // Write first part of the Path
    for (size_t i = 0; i < _vertices.size(); i++) {
      _provider.addVertexToBuilder(_vertices[i], builder);
    }
  }
}

template <class ProviderType, class PathStoreType, class Step>
auto SingleProviderPathResult<ProviderType, PathStoreType, Step>::edgesToVelocyPack(
    arangodb::velocypack::Builder& builder) -> void {
  TRI_ASSERT(builder.isOpenObject());
  {
    builder.add(VPackValue(StaticStrings::GraphQueryEdges));
    VPackArrayBuilder edges(&builder);
    // Write first part of the Path
    for (size_t i = 0; i < _edges.size(); i++) {
      _provider.addEdgeToBuilder(_edges[i], builder);
    }
  }
}

template <class ProviderType, class PathStoreType, class Step>
auto SingleProviderPathResult<ProviderType, PathStoreType, Step>::toVelocyPack(
    arangodb::velocypack::Builder& builder) -> void {
  if (_vertices.empty()) {
    populatePath();
  }
  VPackObjectBuilder path{&builder};
  verticesToVelocyPack(builder);
  edgesToVelocyPack(builder);
}

template <class ProviderType, class PathStoreType, class Step>
auto SingleProviderPathResult<ProviderType, PathStoreType, Step>::lastVertexToVelocyPack(
    arangodb::velocypack::Builder& builder) -> void {
  if (_vertices.empty()) {
    populatePath();
  }

  // TODO [GraphRefactor]: Check for a more elegant solution here
  if (_vertices.empty()) {
    // We can never hand out invalid ids.
    // For production just be sure to add something sensible.
    builder.add(VPackSlice::nullSlice());
  } else {
    _provider.addVertexToBuilder(_vertices[_vertices.size() - 1], builder);
  }
}

template <class ProviderType, class PathStoreType, class Step>
auto SingleProviderPathResult<ProviderType, PathStoreType, Step>::lastEdgeToVelocyPack(
    arangodb::velocypack::Builder& builder) -> void {
  if (_edges.empty()) {
    populatePath();
  }

  // TODO [GraphRefactor]: Check for a more elegant solution here
  if (_edges.empty()) {
    // We can never hand out invalid ids.
    // For production just be sure to add something sensible.
    builder.add(VPackSlice::nullSlice());
  } else {
    _provider.addEdgeToBuilder(_edges[_edges.size() - 1], builder);
  }
}

template <class ProviderType, class PathStoreType, class Step>
auto SingleProviderPathResult<ProviderType, PathStoreType, Step>::isEmpty() const
    -> bool {
  return false;
}

/* SingleServerProvider Section */
using SingleServerProviderStep = ::arangodb::graph::SingleServerProviderStep;

template class ::arangodb::graph::SingleProviderPathResult<
    ::arangodb::graph::SingleServerProvider<SingleServerProviderStep>,
    ::arangodb::graph::PathStore<SingleServerProviderStep>, SingleServerProviderStep>;

template class ::arangodb::graph::SingleProviderPathResult<
    ::arangodb::graph::ProviderTracer<::arangodb::graph::SingleServerProvider<SingleServerProviderStep>>,
    ::arangodb::graph::PathStoreTracer<::arangodb::graph::PathStore<SingleServerProviderStep>>, SingleServerProviderStep>;

#ifdef USE_ENTERPRISE
template class ::arangodb::graph::SingleProviderPathResult<
    ::arangodb::graph::SingleServerProvider<enterprise::SmartGraphStep>,
    ::arangodb::graph::PathStore<enterprise::SmartGraphStep>, enterprise::SmartGraphStep>;

template class ::arangodb::graph::SingleProviderPathResult<
    ::arangodb::graph::ProviderTracer<::arangodb::graph::SingleServerProvider<enterprise::SmartGraphStep>>,
    ::arangodb::graph::PathStoreTracer<::arangodb::graph::PathStore<enterprise::SmartGraphStep>>, enterprise::SmartGraphStep>;
#endif

// TODO: check if cluster is needed here
/* ClusterProvider Section */
template class ::arangodb::graph::SingleProviderPathResult<
    ::arangodb::graph::ClusterProvider, ::arangodb::graph::PathStore<::arangodb::graph::ClusterProvider::Step>,
    ::arangodb::graph::ClusterProvider::Step>;

template class ::arangodb::graph::SingleProviderPathResult<
    ::arangodb::graph::ProviderTracer<::arangodb::graph::ClusterProvider>,
    ::arangodb::graph::PathStoreTracer<::arangodb::graph::PathStore<::arangodb::graph::ClusterProvider::Step>>,
    ::arangodb::graph::ClusterProvider::Step>;
