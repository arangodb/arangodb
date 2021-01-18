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

#include "./PathResult.h"
#include "Basics/StaticStrings.h"

#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Providers/SingleServerProvider.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

template <class ProviderType, class Step>
PathResult<ProviderType, Step>::PathResult(ProviderType& sourceProvider, ProviderType& targetProvider)
    : _numVerticesFromSourceProvider(0),
      _numEdgesFromSourceProvider(0),
      _sourceProvider(sourceProvider),
      _targetProvider(targetProvider) {}

template <class ProviderType, class Step>
auto PathResult<ProviderType, Step>::clear() -> void {
  _numVerticesFromSourceProvider = 0;
  _numEdgesFromSourceProvider = 0;
  _vertices.clear();
  _edges.clear();
}

template <class ProviderType, class Step>
auto PathResult<ProviderType, Step>::appendVertex(typename Step::Vertex v) -> void {
  _vertices.push_back(std::move(v));
}

template <class ProviderType, class Step>
auto PathResult<ProviderType, Step>::prependVertex(typename Step::Vertex v) -> void {
  _numVerticesFromSourceProvider++;
  _vertices.insert(_vertices.begin(), std::move(v));
}

template <class ProviderType, class Step>
auto PathResult<ProviderType, Step>::appendEdge(typename Step::Edge e) -> void {
  _edges.push_back(std::move(e));
}

template <class ProviderType, class Step>
auto PathResult<ProviderType, Step>::prependEdge(typename Step::Edge e) -> void {
  _numEdgesFromSourceProvider++;
  _edges.insert(_edges.begin(), std::move(e));
}

// NOTE:
// Potential optimization: Instead of counting on each append
// We can do a size call to the vector when switching the Provider.

template <class ProviderType, class Step>
auto PathResult<ProviderType, Step>::toVelocyPack(arangodb::velocypack::Builder& builder)
    -> void {
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
}

template <class ProviderType, class Step>
auto PathResult<ProviderType, Step>::isEmpty() const -> bool {
  return _vertices.empty();
}

/* SingleServerProvider Section */

template class ::arangodb::graph::PathResult<::arangodb::graph::SingleServerProvider,
                                             ::arangodb::graph::SingleServerProvider::Step>;

template class ::arangodb::graph::PathResult<::arangodb::graph::ProviderTracer<::arangodb::graph::SingleServerProvider>,
                                             ::arangodb::graph::SingleServerProvider::Step>;

/* ClusterProvider Section */

template class ::arangodb::graph::PathResult<::arangodb::graph::ClusterProvider, ::arangodb::graph::ClusterProvider::Step>;

template class ::arangodb::graph::PathResult<::arangodb::graph::ProviderTracer<::arangodb::graph::ClusterProvider>,
                                             ::arangodb::graph::ClusterProvider::Step>;