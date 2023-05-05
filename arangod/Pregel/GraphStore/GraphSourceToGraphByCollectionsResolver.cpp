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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "Pregel/GraphStore/GraphSourceToGraphByCollectionsResolver.h"

#include "Basics/Result.h"
#include "Basics/ErrorT.h"
#include "Graph/GraphManager.h"
#include "VocBase/vocbase.h"

namespace arangodb::pregel {

auto resolveGraphSourceToGraphByCollections(TRI_vocbase_t& vocbase,
                                            GraphSource graphSource,
                                            std::string shardKeyAttribute)
    -> errors::ErrorT<Result, GraphByCollections> {
  if (std::holds_alternative<GraphCollectionNames>(
          graphSource.graphOrCollections)) {
    auto collectionNames =
        std::get<GraphCollectionNames>(graphSource.graphOrCollections);
    return errors::ErrorT<Result, GraphByCollections>::ok(GraphByCollections{
        .vertexCollections = collectionNames.vertexCollections,
        .edgeCollections = collectionNames.edgeCollections,
        .edgeCollectionRestrictions =
            graphSource.edgeCollectionRestrictions.items,
        .shardKeyAttribute = shardKeyAttribute});
  } else if (std::holds_alternative<GraphName>(
                 graphSource.graphOrCollections)) {
    GraphByCollections graphByCollections;

    graphByCollections.shardKeyAttribute = shardKeyAttribute;

    auto graphName = std::get<GraphName>(graphSource.graphOrCollections);
    if (graphName.graph.empty()) {
      return errors::ErrorT<Result, GraphByCollections>::error(
          Result{TRI_ERROR_BAD_PARAMETER, "expecting graphName as string"});
    }

    graph::GraphManager gmngr{vocbase};
    auto maybeGraph = gmngr.lookupGraphByName(graphName.graph);
    if (maybeGraph.fail()) {
      return errors::ErrorT<Result, GraphByCollections>::error(
          std::move(maybeGraph).result());
    }
    auto graph = std::move(maybeGraph.get());

    auto const& gv = graph->vertexCollections();
    for (auto const& v : gv) {
      graphByCollections.vertexCollections.push_back(v);
    }

    auto const& ge = graph->edgeCollections();
    for (auto const& e : ge) {
      graphByCollections.edgeCollections.push_back(e);
    }

    auto const& ed = graph->edgeDefinitions();
    for (auto const& e : ed) {
      auto const& from = e.second.getFrom();
      for (auto const& f : from) {
        // intentionally create map entry if it does not exist;
        auto& restrictions = graphByCollections.edgeCollectionRestrictions[f];
        restrictions.push_back(e.second.getName());
      }
    }
    return errors::ErrorT<Result, GraphByCollections>::ok(graphByCollections);
  }

  return errors::ErrorT<Result, GraphByCollections>::error(
      Result{TRI_ERROR_FORBIDDEN});
}

}  // namespace arangodb::pregel
