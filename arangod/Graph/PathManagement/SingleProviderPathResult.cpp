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
    Step* step, ProviderType& provider, PathStoreType& store)
    : _step(step), _provider(provider), _store(store) {
  TRI_ASSERT(_step != nullptr);
}

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
auto SingleProviderPathResult<ProviderType, PathStoreType, Step>::toVelocyPack(
    arangodb::velocypack::Builder& builder) -> void {
  if (_vertices.empty()) {
    _store.visitReversePath(*_step, [&](Step const& s) -> bool {
      prependVertex(s.getVertex());
      if (s.getEdge().isValid()) {
        prependEdge(s.getEdge());
      }
      return true;
    });
  }
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

template <class ProviderType, class PathStoreType, class Step>
auto SingleProviderPathResult<ProviderType, PathStoreType, Step>::toSchreierEntry(
    arangodb::velocypack::Builder& result, size_t& currentLength) -> void {
  if constexpr (!std::is_same_v<enterprise::SmartGraphStep, Step>) {  // TODO: eventually move to EE
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  } else {
    size_t prevIndex = 0;

    auto writeStepToBuilder = [&](Step& step) {
      VPackArrayBuilder arrayGuard(&result);
      // Create a VPackValue based on the char* inside the HashedStringRef, will save us a String copy each time.
      result.add(VPackValue(step.getVertex().getID().begin()));

      // Index position of previous step
      result.add(VPackValue(prevIndex));
      // The depth of the current step
      result.add(VPackValue(step.getDepth()));

      bool isResponsible = step.isResponsible(_provider.trx());
      // Print if we have a loose end here (looseEnd := this server is NOT responsible)
      result.add(VPackValue(!isResponsible));
      if (isResponsible) {
        // This server needs to provide the data for the vertex
        _provider.addVertexToBuilder(step.getVertex(), result);
      } else {
        result.add(VPackSlice::nullSlice());
      }

      _provider.addEdgeToBuilder(step.getEdge(), result);
    };  // TODO: Create method instead of lambda

    std::vector<Step*> toWrite{};

    _store.modifyReversePath(*_step, [&](Step& step) -> bool {
      if (!step.hasLocalSchreierIndex()) {
        toWrite.emplace_back(&step);
        return true;
      } else {
        prevIndex = step.getLocalSchreierIndex();
        return false;
      }
    });

    for (auto it = toWrite.rbegin(); it != toWrite.rend(); it++) {
      TRI_ASSERT(!(*it)->hasLocalSchreierIndex());
      writeStepToBuilder(**it);

      prevIndex = currentLength;
      (*it)->setLocalSchreierIndex(currentLength++);
    }
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

/* ClusterProvider Section */
template class ::arangodb::graph::SingleProviderPathResult<
    ::arangodb::graph::ClusterProvider, ::arangodb::graph::PathStore<::arangodb::graph::ClusterProvider::Step>,
    ::arangodb::graph::ClusterProvider::Step>;

template class ::arangodb::graph::SingleProviderPathResult<
    ::arangodb::graph::ProviderTracer<::arangodb::graph::ClusterProvider>,
    ::arangodb::graph::PathStoreTracer<::arangodb::graph::PathStore<::arangodb::graph::ClusterProvider::Step>>,
    ::arangodb::graph::ClusterProvider::Step>;
