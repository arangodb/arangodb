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

#include "./SingleProviderPathResult.h"
#include "Basics/StaticStrings.h"

#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Providers/SingleServerProvider.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

template <class ProviderType, class Step>
SingleProviderPathResult<ProviderType, Step>::SingleProviderPathResult(ProviderType& provider)
    : _provider(provider) {}

template <class ProviderType, class Step>
auto SingleProviderPathResult<ProviderType, Step>::clear() -> void {
  _vertices.clear();
  _edges.clear();
}

template <class ProviderType, class Step>
auto SingleProviderPathResult<ProviderType, Step>::appendVertex(typename Step::Vertex v) -> void {
  _vertices.push_back(std::move(v));
}

template <class ProviderType, class Step>
auto SingleProviderPathResult<ProviderType, Step>::prependVertex(typename Step::Vertex v)
    -> void {
  _vertices.insert(_vertices.begin(), std::move(v));
}

template <class ProviderType, class Step>
auto SingleProviderPathResult<ProviderType, Step>::appendEdge(typename Step::Edge e) -> void {
  _edges.push_back(std::move(e));
}

template <class ProviderType, class Step>
auto SingleProviderPathResult<ProviderType, Step>::prependEdge(typename Step::Edge e) -> void {
  _edges.insert(_edges.begin(), std::move(e));
}

template <class ProviderType, class Step>
auto SingleProviderPathResult<ProviderType, Step>::toVelocyPack(arangodb::velocypack::Builder& builder)
    -> void {
  VPackObjectBuilder path{&builder};
  {
    builder.add(VPackValue(StaticStrings::GraphQueryVertices));
    VPackArrayBuilder vertices{&builder};
    // Write first part of the Path
    for (size_t i = 0; i < _vertices.size(); i++) {
      _provider.addVertexToBuilder(_vertices[i], builder);
    }
  }

  {
    builder.add(VPackValue(StaticStrings::GraphQueryEdges));
    VPackArrayBuilder edges(&builder);
    // Write first part of the Path
    for (size_t i = 0; i < _edges.size(); i++) {
      _provider.addEdgeToBuilder(_edges[i], builder);
    }
  }
}

template <class ProviderType, class Step>
auto SingleProviderPathResult<ProviderType, Step>::isEmpty() const -> bool {
  return _vertices.empty();
}

/* SingleServerProvider Section */

template class ::arangodb::graph::SingleProviderPathResult<::arangodb::graph::SingleServerProvider,
                                                   ::arangodb::graph::SingleServerProvider::Step>;

template class ::arangodb::graph::SingleProviderPathResult<::arangodb::graph::ProviderTracer<::arangodb::graph::SingleServerProvider>,
                                                   ::arangodb::graph::SingleServerProvider::Step>;

/* ClusterProvider Section */

template class ::arangodb::graph::SingleProviderPathResult<::arangodb::graph::ClusterProvider, ::arangodb::graph::ClusterProvider::Step>;

template class ::arangodb::graph::SingleProviderPathResult<::arangodb::graph::ProviderTracer<::arangodb::graph::ClusterProvider>,
                                                   ::arangodb::graph::ClusterProvider::Step>;