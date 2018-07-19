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
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/Graphs.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Aql/SmartGraph.h"
#include "Enterprise/Graph/GraphManagerEE.h"
#endif

using namespace arangodb;
using namespace arangodb::graph;
using UserTransaction = transaction::Methods;
using VelocyPackHelper = basics::VelocyPackHelper;

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
        tmpEdgeDefinitions.emplace(def.getName(), def);

    if (!inserted) {
      return Result(
          TRI_ERROR_GRAPH_COLLECTION_MULTI_USE,
          def.getName() + " " + std::string{TRI_errno_string(
                                    TRI_ERROR_GRAPH_COLLECTION_MULTI_USE)});
    }
  }

  return checkForEdgeDefinitionConflicts(tmpEdgeDefinitions);
}

bool GraphManager::renameGraphCollection(std::string oldName, std::string newName) {
  // todo: return a result, by now just used in the graph modules
  VPackBuilder graphsBuilder;
  readGraphs(graphsBuilder, aql::PART_DEPENDENT);
  VPackSlice graphs = graphsBuilder.slice();

  SingleCollectionTransaction trx(ctx(), StaticStrings::GraphCollection,
                                  AccessMode::Type::WRITE);

  Result res = trx.begin();

  if (!res.ok()) {
    return false;
  }
  OperationOptions options;
  OperationResult checkDoc;


  for (auto graphSlice : VPackArrayIterator(graphs.get("graphs"))) {
    VPackBuilder builder;

    graphSlice = graphSlice.resolveExternals();
    TRI_ASSERT(graphSlice.isObject() && graphSlice.hasKey(StaticStrings::KeyString));
    if (!graphSlice.isObject() || !graphSlice.hasKey(StaticStrings::KeyString)) {
      // return {TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT};
      return false;
    }
    std::unique_ptr<Graph> graph;
    try {
      graph = std::make_unique<Graph>(graphSlice.get(StaticStrings::KeyString).copyString(),
                                      graphSlice);
    } catch (basics::Exception& e) {
      // return {e.message(), e.code()};
      return false;
    }

    // rename not allowed in a smart collection
    if (graph->isSmart()) {
      continue;
    }

    builder.openObject();
    builder.add(StaticStrings::KeyString, VPackValue(graphSlice.get(StaticStrings::KeyString).copyString()));

    builder.add(StaticStrings::GraphEdgeDefinitions, VPackValue(VPackValueType::Array));
    for (auto const& sGED : graph->edgeDefinitions()) {
      builder.openObject();
      std::string col = sGED.first;
      std::set<std::string> froms = sGED.second.getFrom();
      std::set<std::string> tos = sGED.second.getTo();

      if (col != oldName) {
        builder.add("collection", VPackValue(col));
      } else {
        builder.add("collection", VPackValue(newName));
      }

      builder.add("from", VPackValue(VPackValueType::Array));
      for (auto const& from : froms) {
        if (from != oldName) {
          builder.add(VPackValue(from));
        } else {
          builder.add(VPackValue(newName));
        }
      }
      builder.close(); // array

      builder.add("to", VPackValue(VPackValueType::Array));
      for (auto const& to : tos) {
        if (to != oldName) {
          builder.add(VPackValue(to));
        } else {
          builder.add(VPackValue(newName));
        }
      }
      builder.close(); // array

      builder.close(); // object
    }
    builder.close(); // array
    builder.close(); // object

    try {
      checkDoc =
          trx.update(StaticStrings::GraphCollection, builder.slice(), options);
      if (checkDoc.fail()) {
        trx.finish(checkDoc.result);
        return false;
      }
    } catch (...) {
    }
  }

  trx.finish(checkDoc.result);
  return true;
}

Result GraphManager::checkForEdgeDefinitionConflicts(
    std::unordered_map<std::string, EdgeDefinition> const& edgeDefinitions)
    const {
  VPackBuilder graphsBuilder;
  // TODO Maybe use the cache here
  readGraphs(graphsBuilder, aql::PART_DEPENDENT);
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
    std::unique_ptr<Graph> graph;
    try {
      graph = std::make_unique<Graph>(graphSlice.get("_key").copyString(),
                                      graphSlice);
    } catch (basics::Exception& e) {
      return {e.code(), e.message()};
    }

    for (auto const& sGED : graph->edgeDefinitions()) {
      std::string col = sGED.first;

      auto it = edgeDefinitions.find(col);
      if (it != edgeDefinitions.end()) {
        if (sGED.second != it->second) {
          // found an incompatible edge definition for the same collection
          return Result(TRI_ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS,
                 sGED.first + " " +
                     std::string{TRI_errno_string(
                         TRI_ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS)});
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

OperationResult GraphManager::findOrCreateCollectionsByEdgeDefinitions(
  std::map<std::string, EdgeDefinition> const& edgeDefinitions, bool waitForSync,
  VPackSlice options) {
  std::shared_ptr<LogicalCollection> def;

  for (auto const& it : edgeDefinitions) {
    std::string const& edgeCollection = it.first;
    EdgeDefinition const& edgeDefinition = it.second;

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
    for (auto const& colName : edgeDefinition.getFrom()) {
      vertexCollections.emplace(colName);
    }
    for (auto const& colName : edgeDefinition.getTo()) {
      vertexCollections.emplace(colName);
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
    const TRI_vocbase_t& vocbase, std::string const& name) {
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

bool GraphManager::graphExists(std::string graphName) const {
  VPackBuilder checkDocument;
  {
    VPackObjectBuilder guard(&checkDocument);
    checkDocument.add(StaticStrings::KeyString, VPackValue(graphName));
  }

  SingleCollectionTransaction trx(ctx(), StaticStrings::GraphCollection,
                                  AccessMode::Type::READ);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    return false;
  }

  OperationOptions options;
  try {
    OperationResult checkDoc = trx.document(StaticStrings::GraphCollection,
                                            checkDocument.slice(), options);
    if (checkDoc.fail()) {
      trx.finish(checkDoc.result);
      return false;
    }
    trx.finish(checkDoc.result);
  } catch (...) {
  }
  return true;
}

OperationResult GraphManager::createGraph(VPackSlice document,
                                          bool waitForSync) {
  VPackSlice graphNameSlice = document.get("name");
  if (graphNameSlice.isNone()) {
    return OperationResult(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  std::string const graphName {graphNameSlice.copyString()};
  OperationResult result;

  // validate edgeDefinitions
  VPackSlice edgeDefinitionsSlice =
      document.get(StaticStrings::GraphEdgeDefinitions);
  if (edgeDefinitionsSlice.isNull() || edgeDefinitionsSlice.isNone()) {
    edgeDefinitionsSlice = VPackSlice::emptyArraySlice();
  }

  if (!edgeDefinitionsSlice.isArray()) {
    return OperationResult{TRI_ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION};
  }

  std::map<std::string, EdgeDefinition> edgeDefinitions;

  for (auto const& def : VPackArrayIterator(edgeDefinitionsSlice)) {
    auto maybeEdgeDefinition =
        EdgeDefinition::createFromVelocypack(def);
    if (!maybeEdgeDefinition) {
      return OperationResult{maybeEdgeDefinition.copy_result()};
    }
    EdgeDefinition& edgeDefinition = maybeEdgeDefinition.get();
    // copy on purpose to avoid problems with move of edge definition
    std::string name = edgeDefinition.getName();
    edgeDefinitions.emplace(std::move(name), std::move(edgeDefinition));
  }

  // validate orphans
  VPackSlice orphanCollectionsSlice = document.get(StaticStrings::GraphOrphans);
  if (orphanCollectionsSlice.isNull() || orphanCollectionsSlice.isNone()) {
    orphanCollectionsSlice = VPackSlice::emptyArraySlice();
  }

  if (!orphanCollectionsSlice.isArray()) {
    return OperationResult(TRI_ERROR_GRAPH_CREATE_MALFORMED_ORPHAN_LIST);
  }

  std::set<std::string> orphanCollections;

  for (auto const& def : VPackArrayIterator(orphanCollectionsSlice)) {
    Result res = Graph::validateOrphanCollection(def);
    if (res.fail()) {
      return OperationResult(res);
    }
    orphanCollections.emplace(def.copyString());
  }

  uint64_t replicationFactor;
  try {
    replicationFactor =
        document
            .get(std::vector<std::string>(
                {"options", StaticStrings::ReplicationFactor}))
            .getUInt();
  } catch (...) {
    replicationFactor = 1;
  }

  uint64_t numberOfShards;
  try {
    numberOfShards = document
                         .get(std::vector<std::string>(
                             {"options", StaticStrings::NumberOfShards}))
                         .getUInt();
  } catch (...) {
    numberOfShards = 1;
  }

#ifdef USE_ENTERPRISE
  bool isSmart = false;

  try {
    isSmart = document.get(StaticStrings::GraphIsSmart).getBool();
  } catch (...) {
  }

  std::string smartGraphAttribute;
  try {
    smartGraphAttribute =
        document
            .get(std::vector<std::string>(
                {"options", StaticStrings::GraphSmartGraphAttribute}))
            .copyString();
  } catch (...) {
  }

  arangodb::enterprise::graph::SmartGraphManager smartgmngr{ctx()};
  if (isSmart) {
    result =
        smartgmngr.checkPrerequisites(edgeDefinitionsSlice, orphanCollections);
    if (result.fail()) {
      return result;
    }
  }
#endif

  Result permRes =
    checkCreateGraphPermissions(graphName, edgeDefinitions, orphanCollections);
  if (permRes.fail()) {
    return OperationResult{std::move(permRes)};
  }

  SingleCollectionTransaction trx(ctx(), StaticStrings::GraphCollection,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();
  if (res.fail()) {
    return OperationResult{res};
  }

  OperationOptions options;
  options.waitForSync = waitForSync;

  // check, if a collection is already used in a different edgeDefinition
  res = assertFeasibleEdgeDefinitions(edgeDefinitionsSlice);
  if (res.fail()) {
    return OperationResult{res};
  }

  // check for multiple collection graph usage

  // check if graph already exists
  VPackBuilder checkDocument;
  {
    VPackObjectBuilder guard(&checkDocument);
    checkDocument.add(StaticStrings::KeyString, graphNameSlice);
  }

  OperationResult checkDoc = trx.document(StaticStrings::GraphCollection,
                                          checkDocument.slice(), options);
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
      params.add(StaticStrings::GraphSmartGraphAttribute,
                 VPackValue(smartGraphAttribute));
    }
    params.add(StaticStrings::NumberOfShards, VPackValue(numberOfShards));
    params.add(StaticStrings::ReplicationFactor,
               VPackValue(replicationFactor));
  }
  if (isSmart) {
    arangodb::enterprise::SmartGraph::createCollectionOptions(
        collectionsOptions, waitForSync, params.slice(), "");
  } else {
    Graph::createCollectionOptions(collectionsOptions, waitForSync,
                                   params.slice());
  }
#else
  VPackBuilder params;
  {
    VPackObjectBuilder guard(&params);
    params.add(StaticStrings::NumberOfShards, VPackValue(numberOfShards));
    params.add(StaticStrings::ReplicationFactor,
               VPackValue(replicationFactor));
  }
  Graph::createCollectionOptions(collectionsOptions, waitForSync,
                                 params.slice());
#endif

  VPackSlice collectionCreateOptionsSlice = collectionsOptions.slice();

#ifdef USE_ENTERPRISE
  if (isSmart) {
    result = smartgmngr.satisfyPrerequisites(
        edgeDefinitionsSlice, orphanCollections, collectionCreateOptionsSlice,
        waitForSync);
  } else {
    // find or create the collections given by the edge definition
    result = findOrCreateCollectionsByEdgeDefinitions(
        edgeDefinitions, waitForSync, collectionCreateOptionsSlice);

    // find or create the orphan collections
    for (auto const& orphan : orphanCollections) {
      result = findOrCreateVertexCollectionByName(orphan, waitForSync,
                                                  collectionCreateOptionsSlice);
      if (result.fail()) {
        return result;
      }
    }
  }
#else
  // find or create the collections given by the edge definition
  result = findOrCreateCollectionsByEdgeDefinitions(
      edgeDefinitions, waitForSync, collectionCreateOptionsSlice);

  // find or create the orphan collections
  for (auto const& orphan : orphanCollections) {
    result = findOrCreateVertexCollectionByName(orphan, waitForSync,
                                                collectionCreateOptionsSlice);
    if (result.fail()) {
      return result;
    }
  }
#endif
  if (result.fail()) {
    return result;
  }

  // finally save the graph
  VPackBuilder builder;
  builder.add(VPackValue(VPackValueType::Object));
  builder.add(StaticStrings::KeyString, graphNameSlice);
  builder.add(StaticStrings::GraphEdgeDefinitions,
              VPackValue(VPackValueType::Array));
  for (auto const& it : edgeDefinitions) {
    it.second.addToBuilder(builder);
  }
  builder.close(); // edge definitions
  builder.add(StaticStrings::GraphOrphans, orphanCollectionsSlice);
  builder.add(StaticStrings::ReplicationFactor, VPackValue(replicationFactor));
  builder.add(StaticStrings::NumberOfShards, VPackValue(numberOfShards));

#ifdef USE_ENTERPRISE
  builder.add(StaticStrings::GraphIsSmart, VPackValue(isSmart));
  builder.add(StaticStrings::GraphSmartGraphAttribute,
              VPackValue(smartGraphAttribute));
#endif

  builder.close();

#ifdef USE_ENTERPRISE
  if (isSmart) {
    VPackBuilder eOptionsBuilder = smartgmngr.getInitialOptions();
    VPackBuilder merged =
        VelocyPackHelper::merge(eOptionsBuilder.slice(), builder.slice(), false, false);

    result = trx.insert(StaticStrings::GraphCollection, merged.slice(), options);
  } else {
    result = trx.insert(StaticStrings::GraphCollection, builder.slice(), options);
  }
#else
  result = trx.insert(StaticStrings::GraphCollection, builder.slice(), options);
#endif
  res = trx.finish(result.result);

  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }
  return result;
}

OperationResult GraphManager::readGraphs(velocypack::Builder& builder,
                                         aql::QueryPart const queryPart) const {
  std::string const queryStr{"FOR g IN _graphs RETURN g"};
  return readGraphByQuery(builder, queryPart, queryStr);
}

OperationResult GraphManager::readGraphKeys(velocypack::Builder& builder,
                                         aql::QueryPart const queryPart) const {
  std::string const queryStr{"FOR g IN _graphs RETURN g._key"};
  return readGraphByQuery(builder, queryPart, queryStr);
}

OperationResult GraphManager::readGraphByQuery(velocypack::Builder& builder,
                                         aql::QueryPart const queryPart, std::string queryStr) const {
  arangodb::aql::Query query(false, ctx()->vocbase(),
                             arangodb::aql::QueryString(queryStr), nullptr,
                             nullptr, queryPart);

  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
      << "starting to load graphs information";
  aql::QueryResult queryResult =
      query.executeSync(QueryRegistryFeature::QUERY_REGISTRY);

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

Result GraphManager::checkForEdgeDefinitionConflicts(
    arangodb::graph::EdgeDefinition const& edgeDefinition) const {
  std::unordered_map<std::string, EdgeDefinition> edgeDefs{
      std::make_pair(edgeDefinition.getName(), edgeDefinition)};

  return checkForEdgeDefinitionConflicts(edgeDefs);
}

Result GraphManager::checkCreateGraphPermissions(
    std::string const& graphName,
    std::map<std::string, EdgeDefinition> const& edgeDefinitions,
    std::set<std::string> const& orphanCollections) const {
  std::string const& databaseName = ctx()->vocbase().name();

  std::stringstream stringstream;
  stringstream << "When creating graph " << databaseName << "." << graphName << ": ";
  std::string const logprefix = stringstream.str();

  ExecContext const* execContext = ExecContext::CURRENT;
  if (execContext == nullptr) {
    LOG_TOPIC(DEBUG, Logger::GRAPHS) << logprefix
                                     << "Permissions are turned off.";
    return TRI_ERROR_NO_ERROR;
  }

  // collect all used collections in one container
  std::set<std::string> graphCollections;
  for (auto const& it : edgeDefinitions) {
    EdgeDefinition const &edgeDef = it.second;
    setUnion(graphCollections, edgeDef.getFrom());
    setUnion(graphCollections, edgeDef.getTo());
  }
  setUnion(graphCollections, orphanCollections);

  bool canUseDatabaseRW = execContext->canUseDatabase(auth::Level::RW);
  for (auto const& col : graphCollections) {
    // We need RO on all collections. And, in case any collection does not
    // exist, we need RW on the database.
    if (!execContext->canUseCollection(col, auth::Level::RO)) {
      LOG_TOPIC(DEBUG, Logger::GRAPHS) << logprefix << "No read access to "
                                       << databaseName << "." << col;
      return TRI_ERROR_FORBIDDEN;
    }
    if (!collectionExists(col) && !canUseDatabaseRW) {
      LOG_TOPIC(DEBUG, Logger::GRAPHS) << logprefix << "Creation of "
                                       << databaseName << "." << col
                                       << " is not allowed.";
      return TRI_ERROR_FORBIDDEN;
    }
  }

  // We need RW on _graphs (which is the same as RW on the database). But in
  // case we don't even have RO access, throw FORBIDDEN instead of READ_ONLY.
  if (!execContext->canUseCollection(StaticStrings::GraphCollection,
                                     auth::Level::RO)) {
    LOG_TOPIC(DEBUG, Logger::GRAPHS) << logprefix << "No read access to "
                                     << databaseName << "."
                                     << StaticStrings::GraphCollection;
    return TRI_ERROR_FORBIDDEN;
  }

  // Note that this check includes the following check in the loop
  //   if (!collectionExists(col) && !canUseDatabaseRW)
  // as canUseDatabase(RW) <=> canUseCollection("_...", RW).
  // However, in case a collection has to be created but can't, we have to throw
  // FORBIDDEN instead of READ_ONLY for backwards compatibility.
  if (!execContext->canUseCollection(StaticStrings::GraphCollection,
                                     auth::Level::RW)) {
    LOG_TOPIC(DEBUG, Logger::GRAPHS) << logprefix << "No write access to "
                                     << databaseName << "."
                                     << StaticStrings::GraphCollection;
    return TRI_ERROR_ARANGO_READ_ONLY;
  }

  return TRI_ERROR_NO_ERROR;
}

bool GraphManager::collectionExists(std::string const &collection) const {
  return getCollectionByName(ctx()->vocbase(), collection) != nullptr;
}
