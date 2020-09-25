////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "GraphManager.h"
#include "GraphOperations.h"

#include <velocypack/Buffer.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <array>
#include <boost/range/join.hpp>
#include <utility>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AstNode.h"
#include "Aql/Graphs.h"
#include "Aql/Query.h"
#include "Basics/ReadLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Graph/Graph.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Sharding/ShardingInfo.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/V8Context.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/CollectionCreationInfo.h"
#include "VocBase/Methods/Collections.h"

using namespace arangodb;
using namespace arangodb::graph;
using VelocyPackHelper = basics::VelocyPackHelper;

namespace {
static bool arrayContainsCollection(VPackSlice array, std::string const& colName) {
  TRI_ASSERT(array.isArray());
  for (VPackSlice it : VPackArrayIterator(array)) {
    if (it.stringRef() == colName) {
      return true;
    }
  }
  return false;
}
}  // namespace

std::shared_ptr<transaction::Context> GraphManager::ctx() const {
  // we must use v8
  return transaction::V8Context::CreateWhenRequired(_vocbase, true);
}

Result GraphManager::createEdgeCollection(std::string const& name,
                                          bool waitForSync, VPackSlice options) {
  return createCollection(name, TRI_COL_TYPE_EDGE, waitForSync, options);
}

Result GraphManager::createVertexCollection(std::string const& name,
                                            bool waitForSync, VPackSlice options) {
  return createCollection(name, TRI_COL_TYPE_DOCUMENT, waitForSync, options);
}

Result GraphManager::createCollection(std::string const& name, TRI_col_type_e colType,
                                      bool waitForSync, VPackSlice options) {
  TRI_ASSERT(colType == TRI_COL_TYPE_DOCUMENT || colType == TRI_COL_TYPE_EDGE);

  auto& vocbase = ctx()->vocbase();

  VPackBuilder helper;
  helper.openObject();

  if (ServerState::instance()->isCoordinator()) {
    Result res =
        ShardingInfo::validateShardsAndReplicationFactor(options, vocbase.server(), true);
    if (res.fail()) {
      return res;
    }

    bool const forceOneShard =
        vocbase.server().getFeature<ClusterFeature>().forceOneShard() ||
        vocbase.sharding() == "single";

    if (forceOneShard) {
      // force a single shard with shards distributed like "_graph"
      helper.add(StaticStrings::NumberOfShards, VPackValue(1));
      helper.add(StaticStrings::DistributeShardsLike,
                 VPackValue(vocbase.shardingPrototypeName()));
    }
  }

  helper.close();

  VPackBuilder mergedBuilder =
      VPackCollection::merge(options, helper.slice(), false, true);

  std::shared_ptr<LogicalCollection> coll;
  OperationOptions opOptions(ExecContext::current());
  auto res = arangodb::methods::Collections::create(  // create collection
      vocbase,                                        // collection vocbase
      opOptions,
      name,                   // collection name
      colType,                // collection type
      mergedBuilder.slice(),  // collection properties
      waitForSync, true, false, coll);

  return res;
}

Result GraphManager::findOrCreateVertexCollectionByName(const std::string& name, bool waitForSync,
                                                        VPackSlice options) {
  std::shared_ptr<LogicalCollection> def;

  def = getCollectionByName(ctx()->vocbase(), name);
  if (def == nullptr) {
    return createVertexCollection(name, waitForSync, options);
  }

  return Result(TRI_ERROR_NO_ERROR);
}

bool GraphManager::renameGraphCollection(std::string const& oldName,
                                         std::string const& newName) {
  // todo: return a result, by now just used in the graph modules

  std::vector<std::unique_ptr<Graph>> renamedGraphs;

  auto callback = [&](std::unique_ptr<Graph> graph) -> Result {
    bool renamed = graph->renameCollections(oldName, newName);
    if (renamed) {
      renamedGraphs.emplace_back(std::move(graph));
    }
    return Result{};
  };
  Result res = applyOnAllGraphs(callback);
  if (res.fail()) {
    return false;
  }

  SingleCollectionTransaction trx(ctx(), StaticStrings::GraphCollection,
                                  AccessMode::Type::WRITE);
  res = trx.begin();

  if (!res.ok()) {
    return false;
  }
  OperationOptions options(ExecContext::current());

  for (auto const& graph : renamedGraphs) {
    VPackBuilder builder;
    builder.openObject();
    graph->toPersistence(builder);
    builder.close();

    try {
      OperationResult opRes =
          trx.update(StaticStrings::GraphCollection, builder.slice(), options);
      if (opRes.fail()) {
        res = trx.finish(opRes.result);
        if (res.fail()) {
          return false;
        }
      }
    } catch (...) {
    }
  }

  res = trx.finish(Result());  // if we get here, it was a success
  if (res.fail()) {
    return false;
  }
  return true;
}

Result GraphManager::checkForEdgeDefinitionConflicts(std::map<std::string, EdgeDefinition> const& edgeDefinitions,
                                                     std::string const& graphName) const {
  auto callback = [&](std::unique_ptr<Graph> graph) -> Result {
    if (graph->name() == graphName) {
      // No need to check our graph
      return {TRI_ERROR_NO_ERROR};
    }

    for (auto const& sGED : graph->edgeDefinitions()) {
      std::string const& col = sGED.first;
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
    return {TRI_ERROR_NO_ERROR};
  };
  return applyOnAllGraphs(callback);
}

Result GraphManager::findOrCreateCollectionsByEdgeDefinitions(
    Graph const& graph, std::map<std::string, EdgeDefinition> const& edgeDefinitions,
    bool waitForSync, VPackSlice options) {
  for (auto const& it : edgeDefinitions) {
    EdgeDefinition const& edgeDefinition = it.second;
    Result res = findOrCreateCollectionsByEdgeDefinition(graph, edgeDefinition,
                                                         waitForSync, options);

    if (res.fail()) {
      return res;
    }
  }

  return Result{TRI_ERROR_NO_ERROR};
}

Result GraphManager::findOrCreateCollectionsByEdgeDefinition(Graph const& graph,
                                                             EdgeDefinition const& edgeDefinition,
                                                             bool waitForSync,
                                                             VPackSlice const options) {
  std::string const& edgeCollection = edgeDefinition.getName();
  std::shared_ptr<LogicalCollection> def =
      getCollectionByName(ctx()->vocbase(), edgeCollection);

  if (def == nullptr) {
    Result res = createEdgeCollection(edgeCollection, waitForSync, options);
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
      Result res = createVertexCollection(colName, waitForSync, options);
      if (res.fail()) {
        return res;
      }
    } else {
      auto res = graph.validateCollection(*def.get());
      if (res.fail()) {
        return res;
      }
    }
  }

  return Result{TRI_ERROR_NO_ERROR};
}

/// @brief extract the collection by either id or name, may return nullptr!
std::shared_ptr<LogicalCollection> GraphManager::getCollectionByName(
    const TRI_vocbase_t& vocbase, std::string const& name) {
  if (!name.empty()) {
    // try looking up the collection by name then
    if (arangodb::ServerState::instance()->isRunningInCluster()) {
      ClusterInfo& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
      return ci.getCollectionNT(vocbase.name(), name);
    } else {
      return vocbase.lookupCollection(name);
    }
  }

  return nullptr;
}

bool GraphManager::graphExists(std::string const& graphName) const {
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
    OperationResult checkDoc =
        trx.document(StaticStrings::GraphCollection, checkDocument.slice(), options);
    if (checkDoc.fail()) {
      trx.finish(checkDoc.result);
      return false;
    }
    trx.finish(checkDoc.result);
  } catch (...) {
  }
  return true;
}

ResultT<std::unique_ptr<Graph>> GraphManager::lookupGraphByName(std::string const& name) const {
  SingleCollectionTransaction trx(ctx(), StaticStrings::GraphCollection,
                                  AccessMode::Type::READ);

  Result res = trx.begin();

  if (res.fail()) {
    std::stringstream ss;
    ss << "while looking up graph '" << name << "': " << res.errorMessage();
    res.reset(res.errorNumber(), ss.str());
    return {res};
  }

  VPackBuilder b;
  {
    VPackObjectBuilder guard(&b);
    b.add(StaticStrings::KeyString, VPackValue(name));
  }

  // Default options are enough here
  OperationOptions options;

  OperationResult result = trx.document(StaticStrings::GraphCollection, b.slice(), options);

  // Commit or abort.
  res = trx.finish(result.result);

  if (result.fail()) {
    if (result.errorNumber() == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
      std::string msg = basics::Exception::FillExceptionString(TRI_ERROR_GRAPH_NOT_FOUND,
                                                               name.c_str());
      return Result{TRI_ERROR_GRAPH_NOT_FOUND, std::move(msg)};
    } else {
      return Result{result.errorNumber(),
                    "while looking up graph '" + name + "'"};
    }
  }

  if (res.fail()) {
    std::stringstream ss;
    ss << "while looking up graph '" << name << "': " << res.errorMessage();
    res.reset(res.errorNumber(), ss.str());
    return {res};
  }
  return {Graph::fromPersistence(_vocbase, result.slice())};
}

OperationResult GraphManager::createGraph(VPackSlice document, bool waitForSync) const {
  OperationOptions options(ExecContext::current());
  VPackSlice graphNameSlice = document.get("name");
  if (!graphNameSlice.isString()) {
    return OperationResult{TRI_ERROR_GRAPH_CREATE_MISSING_NAME, options};
  }
  std::string const graphName = graphNameSlice.copyString();

  if (graphExists(graphName)) {
    return OperationResult{TRI_ERROR_GRAPH_DUPLICATE, options};
  }

  auto graphRes = buildGraphFromInput(graphName, document);
  if (graphRes.fail()) {
    return OperationResult{std::move(graphRes).result(), options};
  }
  // Guaranteed to not be nullptr
  std::unique_ptr<Graph> graph = std::move(graphRes.get());
  TRI_ASSERT(graph != nullptr);

  // check permissions
  Result res = checkCreateGraphPermissions(graph.get());
  if (res.fail()) {
    return OperationResult{res, options};
  }

  // check edgeDefinitionConflicts
  res = checkForEdgeDefinitionConflicts(graph->edgeDefinitions(), graph->name());
  if (res.fail()) {
    return OperationResult{res, options};
  }

  // Make sure all collections exist and are created
  res = ensureCollections(graph.get(), waitForSync);
  if (res.fail()) {
    return OperationResult{res, options};
  }

  // finally save the graph
  return storeGraph(*(graph.get()), waitForSync, false);
}

OperationResult GraphManager::storeGraph(Graph const& graph, bool waitForSync,
                                         bool isUpdate) const {
  VPackBuilder builder;
  builder.openObject();
  graph.toPersistence(builder);
  builder.close();

  // Here we need a second transaction.
  // If now someone has created a graph with the same name
  // in the meanwhile, sorry bad luck.
  SingleCollectionTransaction trx(ctx(), StaticStrings::GraphCollection,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  OperationOptions options(ExecContext::current());
  options.waitForSync = waitForSync;
  Result res = trx.begin();
  if (res.fail()) {
    return OperationResult{std::move(res), options};
  }
  OperationResult result =
      isUpdate ? trx.update(StaticStrings::GraphCollection, builder.slice(), options)
               : trx.insert(StaticStrings::GraphCollection, builder.slice(), options);

  if (!result.ok()) {
    trx.finish(result.result);
    return result;
  }
  res = trx.finish(result.result);
  if (res.fail()) {
    return OperationResult{std::move(res), options};
  }
  return result;
}

Result GraphManager::applyOnAllGraphs(std::function<Result(std::unique_ptr<Graph>)> const& callback) const {
  std::string const queryStr{"FOR g IN _graphs RETURN g"};
  arangodb::aql::Query query(transaction::StandaloneContext::Create(_vocbase),
                             arangodb::aql::QueryString{"FOR g IN _graphs RETURN g"},
                             nullptr, nullptr);
  aql::QueryResult queryResult = query.executeSync();

  if (queryResult.result.fail()) {
    if (queryResult.result.is(TRI_ERROR_REQUEST_CANCELED) ||
        (queryResult.result.is(TRI_ERROR_QUERY_KILLED))) {
      return {TRI_ERROR_REQUEST_CANCELED};
    }
    return queryResult.result;
  }

  VPackSlice graphsSlice = queryResult.data->slice();
  if (graphsSlice.isNone()) {
    return {TRI_ERROR_OUT_OF_MEMORY};
  } else if (!graphsSlice.isArray()) {
    LOG_TOPIC("cbe2c", ERR, arangodb::Logger::GRAPHS)
        << "cannot read graphs from _graphs collection";
    return {TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT,
            "Cannot read graphs from _graphs collection"};
  }
  Result res;
  for (VPackSlice it : VPackArrayIterator(graphsSlice)) {
    std::unique_ptr<Graph> graph;
    try {
      graph = Graph::fromPersistence(_vocbase, it.resolveExternals());
    } catch (basics::Exception const& e) {
      return {e.code(), e.message()};
    }
    TRI_ASSERT(graph != nullptr);
    if (graph == nullptr) {
      return {TRI_ERROR_OUT_OF_MEMORY};
    }
    res = callback(std::move(graph));
    if (res.fail()) {
      return res;
    }
  }
  TRI_ASSERT(res.ok());
  return res;
}

Result GraphManager::ensureCollections(Graph* graph, bool waitForSync) const {
  // Validation Phase collect a list of collections to create
  std::unordered_set<std::string> documentCollectionsToCreate{};
  std::unordered_set<std::string> edgeCollectionsToCreate{};
  std::unordered_set<std::shared_ptr<LogicalCollection>> existentDocumentCollections{};
  std::unordered_set<std::shared_ptr<LogicalCollection>> existentEdgeCollections{};

  auto& vocbase = ctx()->vocbase();
  Result innerRes{TRI_ERROR_NO_ERROR};

  // I. Check which collections do exists and which not.
  // Check that all edgeCollections are either to be created
  // or exist in a valid way.
  // a) edge collections
  for (auto const& edgeColl : graph->edgeCollections()) {
    std::shared_ptr<LogicalCollection> col;
    Result res = methods::Collections::lookup(vocbase, edgeColl, col);
    if (res.ok()) {
      TRI_ASSERT(col);
      if (col->type() != TRI_COL_TYPE_EDGE) {
        return Result(TRI_ERROR_GRAPH_EDGE_DEFINITION_IS_DOCUMENT,
                      "Collection: '" + col->name() +
                          "' is not an EdgeCollection");
      } else {
        // found the collection
        existentEdgeCollections.emplace(std::move(col));
      }
    } else if (!res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
      return res;
    } else {
      // not found the collection, need to create it later
      edgeCollectionsToCreate.emplace(edgeColl);
    }
  }

  // Check that all vertexCollections are either to be created
  // or exist in a valid way.
  // b) vertex collections
  for (auto const& vertexColl : graph->vertexCollections()) {
    std::shared_ptr<LogicalCollection> col;
    Result res = methods::Collections::lookup(vocbase, vertexColl, col);
    if (res.ok()) {
      TRI_ASSERT(col);
      existentDocumentCollections.emplace(col);
    } else if (!res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
      return res;
    } else {
      if (edgeCollectionsToCreate.find(vertexColl) == edgeCollectionsToCreate.end()) {
        documentCollectionsToCreate.emplace(vertexColl);
      }
    }
  }

  // II. Validate graph
  // a) Initial Validation
  if (!existentDocumentCollections.empty()) {
    for (auto const& col : existentDocumentCollections) {
      graph->ensureInitial(*col);
    }
  }

  // b) Enterprise Sharding
#ifdef USE_ENTERPRISE
  {
    Result res = ensureEnterpriseCollectionSharding(graph, waitForSync,
                                                    documentCollectionsToCreate);
    if (res.fail()) {
      return res;
    }
  }
#endif

  // III. Validate collections
  // document collections
  for (auto const& col : existentDocumentCollections) {
    Result res = graph->validateCollection(*col);
    if (res.fail()) {
      return res;
    }
  }
  // edge collections
  for (auto const& col : existentEdgeCollections) {
    Result res = graph->validateCollection(*col);
    if (res.fail()) {
      return res;
    }
  }

  // IV. Create collections
  VPackBuilder optionsBuilder;
  optionsBuilder.openObject();
  graph->createCollectionOptions(optionsBuilder, waitForSync);
  optionsBuilder.close();
  VPackSlice options = optionsBuilder.slice();
  std::vector<CollectionCreationInfo> collectionsToCreate;
  collectionsToCreate.reserve(documentCollectionsToCreate.size() +
                              edgeCollectionsToCreate.size());
  // Create Document Collections
  for (auto const& vertexColl : documentCollectionsToCreate) {
    collectionsToCreate.emplace_back(
        CollectionCreationInfo{vertexColl, TRI_COL_TYPE_DOCUMENT, options});
  }

  // Create Edge Collections
  for (auto const& edgeColl : edgeCollectionsToCreate) {
    collectionsToCreate.emplace_back(
        CollectionCreationInfo{edgeColl, TRI_COL_TYPE_EDGE, options});
  }
  if (collectionsToCreate.empty()) {
    // NOTE: Empty graph is allowed.
    return TRI_ERROR_NO_ERROR;
  }

  std::vector<std::shared_ptr<LogicalCollection>> created;
  OperationOptions opOptions(ExecContext::current());
  return methods::Collections::create(vocbase, opOptions, collectionsToCreate,
                                      waitForSync, true, false, nullptr, created);
}

bool GraphManager::onlySatellitesUsed(Graph const* graph) const {
  for (auto const& cname : graph->vertexCollections()) {
    if (!_vocbase.lookupCollection(cname).get()->isSatellite()) {
      return false;
    }
  }

  for (auto const& cname : graph->edgeCollections()) {
    if (!_vocbase.lookupCollection(cname).get()->isSatellite()) {
      return false;
    }
  }

  return true;
}

Result GraphManager::readGraphs(velocypack::Builder& builder) const {
  std::string const queryStr{
      "FOR g IN _graphs RETURN MERGE(g, {name: g._key})"};
  return readGraphByQuery(builder, queryStr);
}

Result GraphManager::readGraphKeys(velocypack::Builder& builder) const {
  std::string const queryStr{"FOR g IN _graphs RETURN g._key"};
  return readGraphByQuery(builder, queryStr);
}

Result GraphManager::readGraphByQuery(velocypack::Builder& builder,
                                      std::string const& queryStr) const {
  arangodb::aql::Query query(ctx(), arangodb::aql::QueryString(queryStr),
                             nullptr, nullptr);

  LOG_TOPIC("f6782", DEBUG, arangodb::Logger::GRAPHS)
      << "starting to load graphs information";
  aql::QueryResult queryResult = query.executeSync();

  if (queryResult.result.fail()) {
    if (queryResult.result.is(TRI_ERROR_REQUEST_CANCELED) ||
        (queryResult.result.is(TRI_ERROR_QUERY_KILLED))) {
      return Result(TRI_ERROR_REQUEST_CANCELED);
    }
    return std::move(queryResult.result);
  }

  VPackSlice graphsSlice = queryResult.data->slice();

  if (graphsSlice.isNone()) {
    return Result(TRI_ERROR_OUT_OF_MEMORY);
  } 
  if (!graphsSlice.isArray()) {
    LOG_TOPIC("338b7", ERR, arangodb::Logger::GRAPHS)
        << "cannot read graphs from _graphs collection";
  }

  builder.add(VPackValue(VPackValueType::Object));
  builder.add("graphs", graphsSlice);
  builder.close();

  return Result(TRI_ERROR_NO_ERROR);
}

Result GraphManager::checkForEdgeDefinitionConflicts(arangodb::graph::EdgeDefinition const& edgeDefinition,
                                                     std::string const& graphName) const {
  std::map<std::string, EdgeDefinition> edgeDefs{
      std::make_pair(edgeDefinition.getName(), edgeDefinition)};

  return checkForEdgeDefinitionConflicts(edgeDefs, graphName);
}

Result GraphManager::checkCreateGraphPermissions(Graph const* graph) const {
  std::string const& databaseName = ctx()->vocbase().name();

  std::stringstream stringstream;
  stringstream << "When creating graph " << databaseName << "." << graph->name() << ": ";
  std::string const logprefix = stringstream.str();

  ExecContext const& execContext = ExecContext::current();
  if (!ExecContext::isAuthEnabled()) {
    LOG_TOPIC("952c0", DEBUG, Logger::GRAPHS) << logprefix << "Permissions are turned off.";
    return TRI_ERROR_NO_ERROR;
  }

  // Test if we are allowed to modify _graphs first.
  // Note that this check includes the following check in the loop
  //   if (!collectionExists(col) && !canUseDatabaseRW)
  // as canUseDatabase(RW) <=> canUseCollection("_...", RW).
  // However, in case a collection has to be created but can't, we have to
  // throw FORBIDDEN instead of READ_ONLY for backwards compatibility.
  if (!execContext.canUseDatabase(auth::Level::RW)) {
    // Check for all collections: if it exists and if we have RO access to it.
    // If none fails the check above we need to return READ_ONLY.
    // Otherwise we return FORBIDDEN
    auto checkCollectionAccess = [&](std::string const& col) -> bool {
      // We need RO on all collections. And, in case any collection does not
      // exist, we need RW on the database.
      if (!collectionExists(col)) {
        LOG_TOPIC("ca4de", DEBUG, Logger::GRAPHS)
            << logprefix << "Cannot create collection " << databaseName << "." << col;
        return false;
      }
      if (!execContext.canUseCollection(col, auth::Level::RO)) {
        LOG_TOPIC("b4d48", DEBUG, Logger::GRAPHS)
            << logprefix << "No read access to " << databaseName << "." << col;
        return false;
      }
      return true;
    };

    // Test all edge Collections
    for (auto const& it : graph->edgeCollections()) {
      if (!checkCollectionAccess(it)) {
        return {TRI_ERROR_FORBIDDEN,
                "Createing Graphs requires RW access on the database (" +
                    databaseName + ")"};
      }
    }

    // Test all vertex Collections
    for (auto const& it : graph->vertexCollections()) {
      if (!checkCollectionAccess(it)) {
        return {TRI_ERROR_FORBIDDEN,
                "Createing Graphs requires RW access on the database (" +
                    databaseName + ")"};
      }
    }

    LOG_TOPIC("89b89", DEBUG, Logger::GRAPHS)
        << logprefix << "No write access to " << databaseName << "."
        << StaticStrings::GraphCollection;
    return {TRI_ERROR_ARANGO_READ_ONLY,
            "Createing Graphs requires RW access on the database (" +
                databaseName + ")"};
  }

  auto checkCollectionAccess = [&](std::string const& col) -> bool {
    // We need RO on all collections. And, in case any collection does not
    // exist, we need RW on the database.
    if (!execContext.canUseCollection(col, auth::Level::RO)) {
      LOG_TOPIC("43c84", DEBUG, Logger::GRAPHS)
          << logprefix << "No read access to " << databaseName << "." << col;
      return false;
    }
    return true;
  };

  // Test all edge Collections
  for (auto const& it : graph->edgeCollections()) {
    if (!checkCollectionAccess(it)) {
      return TRI_ERROR_FORBIDDEN;
    }
  }

  // Test all vertex Collections
  for (auto const& it : graph->vertexCollections()) {
    if (!checkCollectionAccess(it)) {
      return TRI_ERROR_FORBIDDEN;
    }
  }
  return TRI_ERROR_NO_ERROR;
}

bool GraphManager::collectionExists(std::string const& collection) const {
  return getCollectionByName(ctx()->vocbase(), collection) != nullptr;
}

OperationResult GraphManager::removeGraph(Graph const& graph, bool waitForSync,
                                          bool dropCollections) {
  std::unordered_set<std::string> leadersToBeRemoved;
  std::unordered_set<std::string> followersToBeRemoved;
  OperationOptions options(ExecContext::current());

  if (dropCollections) {
    auto addToRemoveCollections = [this, &graph, &leadersToBeRemoved,
                                   &followersToBeRemoved](std::string const& colName) {
      std::shared_ptr<LogicalCollection> col =
          getCollectionByName(ctx()->vocbase(), colName);
      if (col == nullptr) {
        return;
      }

      if (col->distributeShardsLike().empty()) {
        pushCollectionIfMayBeDropped(colName, graph.name(), leadersToBeRemoved);
      } else {
        pushCollectionIfMayBeDropped(colName, graph.name(), followersToBeRemoved);
      }
    };

    for (auto const& vertexCollection : graph.vertexCollections()) {
      addToRemoveCollections(vertexCollection);
    }
    for (auto const& orphanCollection : graph.orphanCollections()) {
      addToRemoveCollections(orphanCollection);
    }
    for (auto const& edgeCollection : graph.edgeCollections()) {
      addToRemoveCollections(edgeCollection);
    }
  }

  Result permRes =
      checkDropGraphPermissions(graph, followersToBeRemoved, leadersToBeRemoved);
  if (permRes.fail()) {
    return OperationResult{std::move(permRes), options};
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add(StaticStrings::KeyString, VPackValue(graph.name()));
  }

  {  // Remove from _graphs
    OperationOptions options(ExecContext::current());
    options.waitForSync = waitForSync;

    Result res;
    SingleCollectionTransaction trx{ctx(), StaticStrings::GraphCollection,
                                    AccessMode::Type::WRITE};

    res = trx.begin();
    if (res.fail()) {
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, options);
    }
    VPackSlice search = builder.slice();
    OperationResult result = trx.remove(StaticStrings::GraphCollection, search, options);

    res = trx.finish(result.result);
    if (result.fail()) {
      return result;
    }
    if (res.fail()) {
      return OperationResult{res, options};
    }
    TRI_ASSERT(res.ok() && result.ok());
  }

  {  // drop collections

    Result firstDropError;
    // remove graph related collections.
    // we are not able to do this in a transaction, so doing it afterwards.
    // there may be no collections to drop when dropCollections is false.
    TRI_ASSERT(dropCollections ||
               (leadersToBeRemoved.empty() && followersToBeRemoved.empty()));
    // drop followers (with distributeShardsLike) first and leaders (which
    // occur in some distributeShardsLike) second.
    for (auto const& cname : boost::join(followersToBeRemoved, leadersToBeRemoved)) {
      Result dropResult;
      std::shared_ptr<LogicalCollection> coll;
      Result found = methods::Collections::lookup(ctx()->vocbase(), cname, coll);
      if (found.ok()) {
        TRI_ASSERT(coll);
        dropResult = arangodb::methods::Collections::drop(*coll, false, -1.0);
      }

      if (dropResult.fail()) {
        LOG_TOPIC("04c88", WARN, Logger::GRAPHS)
            << "While removing graph `" << graph.name() << "`: "
            << "Dropping collection `" << cname << "` failed with error "
            << dropResult.errorNumber() << ": " << dropResult.errorMessage();

        // save the first error:
        if (firstDropError.ok()) {
          firstDropError = dropResult;
        }
      }
    }

    if (firstDropError.fail()) {
      return OperationResult{firstDropError, options};
    }
  }

  return OperationResult{TRI_ERROR_NO_ERROR, options};
}

Result GraphManager::pushCollectionIfMayBeDropped(std::string const& colName,
                                                  std::string const& graphName,
                                                  std::unordered_set<std::string>& toBeRemoved) {
  VPackBuilder graphsBuilder;
  Result result = readGraphs(graphsBuilder);
  if (result.fail()) {
    return result;
  }

  VPackSlice graphs = graphsBuilder.slice();

  bool collectionUnused = true;
  TRI_ASSERT(graphs.get("graphs").isArray());

  if (!graphs.get("graphs").isArray()) {
    return Result(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT);
  }

  for (auto graph : VPackArrayIterator(graphs.get("graphs"))) {
    graph = graph.resolveExternals();
    if (!collectionUnused) {
      // Short circuit
      break;
    }
    if (graph.get(StaticStrings::KeyString).stringRef() == graphName) {
      continue;
    }

    // check edge definitions
    VPackSlice edgeDefinitions = graph.get(StaticStrings::GraphEdgeDefinitions);
    if (edgeDefinitions.isArray()) {
      for (auto const& edgeDefinition : VPackArrayIterator(edgeDefinitions)) {
        // edge collection
        if (edgeDefinition.get("collection").stringRef() == colName) {
          collectionUnused = false;
          break;
        }
        // from's
        if (::arrayContainsCollection(edgeDefinition.get(StaticStrings::GraphFrom), colName)) {
          collectionUnused = false;
          break;
        }
        // to's
        if (::arrayContainsCollection(edgeDefinition.get(StaticStrings::GraphTo), colName)) {
          collectionUnused = false;
          break;
        }
      }
    } else {
      return Result(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT);
    }

    // check orphan collections
    VPackSlice orphanCollections = graph.get(StaticStrings::GraphOrphans);
    if (orphanCollections.isArray()) {
      if (::arrayContainsCollection(orphanCollections, colName)) {
        collectionUnused = false;
        break;
      }
    }
  }

  if (collectionUnused) {
    toBeRemoved.emplace(colName);
  }

  return Result(TRI_ERROR_NO_ERROR);
}

Result GraphManager::checkDropGraphPermissions(
    const Graph& graph, const std::unordered_set<std::string>& followersToBeRemoved,
    const std::unordered_set<std::string>& leadersToBeRemoved) {
  std::string const& databaseName = ctx()->vocbase().name();

  std::stringstream stringstream;
  stringstream << "When dropping graph " << databaseName << "." << graph.name() << ": ";
  std::string const logprefix = stringstream.str();

  ExecContext const& execContext = ExecContext::current();
  if (!ExecContext::isAuthEnabled()) {
    LOG_TOPIC("56c2f", DEBUG, Logger::GRAPHS) << logprefix << "Permissions are turned off.";
    return TRI_ERROR_NO_ERROR;
  }

  bool mustDropAtLeastOneCollection =
      !followersToBeRemoved.empty() || !leadersToBeRemoved.empty();
  bool canUseDatabaseRW = execContext.canUseDatabase(auth::Level::RW);

  if (mustDropAtLeastOneCollection && !canUseDatabaseRW) {
    LOG_TOPIC("fdc57", DEBUG, Logger::GRAPHS)
        << logprefix << "Must drop at least one collection in " << databaseName
        << ", but don't have permissions.";
    return TRI_ERROR_FORBIDDEN;
  }

  for (auto const& col : boost::join(followersToBeRemoved, leadersToBeRemoved)) {
    // We need RW to drop a collection.
    if (!execContext.canUseCollection(col, auth::Level::RW)) {
      LOG_TOPIC("96384", DEBUG, Logger::GRAPHS)
          << logprefix << "No write access to " << databaseName << "." << col;
      return TRI_ERROR_FORBIDDEN;
    }
  }

  // We need RW on _graphs (which is the same as RW on the database). But in
  // case we don't even have RO access, throw FORBIDDEN instead of READ_ONLY.
  if (!execContext.canUseCollection(StaticStrings::GraphCollection, auth::Level::RO)) {
    LOG_TOPIC("bfe63", DEBUG, Logger::GRAPHS)
        << logprefix << "No read access to " << databaseName << "."
        << StaticStrings::GraphCollection;
    return TRI_ERROR_FORBIDDEN;
  }

  // Note that this check includes the following check from before
  //   if (mustDropAtLeastOneCollection && !canUseDatabaseRW)
  // as canUseDatabase(RW) <=> canUseCollection("_...", RW).
  // However, in case a collection has to be created but can't, we have to
  // throw FORBIDDEN instead of READ_ONLY for backwards compatibility.
  if (!execContext.canUseCollection(StaticStrings::GraphCollection, auth::Level::RW)) {
    LOG_TOPIC("bbb09", DEBUG, Logger::GRAPHS)
        << logprefix << "No write access to " << databaseName << "."
        << StaticStrings::GraphCollection;
    return TRI_ERROR_ARANGO_READ_ONLY;
  }

  return TRI_ERROR_NO_ERROR;
}

ResultT<std::unique_ptr<Graph>> GraphManager::buildGraphFromInput(std::string const& graphName,
                                                                  VPackSlice input) const {
  try {
    TRI_ASSERT(input.isObject());
    if (ServerState::instance()->isCoordinator()) {
      VPackSlice s = input.get(StaticStrings::IsSmart);
      if (s.isBoolean() && s.getBoolean()) {
        s = input.get("options");
        if (s.isObject()) {
          s = s.get(StaticStrings::ReplicationFactor);
          if ((s.isNumber() && s.getNumber<int>() == 0) ||
              (s.isString() && s.stringRef() == "satellite")) {
            return Result{TRI_ERROR_BAD_PARAMETER, "invalid combination of 'isSmart' and 'satellite' replicationFactor"};
          }
        }
      }

      // validate numberOfShards and replicationFactor
      Result res =
          ShardingInfo::validateShardsAndReplicationFactor(input.get("options"),
                                                           _vocbase.server(), true);
      if (res.fail()) {
        return res;
      }
    }
    return Graph::fromUserInput(_vocbase, graphName, input,
                                input.get(StaticStrings::GraphOptions));
  } catch (arangodb::basics::Exception const& e) {
    return Result{e.code(), e.message()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL};
  }
}
