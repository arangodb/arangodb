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
#include "Graph/Graph.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/Graphs.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

using namespace arangodb;
using namespace arangodb::graph;
using UserTransaction = transaction::Methods;

Result GraphManager::createEdgeCollection(std::string const& name,
                                          bool waitForSync) {
  return createCollection(name, TRI_COL_TYPE_EDGE, waitForSync);
}

Result GraphManager::createVertexCollection(std::string const& name,
                                            bool waitForSync) {
  return createCollection(name, TRI_COL_TYPE_DOCUMENT, waitForSync);
}

Result GraphManager::createCollection(std::string const& name,
                                      TRI_col_type_e colType,
                                      bool waitForSync) {
  TRI_ASSERT(colType == TRI_COL_TYPE_DOCUMENT || colType == TRI_COL_TYPE_EDGE);

  Result res = methods::Collections::create(
      &ctx()->vocbase(), name, colType, VPackSlice::emptyObjectSlice(),
      waitForSync, true, [&](LogicalCollection* coll) {});

  return res;
}

void GraphManager::findOrCreateVertexCollectionByName(const std::string& name,
                                                      bool waitForSync) {
  std::shared_ptr<LogicalCollection> def;

  def = getCollectionByName(ctx()->vocbase(), name);
  if (def == nullptr) {
    createVertexCollection(name, waitForSync);
  }
}

void GraphManager::assertFeasibleEdgeDefinitions(
    VPackSlice edgeDefinitionsSlice) const {
  TRI_ASSERT(edgeDefinitionsSlice.isArray());
  if (!edgeDefinitionsSlice.isArray()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  std::unordered_map<std::string, ::arangodb::graph::EdgeDefinition>
      tmpEdgeDefinitions;

  for (auto const& slice : VPackArrayIterator(edgeDefinitionsSlice)) {
    auto res = EdgeDefinition::createFromVelocypack(slice);
    TRI_ASSERT(res.ok());
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
    EdgeDefinition& def = res.get();
    bool inserted;
    std::tie(std::ignore, inserted) =
        tmpEdgeDefinitions.emplace(def.getName(), std::move(def));

    if (!inserted) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_COLLECTION_MULTI_USE);
    }
  }

  VPackBuilder graphsBuilder;
  // TODO Maybe use the cache here
  readGraphs(graphsBuilder, arangodb::aql::PART_DEPENDENT);
  VPackSlice graphs = graphsBuilder.slice();

  TRI_ASSERT(graphs.get("graphs").isArray());
  if (!graphs.get("graphs").isArray()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT);
  }

  for (auto graphSlice : VPackArrayIterator(graphs.get("graphs"))) {
    graphSlice = graphSlice.resolveExternals();
    TRI_ASSERT(graphSlice.isObject() && graphSlice.hasKey("_key"));
    if (!graphSlice.isObject() || !graphSlice.hasKey("_key")) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT);
    }
    Graph graph{graphSlice.get("_key").copyString(), graphSlice};

    for (auto const& sGED : graph.edgeDefinitions()) {
      std::string col = sGED.first;

      auto it = tmpEdgeDefinitions.find(col);
      if (it != tmpEdgeDefinitions.end()) {
        if (sGED.second != it->second) {
          // found an incompatible edge definition for the same collection
          THROW_ARANGO_EXCEPTION(
              TRI_ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS);
        }
      }
    }
  }
}

// TODO replace the slice argument with an actual EdgeDefinition
void GraphManager::findOrCreateCollectionsByEdgeDefinitions(
    VPackSlice edgeDefinitions, bool waitForSync) {
  std::shared_ptr<LogicalCollection> def;

  for (auto const& edgeDefinition : VPackArrayIterator(edgeDefinitions)) {
    std::string collection = edgeDefinition.get("collection").copyString();
    VPackSlice from = edgeDefinition.get("from");
    VPackSlice to = edgeDefinition.get("to");

    std::unordered_set<std::string> fromSet;
    std::unordered_set<std::string> toSet;

    def = getCollectionByName(ctx()->vocbase(), collection);

    if (def == nullptr) {
      createEdgeCollection(collection, waitForSync);
    }

    // duplicates in from and to shouldn't occur, but are safely ignored here
    for (auto const& edge : VPackArrayIterator(from)) {
      def = getCollectionByName(ctx()->vocbase(), edge.copyString());
      if (def == nullptr) {
        createVertexCollection(edge.copyString(), waitForSync);
      }
    }
    for (auto const& edge : VPackArrayIterator(to)) {
      def = getCollectionByName(ctx()->vocbase(), edge.copyString());
      if (def == nullptr) {
        createVertexCollection(edge.copyString(), waitForSync);
      }
    }
  }
}

/// @brief extract the collection by either id or name, may return nullptr!
std::shared_ptr<LogicalCollection> GraphManager::getCollectionByName(
    const TRI_vocbase_t& vocbase, std::string const& name) const {
  std::shared_ptr<LogicalCollection> nameCol;

  if (!name.empty()) {
    // try looking up the collection by name then
    try {
      nameCol = vocbase.lookupCollection(name);
    } catch (...) {
    }
  }

  // may be nullptr
  return nameCol;
}

ResultT<std::pair<OperationResult, Result>> GraphManager::createGraph(
    VPackSlice document, bool waitForSync) {
  VPackSlice graphName = document.get("name");
  if (graphName.isNone()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }

  // validate edgeDefinitions
  VPackSlice edgeDefinitions = document.get(Graph::_attrEdgeDefs);
  if (edgeDefinitions.isNull() || edgeDefinitions.isNone()) {
    // TODO there is no test for this
    edgeDefinitions = VPackSlice::emptyArraySlice();
  }

  if (!edgeDefinitions.isArray()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION);
  }

  for (auto const& def : VPackArrayIterator(edgeDefinitions)) {
    Result res = EdgeDefinition::validateEdgeDefinition(def);
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  // validate orphans
  VPackSlice orphanCollections = document.get(Graph::_attrOrphans);
  if (orphanCollections.isNull() || orphanCollections.isNone()) {
    orphanCollections = VPackSlice::emptyArraySlice();
  }

  if (!orphanCollections.isArray()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MALFORMED_ORPHAN_LIST);
  }

  for (auto const& def : VPackArrayIterator(orphanCollections)) {
    Result res = Graph::validateOrphanCollection(def);
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  uint64_t replicationFactor;
  try {
    replicationFactor = document.get(std::vector<std::string>({ "options", Graph::_attrReplicationFactor})).getUInt();
  } catch (...) {
    replicationFactor = 1;
  }

  uint64_t numberOfShards;
  try {
    numberOfShards = document.get(std::vector<std::string>({ "options", Graph::_attrNumberOfShards})).getUInt();
  } catch (...) {
    numberOfShards = 1;
  }

  #ifdef USE_ENTERPRISE
  bool isSmart = false;
  try {
    isSmart = document.get(Graph::_attrIsSmart).getBool();
  } catch (...) {
  }

  std::string smartGraphAttribute;
  try {
    smartGraphAttribute = document.get(std::vector<std::string>({ "options", Graph::_attrSmartGraphAttribute})).copyString();
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
  transaction::Options trxOptions;
  trxOptions.waitForSync = waitForSync;

  SingleCollectionTransaction trx(ctx(), Graph::_graphs,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    return res;
  }

  OperationOptions options;
  options.waitForSync = waitForSync;

  // check, if a collection is already used in a different edgeDefinition
  assertFeasibleEdgeDefinitions(edgeDefinitions);

  OperationResult checkDoc =
      trx.document(Graph::_graphs, checkDocument.slice(), options);
  if (checkDoc.ok()) {
    trx.finish(checkDoc.result);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_DUPLICATE,
                                   "graph already exists");
  }

  // find or create the collections given by the edge definition
  findOrCreateCollectionsByEdgeDefinitions(edgeDefinitions, waitForSync);

  // find or create the oprhan collections
  for (auto const& orphan : VPackArrayIterator(orphanCollections)) {
    findOrCreateVertexCollectionByName(orphan.copyString(), waitForSync);
  }

  // finally save the graph
  VPackBuilder builder;
  builder.add(VPackValue(VPackValueType::Object));
  builder.add(StaticStrings::KeyString, graphName);
  builder.add(Graph::_attrEdgeDefs, edgeDefinitions);
  builder.add(Graph::_attrOrphans, orphanCollections);
  builder.add(Graph::_attrReplicationFactor, VPackValue(replicationFactor));
  builder.add(Graph::_attrNumberOfShards, VPackValue(numberOfShards));

#ifdef USE_ENTERPRISE
  builder.add(Graph::_attrIsSmart, VPackValue(isSmart));
  builder.add(Graph::_attrSmartGraphAttribute, VPackValue(smartGraphAttribute));
#endif

  builder.close();

  OperationResult result = trx.insert(Graph::_graphs, builder.slice(), options);

  res = trx.finish(result.result);
  return std::make_pair(std::move(result), std::move(res));
}

void GraphManager::readGraphs(velocypack::Builder& builder,
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
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(
        queryResult.code,
        "Error executing graphs query: " + queryResult.details);
  }

  VPackSlice graphsSlice = queryResult.result->slice();

  if (graphsSlice.isNone()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  } else if (!graphsSlice.isArray()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot read users from _graphs collection";
  }

  builder.add(VPackValue(VPackValueType::Object));
  builder.add("graphs", graphsSlice);
  builder.close();
}

// TODO unify with assertFeasibleEdgeDefinitions
Result GraphManager::checkForEdgeDefinitionConflicts(
    std::string const& edgeDefinitionName, VPackSlice edgeDefinition) {
  VPackBuilder graphsBuilder;
  readGraphs(graphsBuilder, arangodb::aql::PART_DEPENDENT);
  VPackSlice graphs = graphsBuilder.slice();

  TRI_ASSERT(graphs.get("graphs").isArray());

  if (!graphs.get("graphs").isArray()) {
    return {TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT};
  }

  for (auto singleGraph : VPackArrayIterator(graphs.get("graphs"))) {
    singleGraph = singleGraph.resolveExternals();
    VPackSlice sGEDs = singleGraph.get("edgeDefinitions");
    if (!sGEDs.isArray()) {
      return {TRI_ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION};
    }
    for (auto const& sGED : VPackArrayIterator(sGEDs)) {
      std::string col;
      if (sGED.get("collection").isString()) {
        col = sGED.get("collection").copyString();
      } else {
        return {TRI_ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION};
      }
      if (col == edgeDefinitionName) {
        if (sGED.toString() != edgeDefinition.toString()) {
          return {TRI_ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS};
        }
      }
    }
  }

  return {TRI_ERROR_NO_ERROR};
}
