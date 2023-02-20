////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "Graph/Steps/SingleServerProviderStep.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#include "Enterprise/Graph/Providers/SmartGraphProvider.h"
#include "Logger/LogMacros.h"
#endif

#include <velocypack/Builder.h>

using namespace arangodb;
using namespace arangodb::graph;

template<class ProviderType, class Step>
PathResult<ProviderType, Step>::PathResult(ProviderType& sourceProvider,
                                           ProviderType& targetProvider)
    : _numVerticesFromSourceProvider(0),
      _numEdgesFromSourceProvider(0),
      _sourceProvider(sourceProvider),
      _targetProvider(targetProvider) {}

template<class ProviderType, class Step>
auto PathResult<ProviderType, Step>::clear() -> void {
  _numVerticesFromSourceProvider = 0;
  _numEdgesFromSourceProvider = 0;
  _vertices.clear();
  _edges.clear();
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
auto PathResult<ProviderType, Step>::appendEdge(typename Step::Edge e) -> void {
  _edges.push_back(std::move(e));
}

template<class ProviderType, class Step>
auto PathResult<ProviderType, Step>::prependEdge(typename Step::Edge e)
    -> void {
  _numEdgesFromSourceProvider++;
  _edges.insert(_edges.begin(), std::move(e));
}

// NOTE:
// Potential optimization: Instead of counting on each append
// We can do a size call to the vector when switching the Provider.

template<class ProviderType, class Step>
auto PathResult<ProviderType, Step>::toVelocyPack(
    arangodb::velocypack::Builder& builder, WeightType weightType) -> void {
  TRI_ASSERT(_numVerticesFromSourceProvider <= _vertices.size());
  double sumWeights = 0;
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
    auto getActualWeight = [&](size_t position) {
      LOG_DEVEL << "Get actual weight of : " << position;
      VPackBuilder tmpBuilder;
      LOG_DEVEL << " - 1 - ";

      LOG_DEVEL << " - 2 - ";
      // TODO 0.) I do not like the fact that we have to put it into a
      //  tmpBuilder to be able to read it.

      LOG_DEVEL << " - 3 - ";
      _targetProvider.addEdgeToBuilder(_edges[position], tmpBuilder);

      // TODO 1.): This is not true. It can be any value. For now ok.
      LOG_DEVEL << tmpBuilder.toJson();
      if (tmpBuilder.slice().hasKey(StaticStrings::GraphQueryWeight)) {
        // TRI_ASSERT() <-- TODO: Add assert / ADD PROPER mechanism to read
        //  default weight and weight attribute here
        // TODO !IMPORTANT!
        LOG_DEVEL << " - 4 - ";

        if (tmpBuilder.slice()
                .get(StaticStrings::GraphQueryWeight)
                .isNumber()) {
          double dummy = tmpBuilder.slice()
                             .get(StaticStrings::GraphQueryWeight)
                             .getNumber<double>();
          LOG_DEVEL << " - 5 - ";
          return dummy;
        }
      }

      // TODO 2.): What if value is not inside the edge, we need to use default
      // weight.
      LOG_DEVEL << " - 6 - ";
      return 1.0;
    };

    builder.add(VPackValue(StaticStrings::GraphQueryEdges));
    VPackArrayBuilder edges(&builder);
    // Write first part of the Path
    for (size_t i = 0; i < _numEdgesFromSourceProvider; i++) {
      _sourceProvider.addEdgeToBuilder(_edges[i], builder);
      if (weightType == WeightType::ACTUAL_WEIGHT) {
        sumWeights += getActualWeight(i);
      }
    }
    // Write second part of the Path
    for (size_t i = _numEdgesFromSourceProvider; i < _edges.size(); i++) {
      _targetProvider.addEdgeToBuilder(_edges[i], builder);
      if (weightType == WeightType::ACTUAL_WEIGHT) {
        sumWeights += getActualWeight(i);
      }
    }
  }

  if (weightType != WeightType::NONE) {
    // TODO: Handle both cases properly. Think about better interface here.

    // We need to handled two different cases here. In case no weight callback
    // has been set, we need to write the amount of edges here. In case a weight
    // callback is set, we need to set the calculated weight.

    if (weightType == WeightType::AMOUNT_EDGES) {
      // Case 1) Amount of edges (as will be seen as weight=1 per edge)
      builder.add(StaticStrings::GraphQueryWeight, VPackValue(_edges.size()));
    } else if (weightType == WeightType::ACTUAL_WEIGHT) {
      // Case 2) Calculated weight (currently passed as parameter)
      builder.add(StaticStrings::GraphQueryWeight, VPackValue(sumWeights));
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
