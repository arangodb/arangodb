////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "GraphManager.h"

#include <velocypack/Buffer.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <array>
#include <boost/variant.hpp>
#include <utility>

#include "Aql/AstNode.h"
#include "Aql/Graphs.h"
#include "Aql/Query.h"
#include "Basics/ReadLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Graph/Graph.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/Graphs.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Aql/SmartGraph.h"
#endif

using namespace arangodb;
using namespace arangodb::graph;
using UserTransaction = transaction::Methods;

OperationResult GraphManager::createEdgeCollection(std::string const& name,
                                                   bool waitForSync,
                                                   VPackSlice options) {
  return createCollection(name, TRI_COL_TYPE_EDGE, waitForSync, options);
}

OperationResult GraphManager::createVertexCollection(std::string const& name,
                                                     bool waitForSync,
                                                     VPackSlice options) {
  return createCollection(name, TRI_COL_TYPE_DOCUMENT, waitForSync, options);
}

OperationResult GraphManager::createCollection(std::string const& name,
                                               TRI_col_type_e colType,
                                               bool waitForSync,
                                               VPackSlice options) {
  TRI_ASSERT(colType == TRI_COL_TYPE_DOCUMENT || colType == TRI_COL_TYPE_EDGE);

  Result res = methods::Collections::create(&ctx()->vocbase(), name, colType,
                                            options, waitForSync, true,
                                            [&](LogicalCollection* coll) {});

  return OperationResult(res);
}

OperationResult GraphManager::findOrCreateVertexCollectionByName(
    const std::string& name, bool waitForSync, VPackSlice options) {
  std::shared_ptr<LogicalCollection> def;

  def = getCollectionByName(ctx()->vocbase(), name);
  if (def == nullptr) {
    return createVertexCollection(name, waitForSync, options);
  }

  return OperationResult(TRI_ERROR_NO_ERROR);
}

Result GraphManager::assertFeasibleEdgeDefinitions(
    VPackSlice edgeDefinitionsSlice) const {
  TRI_ASSERT(edgeDefinitionsSlice.isArray());
  if (!edgeDefinitionsSlice.isArray()) {
    return {TRI_ERROR_INTERNAL};
  }

  std::unordered_map<std::string, ::arangodb::graph::EdgeDefinition>
      tmpEdgeDefinitions;

  for (auto const& slice : VPackArrayIterator(edgeDefinitionsSlice)) {
    auto res = EdgeDefinition::createFromVelocypack(slice);
    TRI_ASSERT(res.ok());
    if (res.fail()) {
      return res.copy_result();
    }
    EdgeDefinition& def = res.get();
    bool inserted;
    std::tie(std::ignore, inserted) =
        tmpEdgeDefinitions.emplace(def.getName(), std::move(def));

    if (!inserted) {
      return {TRI_ERROR_GRAPH_COLLECTION_MULTI_USE};
    }
  }

  VPackBuilder graphsBuilder;
  // TODO Maybe use the cache here
  readGraphs(graphsBuilder, arangodb::aql::PART_DEPENDENT);
  VPackSlice graphs = graphsBuilder.slice();

  TRI_ASSERT(graphs.get("graphs").isArray());
  if (!graphs.get("graphs").isArray()) {
    return {TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT};
  }

  for (auto graphSlice : VPackArrayIterator(graphs.get("graphs"))) {
    graphSlice = graphSlice.resolveExternals();
    TRI_ASSERT(graphSlice.isObject() && graphSlice.hasKey("_key"));
    if (!graphSlice.isObject() || !graphSlice.hasKey("_key")) {
      return {TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT};
    }
    Graph graph{graphSlice.get("_key").copyString(), graphSlice};

    for (auto const& sGED : graph.edgeDefinitions()) {
      std::string col = sGED.first;

      auto it = tmpEdgeDefinitions.find(col);
      if (it != tmpEdgeDefinitions.end()) {
        if (sGED.second != it->second) {
          // found an incompatible edge definition for the same collection
          return {TRI_ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS};
        }
      }
    }
  }

  return {TRI_ERROR_NO_ERROR};
}

// TODO replace the slice argument with an actual EdgeDefinition
OperationResult GraphManager::findOrCreateCollectionsByEdgeDefinitions(
    VPackSlice edgeDefinitions, bool waitForSync, VPackSlice options) {
  std::shared_ptr<LogicalCollection> def;

  for (auto const& edgeDefinition : VPackArrayIterator(edgeDefinitions)) {
    std::string edgeCollection = edgeDefinition.get("collection").copyString();
    VPackSlice from = edgeDefinition.get(StaticStrings::GraphFrom);
    VPackSlice to = edgeDefinition.get(StaticStrings::GraphTo);

    def = getCollectionByName(ctx()->vocbase(), edgeCollection);

    if (def == nullptr) {
      OperationResult res =
          createEdgeCollection(edgeCollection, waitForSync, options);
      if (res.fail()) {
        return res;
      }
    }

    std::unordered_set<std::string> vertexCollections;

    // duplicates in from and to shouldn't occur, but are safely ignored here
    for (auto const& colName : VPackArrayIterator(from)) {
      vertexCollections.emplace(colName.copyString());
    }
    for (auto const& colName : VPackArrayIterator(to)) {
      vertexCollections.emplace(colName.copyString());
    }
    for (auto const& colName : vertexCollections) {
      def = getCollectionByName(ctx()->vocbase(), colName);
      if (def == nullptr) {
        OperationResult res =
            createVertexCollection(colName, waitForSync, options);
        if (res.fail()) {
          return res;
        }
      }
    }
  }

  return OperationResult(TRI_ERROR_NO_ERROR);
}

/// @brief extract the collection by either id or name, may return nullptr!
std::shared_ptr<LogicalCollection> GraphManager::getCollectionByName(
    const TRI_vocbase_t& vocbase, std::string const& name) const {
  std::shared_ptr<LogicalCollection> nameCol;

  if (!name.empty()) {
    // try looking up the collection by name then
    try {
      if (arangodb::ServerState::instance()->isRunningInCluster()) {
        ClusterInfo* ci = ClusterInfo::instance();
        nameCol = ci->getCollection(vocbase.name(), name);
      } else {
        nameCol = vocbase.lookupCollection(name);
      }
    } catch (...) {
    }
  }

  // may be nullptr
  return nameCol;
}

OperationResult GraphManager::createGraph(VPackSlice document,
                                          bool waitForSync) {
  VPackSlice graphName = document.get("name");
  if (graphName.isNone()) {
    return OperationResult(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }

  // validate edgeDefinitions
  VPackSlice edgeDefinitions = document.get(StaticStrings::GraphEdgeDefinitions);
  if (edgeDefinitions.isNull() || edgeDefinitions.isNone()) {
    edgeDefinitions = VPackSlice::emptyArraySlice();
  }

  if (!edgeDefinitions.isArray()) {
    return OperationResult(TRI_ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION);
  }

  for (auto const& def : VPackArrayIterator(edgeDefinitions)) {
    Result res = EdgeDefinition::validateEdgeDefinition(def);
    if (res.fail()) {
      return OperationResult(res);
    }
  }

  // validate orphans
  VPackSlice orphanCollections = document.get(StaticStrings::GraphOrphans);
  if (orphanCollections.isNull() || orphanCollections.isNone()) {
    orphanCollections = VPackSlice::emptyArraySlice();
  }

  if (!orphanCollections.isArray()) {
    return OperationResult(TRI_ERROR_GRAPH_CREATE_MALFORMED_ORPHAN_LIST);
  }

  for (auto const& def : VPackArrayIterator(orphanCollections)) {
    Result res = Graph::validateOrphanCollection(def);
    if (res.fail()) {
      return OperationResult(res);
    }
  }

  uint64_t replicationFactor;
  try {
    replicationFactor = document
                            .get(std::vector<std::string>(
                                {"options", StaticStrings::GraphReplicationFactor}))
                            .getUInt();
  } catch (...) {
    replicationFactor = 1;
  }

  uint64_t numberOfShards;
  try {
    numberOfShards = document
                         .get(std::vector<std::string>(
                             {"options", StaticStrings::GraphNumberOfShards}))
                         .getUInt();
  } catch (...) {
    numberOfShards = 1;
  }

  bool isSmart = false;

#ifdef USE_ENTERPRISE
  try {
    isSmart = document.get(StaticStrings::GraphIsSmart).getBool();
  } catch (...) {
  }

  std::string smartGraphAttribute;
  try {
    smartGraphAttribute = document
                              .get(std::vector<std::string>(
                                  {"options", StaticStrings::GraphSmartGraphAttribute}))
                              .copyString();
  } catch (...) {
  }
#endif

  // check for multiple collection graph usage

  // check if graph already exists
  VPackBuilder checkDocument;
  {
    VPackObjectBuilder guard(&checkDocument);
    checkDocument.add(StaticStrings::KeyString, graphName);
  }

  SingleCollectionTransaction trx(ctx(), StaticStrings::GraphCollection,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    return OperationResult(res);
  }

  OperationOptions options;
  options.waitForSync = waitForSync;

  // check, if a collection is already used in a different edgeDefinition
  res = assertFeasibleEdgeDefinitions(edgeDefinitions);
  if (!res.ok()) {
    return OperationResult(res);
  }

  OperationResult checkDoc =
      trx.document(StaticStrings::GraphCollection, checkDocument.slice(), options);
  if (checkDoc.ok()) {
    trx.finish(checkDoc.result);
    return OperationResult(TRI_ERROR_GRAPH_DUPLICATE);
  }

  VPackBuilder collectionsOptions;
#ifdef USE_ENTERPRISE
  VPackBuilder params;
  {
    VPackObjectBuilder guard(&params);
    if (isSmart) {
      params.add(StaticStrings::GraphIsSmart, VPackValue(isSmart));
      params.add(StaticStrings::GraphSmartGraphAttribute, VPackValue(smartGraphAttribute));
    }
    params.add(StaticStrings::GraphNumberOfShards, VPackValue(numberOfShards));
    params.add(StaticStrings::GraphReplicationFactor, VPackValue(replicationFactor));
  }
  if (isSmart) {
    arangodb::enterprise::SmartGraph::createCollectionOptions(collectionsOptions, waitForSync, params.slice());
  } else {
    Graph::createCollectionOptions(collectionsOptions, waitForSync, params.slice());
  }
#else
  VPackBuilder params;
  {
    VPackObjectBuilder guard(&params);
    params.add(StaticStrings::GraphNumberOfShards, VPackValue(numberOfShards));
    params.add(StaticStrings::GraphReplicationFactor, VPackValue(replicationFactor));
  }
  Graph::createCollectionOptions(collectionsOptions, waitForSync, params.slice());
#endif

  VPackSlice collectionCreateOptionsSlice = collectionsOptions.slice();

  // find or create the collections given by the edge definition
  OperationResult result = findOrCreateCollectionsByEdgeDefinitions(
      edgeDefinitions, waitForSync, collectionCreateOptionsSlice);
  if (result.fail()) {
    return result;
  }

  // find or create the oprhan collections
  for (auto const& orphan : VPackArrayIterator(orphanCollections)) {
    result = findOrCreateVertexCollectionByName(
        orphan.copyString(), waitForSync, collectionCreateOptionsSlice);
    if (result.fail()) {
      return result;
    }
  }

  // finally save the graph
  VPackBuilder builder;
  builder.add(VPackValue(VPackValueType::Object));
  builder.add(StaticStrings::KeyString, graphName);
  builder.add(StaticStrings::GraphEdgeDefinitions, edgeDefinitions);
  builder.add(StaticStrings::GraphOrphans, orphanCollections);
  builder.add(StaticStrings::GraphReplicationFactor, VPackValue(replicationFactor));
  builder.add(StaticStrings::GraphNumberOfShards, VPackValue(numberOfShards));

#ifdef USE_ENTERPRISE
  builder.add(StaticStrings::GraphIsSmart, VPackValue(isSmart));
  builder.add(StaticStrings::GraphSmartGraphAttribute, VPackValue(smartGraphAttribute));
#endif

  builder.close();

  result = trx.insert(StaticStrings::GraphCollection, builder.slice(), options);
  res = trx.finish(result.result);

  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }
  return result;
}

OperationResult GraphManager::readGraphs(velocypack::Builder& builder,
                                         aql::QueryPart const queryPart) const {
  std::string const queryStr{"FOR g IN _graphs RETURN g"};
  arangodb::aql::Query query(false, ctx()->vocbase(),
                             arangodb::aql::QueryString(queryStr), nullptr,
                             nullptr, queryPart);

  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
      << "starting to load graphs information";
  aql::QueryResult queryResult =
      query.execute(QueryRegistryFeature::QUERY_REGISTRY);

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
        (queryResult.code == TRI_ERROR_QUERY_KILLED)) {
      return OperationResult(TRI_ERROR_REQUEST_CANCELED);
    }
    return OperationResult(queryResult.code);
  }

  VPackSlice graphsSlice = queryResult.result->slice();

  if (graphsSlice.isNone()) {
    return OperationResult(TRI_ERROR_OUT_OF_MEMORY);
  } else if (!graphsSlice.isArray()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot read users from _graphs collection";
  }

  builder.add(VPackValue(VPackValueType::Object));
  builder.add("graphs", graphsSlice);
  builder.close();

  return OperationResult(TRI_ERROR_NO_ERROR);
}

// TODO unify with assertFeasibleEdgeDefinitions
OperationResult GraphManager::checkForEdgeDefinitionConflicts(
    std::string const& edgeDefinitionName, VPackSlice edgeDefinition) {
  VPackBuilder graphsBuilder;
  OperationResult result =
      readGraphs(graphsBuilder, arangodb::aql::PART_DEPENDENT);
  if (result.fail()) {
    return result;
  }
  VPackSlice graphs = graphsBuilder.slice();

  TRI_ASSERT(graphs.get("graphs").isArray());

  if (!graphs.get("graphs").isArray()) {
    return OperationResult(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT);
  }

  for (auto singleGraph : VPackArrayIterator(graphs.get("graphs"))) {
    singleGraph = singleGraph.resolveExternals();
    VPackSlice sGEDs = singleGraph.get("edgeDefinitions");
    if (!sGEDs.isArray()) {
      return OperationResult(TRI_ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION);
    }
    for (auto const& sGED : VPackArrayIterator(sGEDs)) {
      std::string col;
      if (sGED.get("collection").isString()) {
        col = sGED.get("collection").copyString();
      } else {
        return OperationResult(
            TRI_ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION);
      }
      if (col == edgeDefinitionName) {
        if (sGED.toString() != edgeDefinition.toString()) {
          return OperationResult(
              TRI_ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS);
        }
      }
    }
  }

  return OperationResult(TRI_ERROR_NO_ERROR);
}
