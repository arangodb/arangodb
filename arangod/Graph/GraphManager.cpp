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

#include "GraphOperations.h"
#include "GraphManager.h"

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
#include "Transaction/V8Context.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/ExecContext.h"
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
using VelocyPackHelper = basics::VelocyPackHelper;

namespace {
static bool ArrayContainsCollection(VPackSlice array, std::string const& colName) {
  TRI_ASSERT(array.isArray());
  for (auto const& it : VPackArrayIterator(array)) {
      if (it.copyString() == colName) {
        return true;
      }
  }
  return false;
}
}


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
    std::map<std::string, EdgeDefinition> const& edgeDefinitions)
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
    try {
      if (arangodb::ServerState::instance()->isRunningInCluster()) {
        ClusterInfo* ci = ClusterInfo::instance();
        return ci->getCollection(vocbase.name(), name);
      } else {
        return vocbase.lookupCollection(name);
      }
    } catch (...) {
    }
  }

  return nullptr;
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
  if (!graphNameSlice.isString()) {
    return OperationResult{TRI_ERROR_GRAPH_CREATE_MISSING_NAME};
  }
  std::string const graphName = graphNameSlice.copyString();
 
  std::shared_ptr<Graph> graph;
  try {
    graph.reset(lookupGraphByName(ctx(), graphName));
    return OperationResult{TRI_ERROR_GRAPH_DUPLICATE};
  } catch (arangodb::basics::Exception const& e) {
    if (e.code() != TRI_ERROR_GRAPH_NOT_FOUND) {
      // This is a real error throw it.
      throw e;
    }
  } catch (...) {
    throw;
  }
  TRI_ASSERT(graph == nullptr);

  auto graphRes = buildGraphFromInput(graphName, document);
  if (graphRes.fail()) {
    return OperationResult{graphRes.copy_result()};
  }
  graph = graphRes.get();

  // check permissions
  Result res = checkCreateGraphPermissions(graph.get());
  if (res.fail()) {
    return OperationResult{res};
  }

  // check edgeDefinitionConflicts
  res = checkForEdgeDefinitionConflicts(graph->edgeDefinitions());
  if (res.fail()) {
    return OperationResult{res};
  }

  // Make sure all collections exist and are created
  res = ensureCollections(graph.get(), waitForSync);
  if (res.fail()) {
    return OperationResult{res};
  }

  // finally save the graph
  VPackBuilder builder;
  builder.openObject();
  graph->toPersistence(builder);
  builder.close();

  // Here we need a second transaction.
  // If now someone has created a graph with the same name
  // in the meanwhile, sorry bad luck.
  SingleCollectionTransaction trx(ctx(), StaticStrings::GraphCollection,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  OperationOptions options;
  options.waitForSync = waitForSync;
  res = trx.begin();
  if (res.fail()) {
    return OperationResult{res};
  }

  OperationResult result = trx.insert(StaticStrings::GraphCollection, builder.slice(), options);
  if (!result.ok()) {
    trx.finish(result.result);
    return result;
  }
  res = trx.finish(result.result);
  if (res.fail()) {
    return OperationResult{res};
  }
  return result;
}

Result GraphManager::ensureCollections(Graph const* graph, bool waitForSync) const {
  TRI_ASSERT(graph != nullptr);
#ifdef USE_ENTERPRISE
  {
    Result res = ensureSmartCollectionSharding(graph, waitForSync);
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
  TRI_vocbase_t* vocbase = &(ctx()->vocbase());
  Result innerRes{TRI_ERROR_NO_ERROR};
  // Create all VertexCollections, or validate that they are there.
  for (auto const& vertexColl : graph->vertexCollections()) {
    bool found = false;
    Result res = methods::Collections::lookup(
        vocbase, vertexColl,
        [&found, &innerRes, &graph](LogicalCollection* col) {
          innerRes = graph->validateCollection(col);
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
      // Create Document Collection
      res = methods::Collections::create(
          vocbase, vertexColl, TRI_COL_TYPE_DOCUMENT, options, waitForSync, true,
          [&](LogicalCollection* coll) {});
      if (res.fail()) {
        return res;
      }
    }
  }

  // Create all Edge Collections, or validate that they are there.
  for (auto const& edgeColl : graph->edgeCollections()) {
    bool found = false;
    Result res = methods::Collections::lookup(
        vocbase, edgeColl, [&found, &innerRes, &graph](LogicalCollection* col) {
          if (col->type() != TRI_COL_TYPE_EDGE) {
            innerRes.reset(
                TRI_ERROR_GRAPH_EDGE_DEFINITION_IS_DOCUMENT,
                "Collection: '" + col->name() + "' is not an EdgeCollection");
          } else {
            innerRes = graph->validateCollection(col);
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
      // Create Edge Collection
      res = methods::Collections::create(vocbase, edgeColl, TRI_COL_TYPE_EDGE,
                                         options, waitForSync, true,
                                         [&](LogicalCollection* coll) {});
      if (res.fail()) {
        return res;
      }
    }
  }
  return TRI_ERROR_NO_ERROR;
};

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
        << "cannot read graphs from _graphs collection";
  }

  builder.add(VPackValue(VPackValueType::Object));
  builder.add("graphs", graphsSlice);
  builder.close();

  return OperationResult(TRI_ERROR_NO_ERROR);
}

Result GraphManager::checkForEdgeDefinitionConflicts(
    arangodb::graph::EdgeDefinition const& edgeDefinition) const {
  std::map<std::string, EdgeDefinition> edgeDefs{
      std::make_pair(edgeDefinition.getName(), edgeDefinition)};

  return checkForEdgeDefinitionConflicts(edgeDefs);
}

Result GraphManager::checkCreateGraphPermissions(
    Graph const* graph) const {
  std::string const& databaseName = ctx()->vocbase().name();

  std::stringstream stringstream;
  stringstream << "When creating graph " << databaseName << "." << graph->name() << ": ";
  std::string const logprefix = stringstream.str();

  ExecContext const* execContext = ExecContext::CURRENT;
  if (execContext == nullptr) {
    LOG_TOPIC(DEBUG, Logger::GRAPHS) << logprefix
                                     << "Permissions are turned off.";
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
        LOG_TOPIC(DEBUG, Logger::GRAPHS) << logprefix << "Cannot create collection "
                                         << databaseName << "." << col;
        return false;
      }
      if (!execContext->canUseCollection(col, auth::Level::RO)) {
        LOG_TOPIC(DEBUG, Logger::GRAPHS) << logprefix << "No read access to "
                                         << databaseName << "." << col;
        return false;
      }
      return true;
    };

    // Test all edge Collections
    for (auto const& it : graph->edgeCollections()) {
      if (!checkCollectionAccess(it)) {
        return {TRI_ERROR_FORBIDDEN, "Createing Graphs requires RW access on the database (" + databaseName + ")"};
      }
    }

    // Test all vertex Collections
    for (auto const& it : graph->vertexCollections()) {
      if (!checkCollectionAccess(it)) {
        return {TRI_ERROR_FORBIDDEN, "Createing Graphs requires RW access on the database (" + databaseName + ")"};
      }
    }
 
    LOG_TOPIC(DEBUG, Logger::GRAPHS) << logprefix << "No write access to "
                                     << databaseName << "."
                                     << StaticStrings::GraphCollection;
    return {TRI_ERROR_ARANGO_READ_ONLY, "Createing Graphs requires RW access on the database (" + databaseName + ")"};
  }

  auto checkCollectionAccess = [&](std::string const& col) -> bool {
    // We need RO on all collections. And, in case any collection does not
    // exist, we need RW on the database.
    if (!execContext->canUseCollection(col, auth::Level::RO)) {
      LOG_TOPIC(DEBUG, Logger::GRAPHS) << logprefix << "No read access to "
                                       << databaseName << "." << col;
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

bool GraphManager::collectionExists(std::string const &collection) const {
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

  { // drop collections

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
          &ctx()->vocbase(), collection, [&](LogicalCollection* coll) {
            dropResult = methods::Collections::drop(&ctx()->vocbase(), coll,
                                                    false, -1.0);
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
        std::string collection =
            edgeDefinition.get("collection").copyString();
        if (collection == colName) {
          collectionUnused = false;
        }
        // from's
        if (::ArrayContainsCollection(edgeDefinition.get(StaticStrings::GraphFrom), colName)) {
          collectionUnused = false;
          break;
        }
        if (::ArrayContainsCollection(edgeDefinition.get(StaticStrings::GraphTo), colName)) {
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
    LOG_TOPIC(DEBUG, Logger::GRAPHS) << logprefix
                                     << "Permissions are turned off.";
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

  for (auto const& col : boost::join(followersToBeRemoved, leadersToBeRemoved)) {
    // We need RW to drop a collection.
    if (!execContext->canUseCollection(col, auth::Level::RW)) {
      LOG_TOPIC(DEBUG, Logger::GRAPHS) << logprefix << "No write access to "
                                       << databaseName << "." << col;
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

  // Note that this check includes the following check from before
  //   if (mustDropAtLeastOneCollection && !canUseDatabaseRW)
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

#ifndef USE_ENTERPRISE
ResultT<std::shared_ptr<Graph>> GraphManager::buildGraphFromInput(std::string const& graphName, VPackSlice input) const {
  try {
    return {std::make_shared<Graph>(graphName, input)};
  } catch (arangodb::basics::Exception const& e) {
    return Result{e.code(), e.message()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL};
  }
  TRI_ASSERT(false); // Catch all above!
  return {TRI_ERROR_INTERNAL };
}
#endif

/*
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


*/
