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
#include "GraphOperations.h"

#include <velocypack/Buffer.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <array>
#include <boost/range/join.hpp>
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
#include "Transaction/V8Context.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

using namespace arangodb;
using namespace arangodb::graph;
using VelocyPackHelper = basics::VelocyPackHelper;

namespace {
static bool ArrayContainsCollection(VPackSlice array,
                                    std::string const& colName) {
  TRI_ASSERT(array.isArray());
  for (auto const& it : VPackArrayIterator(array)) {
    if (it.copyString() == colName) {
      return true;
    }
  }
  return false;
}
}  // namespace

std::shared_ptr<transaction::Context> GraphManager::ctx() const {
  if (_isInTransaction) {
    // we must use v8
    return transaction::V8Context::Create(_vocbase, true);
  }

  return transaction::StandaloneContext::Create(_vocbase);
};

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

  Result res = methods::Collections::create(
      &ctx()->vocbase(), name, colType, options, waitForSync, true,
      [](std::shared_ptr<LogicalCollection> const&) -> void {});

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
  OperationOptions options;
  OperationResult checkDoc;

  for (auto const& graph : renamedGraphs) {
    VPackBuilder builder;
    builder.openObject();
    graph->toPersistence(builder);
    builder.close();
    
    try {
      OperationResult checkDoc =
          trx.update(StaticStrings::GraphCollection, builder.slice(), options);
      if (checkDoc.fail()) {
        res = trx.finish(checkDoc.result);
        if (res.fail()) {
          return false;
        }
      }
    } catch (...) {
    }
  };

  res = trx.finish(checkDoc.result);
  if (res.fail()) {
    return false;
  }
  return true;
}

Result GraphManager::checkForEdgeDefinitionConflicts(
    std::map<std::string, EdgeDefinition> const& edgeDefinitions,
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
          return Result(
              TRI_ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS,
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

OperationResult GraphManager::findOrCreateCollectionsByEdgeDefinitions(
    std::map<std::string, EdgeDefinition> const& edgeDefinitions,
    bool waitForSync, VPackSlice options) {
  for (auto const& it : edgeDefinitions) {
    EdgeDefinition const& edgeDefinition = it.second;
    OperationResult res = findOrCreateCollectionsByEdgeDefinition(
        edgeDefinition, waitForSync, options);

    if (res.fail()) {
      return res;
    }
  }

  return OperationResult{TRI_ERROR_NO_ERROR};
}

OperationResult GraphManager::findOrCreateCollectionsByEdgeDefinition(
    EdgeDefinition const& edgeDefinition, bool waitForSync,
    VPackSlice const options) {
  std::string const& edgeCollection = edgeDefinition.getName();
  std::shared_ptr<LogicalCollection> def =
      getCollectionByName(ctx()->vocbase(), edgeCollection);

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

  return OperationResult{TRI_ERROR_NO_ERROR};
}

/// @brief extract the collection by either id or name, may return nullptr!
std::shared_ptr<LogicalCollection> GraphManager::getCollectionByName(
    const TRI_vocbase_t& vocbase, std::string const& name) {
  if (!name.empty()) {
    // try looking up the collection by name then
    if (arangodb::ServerState::instance()->isRunningInCluster()) {
        ClusterInfo* ci = ClusterInfo::instance();
        return ci->getCollectionNT(vocbase.name(), name);
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

ResultT<std::unique_ptr<Graph>> GraphManager::lookupGraphByName(
    std::string const& name) const {
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

  OperationResult result =
      trx.document(StaticStrings::GraphCollection, b.slice(), options);

  // Commit or abort.
  res = trx.finish(result.result);

  if (result.fail()) {
    if (result.errorNumber() == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
      std::string msg = basics::Exception::FillExceptionString(TRI_ERROR_GRAPH_NOT_FOUND, name.c_str());
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
  return {Graph::fromPersistence(result.slice(), _vocbase)};
}

OperationResult GraphManager::createGraph(VPackSlice document,
                                          bool waitForSync) const {
  VPackSlice graphNameSlice = document.get("name");
  if (!graphNameSlice.isString()) {
    return OperationResult{TRI_ERROR_GRAPH_CREATE_MISSING_NAME};
  }
  std::string const graphName = graphNameSlice.copyString();

  if (graphExists(graphName)) {
    return OperationResult{TRI_ERROR_GRAPH_DUPLICATE};
  }

  auto graphRes = buildGraphFromInput(graphName, document);
  if (graphRes.fail()) {
    return OperationResult{graphRes.copy_result()};
  }
  // Guaranteed to not be nullptr
  std::unique_ptr<Graph> graph = std::move(graphRes.get());
  TRI_ASSERT(graph != nullptr);

  // check permissions
  Result res = checkCreateGraphPermissions(graph.get());
  if (res.fail()) {
    return OperationResult{res};
  }

  // check edgeDefinitionConflicts
  res =
      checkForEdgeDefinitionConflicts(graph->edgeDefinitions(), graph->name());
  if (res.fail()) {
    return OperationResult{res};
  }

  // Make sure all collections exist and are created
  res = ensureCollections(graph.get(), waitForSync);
  if (res.fail()) {
    return OperationResult{res};
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

  OperationOptions options;
  options.waitForSync = waitForSync;
  Result res = trx.begin();
  if (res.fail()) {
    return OperationResult{std::move(res)};
  }
  OperationResult result;
  if (isUpdate) {
    result =
        trx.update(StaticStrings::GraphCollection, builder.slice(), options);
  } else {
    result =
        trx.insert(StaticStrings::GraphCollection, builder.slice(), options);
  }
  if (!result.ok()) {
    trx.finish(result.result);
    return result;
  }
  res = trx.finish(result.result);
  if (res.fail()) {
    return OperationResult{std::move(res)};
  }
  return result;
}

Result GraphManager::applyOnAllGraphs(
    std::function<Result(std::unique_ptr<Graph>)> const& callback) const {
  std::string const queryStr{"FOR g IN _graphs RETURN g"};
  arangodb::aql::Query query(
      false, _vocbase, arangodb::aql::QueryString{"FOR g IN _graphs RETURN g"},
      nullptr, nullptr, aql::PART_MAIN);
  aql::QueryResult queryResult =
      query.executeSync(QueryRegistryFeature::registry());

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
        (queryResult.code == TRI_ERROR_QUERY_KILLED)) {
      return {TRI_ERROR_REQUEST_CANCELED};
    }
    return {queryResult.code};
  }

  VPackSlice graphsSlice = queryResult.result->slice();
  if (graphsSlice.isNone()) {
    return {TRI_ERROR_OUT_OF_MEMORY};
  } else if (!graphsSlice.isArray()) {
    LOG_TOPIC(ERR, arangodb::Logger::GRAPHS)
        << "cannot read graphs from _graphs collection";
    return {TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT,
            "Cannot read graphs from _graphs collection"};
  }
  Result res;
  for (auto const& it : VPackArrayIterator(graphsSlice)) {
    std::unique_ptr<Graph> graph;
    try {
      graph = Graph::fromPersistence(it.resolveExternals(), _vocbase);
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

Result GraphManager::ensureCollections(Graph const* graph,
                                       bool waitForSync) const {
  // Validation Phase collect a list of collections to create
  std::unordered_set<std::string> documentCollectionsToCreate{};
  std::unordered_set<std::string> edgeCollectionsToCreate{};

  TRI_vocbase_t* vocbase = &(ctx()->vocbase());
  Result innerRes{TRI_ERROR_NO_ERROR};

  // Check that all edgeCollections are either to be created
  // or exist in a valid way.
  for (auto const& edgeColl : graph->edgeCollections()) {
    bool found = false;
    Result res = methods::Collections::lookup(
        vocbase, edgeColl,
        [&found, &innerRes,
         &graph](std::shared_ptr<LogicalCollection> const& col) -> void {
          TRI_ASSERT(col);
          if (col->type() != TRI_COL_TYPE_EDGE) {
            innerRes.reset(
                TRI_ERROR_GRAPH_EDGE_DEFINITION_IS_DOCUMENT,
                "Collection: '" + col->name() + "' is not an EdgeCollection");
          } else {
            innerRes = graph->validateCollection(*col);
            found = true;
          }
        });

    if (innerRes.fail()) {
      return innerRes;
    }

    // Check if we got an error other then CollectionNotFound
    if (res.fail() && !res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
      return res;
    }

    if (!found) {
      edgeCollectionsToCreate.emplace(edgeColl);
    }
  }

  // Check that all vertexCollections are either to be created
  // or exist in a valid way.
  // If there is an edge collection used as a vertex collection
  // it will not be created twice
  for (auto const& vertexColl : graph->vertexCollections()) {
    bool found = false;
    Result res = methods::Collections::lookup(
        vocbase, vertexColl,
        [&found, &innerRes,
         &graph](std::shared_ptr<LogicalCollection> const& col) -> void {
          TRI_ASSERT(col);
          innerRes = graph->validateCollection(*col);
          found = true;
        });

    if (innerRes.fail()) {
      return innerRes;
    }

    // Check if we got an error other then CollectionNotFound
    if (res.fail() && !res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
      return res;
    }

    if (!found) {
      if (edgeCollectionsToCreate.find(vertexColl) ==
          edgeCollectionsToCreate.end()) {
        documentCollectionsToCreate.emplace(vertexColl);
      }
    }
  }

#ifdef USE_ENTERPRISE
  {
    Result res = ensureSmartCollectionSharding(graph, waitForSync,
                                               documentCollectionsToCreate);
    if (res.fail()) {
      return res;
    }
  }
#endif

  VPackBuilder optionsBuilder;
  optionsBuilder.openObject();
  graph->createCollectionOptions(optionsBuilder, waitForSync);
  optionsBuilder.close();
  VPackSlice options = optionsBuilder.slice();

  // Create Document Collections
  for (auto const& vertexColl : documentCollectionsToCreate) {
    Result res = methods::Collections::create(
        vocbase, vertexColl, TRI_COL_TYPE_DOCUMENT, options, waitForSync, true,
        [](std::shared_ptr<LogicalCollection> const&) -> void {});

    if (res.fail()) {
      return res;
    }
  }

  // Create Edge Collections
  for (auto const& edgeColl : edgeCollectionsToCreate) {
    Result res = methods::Collections::create(
        vocbase, edgeColl, TRI_COL_TYPE_EDGE, options, waitForSync, true,
        [](std::shared_ptr<LogicalCollection> const&) -> void {});

    if (res.fail()) {
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

OperationResult GraphManager::readGraphs(velocypack::Builder& builder,
                                         aql::QueryPart const queryPart) const {
  std::string const queryStr{"FOR g IN _graphs RETURN g"};
  return readGraphByQuery(builder, queryPart, queryStr);
}

OperationResult GraphManager::readGraphKeys(
    velocypack::Builder& builder, aql::QueryPart const queryPart) const {
  std::string const queryStr{"FOR g IN _graphs RETURN g._key"};
  return readGraphByQuery(builder, queryPart, queryStr);
}

OperationResult GraphManager::readGraphByQuery(velocypack::Builder& builder,
                                               aql::QueryPart const queryPart,
                                               std::string queryStr) const {
  arangodb::aql::Query query(false, ctx()->vocbase(),
                             arangodb::aql::QueryString(queryStr), nullptr,
                             nullptr, queryPart);

  LOG_TOPIC(DEBUG, arangodb::Logger::GRAPHS)
      << "starting to load graphs information";
  aql::QueryResult queryResult =
      query.executeSync(QueryRegistryFeature::registry());

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
    LOG_TOPIC(ERR, arangodb::Logger::GRAPHS)
        << "cannot read graphs from _graphs collection";
  }

  builder.add(VPackValue(VPackValueType::Object));
  builder.add("graphs", graphsSlice);
  builder.close();

  return OperationResult(TRI_ERROR_NO_ERROR);
}

Result GraphManager::checkForEdgeDefinitionConflicts(
    arangodb::graph::EdgeDefinition const& edgeDefinition,
    std::string const& graphName) const {
  std::map<std::string, EdgeDefinition> edgeDefs{
      std::make_pair(edgeDefinition.getName(), edgeDefinition)};

  return checkForEdgeDefinitionConflicts(edgeDefs, graphName);
}

Result GraphManager::checkCreateGraphPermissions(Graph const* graph) const {
  std::string const& databaseName = ctx()->vocbase().name();

  std::stringstream stringstream;
  stringstream << "When creating graph " << databaseName << "." << graph->name()
               << ": ";
  std::string const logprefix = stringstream.str();

  ExecContext const* execContext = ExecContext::CURRENT;
  if (execContext == nullptr) {
    LOG_TOPIC(DEBUG, Logger::GRAPHS)
        << logprefix << "Permissions are turned off.";
    return TRI_ERROR_NO_ERROR;
  }

  // Test if we are allowed to modify _graphs first.
  // Note that this check includes the following check in the loop
  //   if (!collectionExists(col) && !canUseDatabaseRW)
  // as canUseDatabase(RW) <=> canUseCollection("_...", RW).
  // However, in case a collection has to be created but can't, we have to throw
  // FORBIDDEN instead of READ_ONLY for backwards compatibility.
  if (!execContext->canUseDatabase(auth::Level::RW)) {
    // Check for all collections: if it exists and if we have RO access to it.
    // If none fails the check above we need to return READ_ONLY.
    // Otherwise we return FORBIDDEN
    auto checkCollectionAccess = [&](std::string const& col) -> bool {
      // We need RO on all collections. And, in case any collection does not
      // exist, we need RW on the database.
      if (!collectionExists(col)) {
        LOG_TOPIC(DEBUG, Logger::GRAPHS)
            << logprefix << "Cannot create collection " << databaseName << "."
            << col;
        return false;
      }
      if (!execContext->canUseCollection(col, auth::Level::RO)) {
        LOG_TOPIC(DEBUG, Logger::GRAPHS)
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

    LOG_TOPIC(DEBUG, Logger::GRAPHS)
        << logprefix << "No write access to " << databaseName << "."
        << StaticStrings::GraphCollection;
    return {TRI_ERROR_ARANGO_READ_ONLY,
            "Createing Graphs requires RW access on the database (" +
                databaseName + ")"};
  }

  auto checkCollectionAccess = [&](std::string const& col) -> bool {
    // We need RO on all collections. And, in case any collection does not
    // exist, we need RW on the database.
    if (!execContext->canUseCollection(col, auth::Level::RO)) {
      LOG_TOPIC(DEBUG, Logger::GRAPHS)
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

  if (dropCollections) {
    auto addToRemoveCollections = [this, &graph, &leadersToBeRemoved,
                                   &followersToBeRemoved](
                                      std::string const& colName) {
      std::shared_ptr<LogicalCollection> col =
          getCollectionByName(ctx()->vocbase(), colName);
      if (col == nullptr) {
        return;
      }

      if (col->distributeShardsLike().empty()) {
        pushCollectionIfMayBeDropped(colName, graph.name(), leadersToBeRemoved);
      } else {
        pushCollectionIfMayBeDropped(colName, graph.name(),
                                     followersToBeRemoved);
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

  Result permRes = checkDropGraphPermissions(graph, followersToBeRemoved,
                                             leadersToBeRemoved);
  if (permRes.fail()) {
    return OperationResult{std::move(permRes)};
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add(StaticStrings::KeyString, VPackValue(graph.name()));
  }

  {  // Remove from _graphs
    OperationOptions options;
    options.waitForSync = waitForSync;

    Result res;
    OperationResult result;
    SingleCollectionTransaction trx{ctx(), StaticStrings::GraphCollection,
                                    AccessMode::Type::WRITE};

    res = trx.begin();
    if (res.fail()) {
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    }
    VPackSlice search = builder.slice();
    result = trx.remove(StaticStrings::GraphCollection, search, options);

    res = trx.finish(result.result);
    if (result.fail()) {
      return result;
    }
    if (res.fail()) {
      return OperationResult{res};
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
    // drop followers (with distributeShardsLike) first and leaders (which occur
    // in some distributeShardsLike) second.
    for (auto const& collection :
         boost::join(followersToBeRemoved, leadersToBeRemoved)) {
      Result dropResult;
      Result found = methods::Collections::lookup(
          &ctx()->vocbase(), collection,
          [&](std::shared_ptr<LogicalCollection> const& coll) -> void {
            TRI_ASSERT(coll);
            dropResult = methods::Collections::drop(&ctx()->vocbase(),
                                                    coll.get(), false, -1.0);
          });

      if (dropResult.fail()) {
        LOG_TOPIC(WARN, Logger::GRAPHS)
            << "While removing graph `" << graph.name() << "`: "
            << "Dropping collection `" << collection << "` failed with error "
            << dropResult.errorNumber() << ": " << dropResult.errorMessage();

        // save the first error:
        if (firstDropError.ok()) {
          firstDropError = dropResult;
        }
      }
    }

    if (firstDropError.fail()) {
      return OperationResult{firstDropError};
    }
  }

  return OperationResult{TRI_ERROR_NO_ERROR};
}

OperationResult GraphManager::pushCollectionIfMayBeDropped(
    const std::string& colName, const std::string& graphName,
    std::unordered_set<std::string>& toBeRemoved) {
  VPackBuilder graphsBuilder;
  OperationResult result = readGraphs(graphsBuilder, aql::PART_DEPENDENT);
  if (result.fail()) {
    return result;
  }

  VPackSlice graphs = graphsBuilder.slice();

  bool collectionUnused = true;
  TRI_ASSERT(graphs.get("graphs").isArray());

  if (!graphs.get("graphs").isArray()) {
    return OperationResult(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT);
  }

  for (auto graph : VPackArrayIterator(graphs.get("graphs"))) {
    graph = graph.resolveExternals();
    if (!collectionUnused) {
      // Short circuit
      break;
    }
    if (graph.get(StaticStrings::KeyString).copyString() == graphName) {
      continue;
    }

    // check edge definitions
    VPackSlice edgeDefinitions = graph.get(StaticStrings::GraphEdgeDefinitions);
    if (edgeDefinitions.isArray()) {
      for (auto const& edgeDefinition : VPackArrayIterator(edgeDefinitions)) {
        // edge collection
        std::string collection = edgeDefinition.get("collection").copyString();
        if (collection == colName) {
          collectionUnused = false;
        }
        // from's
        if (::ArrayContainsCollection(
                edgeDefinition.get(StaticStrings::GraphFrom), colName)) {
          collectionUnused = false;
          break;
        }
        if (::ArrayContainsCollection(
                edgeDefinition.get(StaticStrings::GraphTo), colName)) {
          collectionUnused = false;
          break;
        }
      }
    } else {
      return OperationResult(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT);
    }

    // check orphan collections
    VPackSlice orphanCollections = graph.get(StaticStrings::GraphOrphans);
    if (orphanCollections.isArray()) {
      if (::ArrayContainsCollection(orphanCollections, colName)) {
        collectionUnused = false;
        break;
      }
    }
  }

  if (collectionUnused) {
    toBeRemoved.emplace(colName);
  }

  return OperationResult(TRI_ERROR_NO_ERROR);
}

Result GraphManager::checkDropGraphPermissions(
    const Graph& graph,
    const std::unordered_set<std::string>& followersToBeRemoved,
    const std::unordered_set<std::string>& leadersToBeRemoved) {
  std::string const& databaseName = ctx()->vocbase().name();

  std::stringstream stringstream;
  stringstream << "When dropping graph " << databaseName << "." << graph.name()
               << ": ";
  std::string const logprefix = stringstream.str();

  ExecContext const* execContext = ExecContext::CURRENT;
  if (execContext == nullptr) {
    LOG_TOPIC(DEBUG, Logger::GRAPHS)
        << logprefix << "Permissions are turned off.";
    return TRI_ERROR_NO_ERROR;
  }

  bool mustDropAtLeastOneCollection =
      !followersToBeRemoved.empty() || !leadersToBeRemoved.empty();
  bool canUseDatabaseRW = execContext->canUseDatabase(auth::Level::RW);

  if (mustDropAtLeastOneCollection && !canUseDatabaseRW) {
    LOG_TOPIC(DEBUG, Logger::GRAPHS)
        << logprefix << "Must drop at least one collection in " << databaseName
        << ", but don't have permissions.";
    return TRI_ERROR_FORBIDDEN;
  }

  for (auto const& col :
       boost::join(followersToBeRemoved, leadersToBeRemoved)) {
    // We need RW to drop a collection.
    if (!execContext->canUseCollection(col, auth::Level::RW)) {
      LOG_TOPIC(DEBUG, Logger::GRAPHS)
          << logprefix << "No write access to " << databaseName << "." << col;
      return TRI_ERROR_FORBIDDEN;
    }
  }

  // We need RW on _graphs (which is the same as RW on the database). But in
  // case we don't even have RO access, throw FORBIDDEN instead of READ_ONLY.
  if (!execContext->canUseCollection(StaticStrings::GraphCollection,
                                     auth::Level::RO)) {
    LOG_TOPIC(DEBUG, Logger::GRAPHS)
        << logprefix << "No read access to " << databaseName << "."
        << StaticStrings::GraphCollection;
    return TRI_ERROR_FORBIDDEN;
  }

  // Note that this check includes the following check from before
  //   if (mustDropAtLeastOneCollection && !canUseDatabaseRW)
  // as canUseDatabase(RW) <=> canUseCollection("_...", RW).
  // However, in case a collection has to be created but can't, we have to throw
  // FORBIDDEN instead of READ_ONLY for backwards compatibility.
  if (!execContext->canUseCollection(StaticStrings::GraphCollection,
                                     auth::Level::RW)) {
    LOG_TOPIC(DEBUG, Logger::GRAPHS)
        << logprefix << "No write access to " << databaseName << "."
        << StaticStrings::GraphCollection;
    return TRI_ERROR_ARANGO_READ_ONLY;
  }

  return TRI_ERROR_NO_ERROR;
}

ResultT<std::unique_ptr<Graph>> GraphManager::buildGraphFromInput(
    std::string const& graphName, VPackSlice input) const {
  try {
    TRI_ASSERT(input.isObject());
    return Graph::fromUserInput(graphName, input,
                                input.get(StaticStrings::GraphOptions));
  } catch (arangodb::basics::Exception const& e) {
    return Result{e.code(), e.message()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL};
  }
}
