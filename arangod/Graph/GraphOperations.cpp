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
/// @author Tobias Gödderz & Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "GraphOperations.h"

#include <velocypack/Buffer.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <array>
#include <boost/range/join.hpp>
#include <utility>

#include "Aql/Query.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/Graph.h"
#include "Graph/GraphManager.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Transaction/Methods.h"
#include "Transaction/SmartContext.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/V8Context.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

using namespace arangodb;
using namespace arangodb::graph;

std::shared_ptr<transaction::Context> GraphOperations::ctx() {
  if (!_ctx) {
    _ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);
  }
  return _ctx;
}

void GraphOperations::checkForUsedEdgeCollections(const Graph& graph,
                                                  const std::string& collectionName,
                                                  std::unordered_set<std::string>& possibleEdgeCollections) {
  for (auto const& it : graph.edgeDefinitions()) {
    if (it.second.isVertexCollectionUsed(collectionName)) {
      possibleEdgeCollections.emplace(it.second.getName());
    }
  }
}

OperationResult GraphOperations::changeEdgeDefinitionForGraph(Graph& graph,
                                                              EdgeDefinition const& newEdgeDef,
                                                              bool waitForSync,
                                                              transaction::Methods& trx) {
  VPackBuilder builder;
  // remove old definition, insert the new one instead
  Result res = graph.replaceEdgeDefinition(newEdgeDef);
  if (res.fail()) {
    return OperationResult(res);
  }

  builder.openObject();
  graph.toPersistence(builder);
  builder.close();

  GraphManager gmngr{_vocbase};
  std::set<std::string> newCollections;

  // add collections that didn't exist in the graph before to newCollections:
  for (auto const& it : boost::join(newEdgeDef.getFrom(), newEdgeDef.getTo())) {
    if (!graph.hasVertexCollection(it) && !graph.hasOrphanCollection(it)) {
      newCollections.emplace(it);
    }
  }

  VPackBuilder collectionOptions;
  collectionOptions.openObject();
  _graph.createCollectionOptions(collectionOptions, waitForSync);
  collectionOptions.close();
  for (auto const& newCollection : newCollections) {
    // While the collection is new in the graph, it may still already exist.
    if (GraphManager::getCollectionByName(_vocbase, newCollection)) {
      continue;
    }

    OperationResult result = gmngr.createVertexCollection(newCollection, waitForSync,
                                                          collectionOptions.slice());
    if (result.fail()) {
      return result;
    }
  }

  OperationOptions options;
  options.waitForSync = waitForSync;
  // now write to database
  return trx.update(StaticStrings::GraphCollection, builder.slice(), options);
}

OperationResult GraphOperations::eraseEdgeDefinition(bool waitForSync,
                                                     std::string const& edgeDefinitionName,
                                                     bool dropCollection) {
  // check if edgeCollection is available
  OperationResult result = checkEdgeCollectionAvailability(edgeDefinitionName);
  if (result.fail()) {
    return result;
  }

  if (dropCollection && !hasRWPermissionsFor(edgeDefinitionName)) {
    return OperationResult{TRI_ERROR_FORBIDDEN};
  }

  // remove edgeDefinition from graph config
  _graph.removeEdgeDefinition(edgeDefinitionName);

  OperationOptions options;
  options.waitForSync = waitForSync;

  VPackBuilder builder;
  builder.openObject();
  _graph.toPersistence(builder);
  builder.close();

  SingleCollectionTransaction trx(ctx(), StaticStrings::GraphCollection,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    res = trx.finish(res);
    return OperationResult(res);
  }
  result = trx.update(StaticStrings::GraphCollection, builder.slice(), options);

  if (dropCollection) {
    std::unordered_set<std::string> collectionsToBeRemoved;
    GraphManager gmngr{_vocbase};

    // add the edge collection itself for removal
    gmngr.pushCollectionIfMayBeDropped(edgeDefinitionName, _graph.name(), collectionsToBeRemoved);
    for (auto const& cname : collectionsToBeRemoved) {
      std::shared_ptr<LogicalCollection> coll;
      res = methods::Collections::lookup(_vocbase, cname, coll);
      if (res.ok()) {
        TRI_ASSERT(coll);
#ifdef USE_ENTERPRISE
        {
          if (coll->type() == TRI_COL_TYPE_DOCUMENT) {
            bool initial = isUsedAsInitialCollection(cname);
            if (initial) {
              return OperationResult(TRI_ERROR_GRAPH_COLLECTION_IS_INITIAL);
            }
          }
        }
#endif
        res = methods::Collections::drop(*coll, false, -1.0);
        if (res.fail()) {
          res = trx.finish(result.result);
          return OperationResult(res);
        }
      } else {
        res = trx.finish(result.result);
        return OperationResult(res);
      }
    }
  }

  res = trx.finish(result.result);

  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }

  return result;
}

OperationResult GraphOperations::checkEdgeCollectionAvailability(std::string const& edgeCollectionName) {
  bool found = _graph.edgeCollections().find(edgeCollectionName) !=
               _graph.edgeCollections().end();

  if (!found) {
    return OperationResult(TRI_ERROR_GRAPH_EDGE_COLLECTION_NOT_USED);
  }

  return OperationResult(TRI_ERROR_NO_ERROR);
}

OperationResult GraphOperations::checkVertexCollectionAvailability(std::string const& vertexCollectionName) {
  std::shared_ptr<LogicalCollection> def =
      GraphManager::getCollectionByName(_vocbase, vertexCollectionName);

  if (def == nullptr) {
    return OperationResult(
        Result(TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST,
               vertexCollectionName + " " +
                   std::string{TRI_errno_string(TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST)}));
  }

  return OperationResult(TRI_ERROR_NO_ERROR);
}

OperationResult GraphOperations::editEdgeDefinition(VPackSlice edgeDefinitionSlice,
                                                    bool waitForSync,
                                                    std::string const& edgeDefinitionName) {
  auto maybeEdgeDef = EdgeDefinition::createFromVelocypack(edgeDefinitionSlice);
  if (!maybeEdgeDef) {
    return OperationResult{std::move(maybeEdgeDef).result()};
  }
  EdgeDefinition const& edgeDefinition = maybeEdgeDef.get();

  // check if edgeCollection is available
  OperationResult result = checkEdgeCollectionAvailability(edgeDefinitionName);
  if (result.fail()) {
    return result;
  }

  Result permRes = checkEdgeDefinitionPermissions(edgeDefinition);
  if (permRes.fail()) {
    return OperationResult{permRes};
  }

  GraphManager gmngr{_vocbase};
  VPackBuilder collectionsOptions;
  collectionsOptions.openObject();
  _graph.createCollectionOptions(collectionsOptions, waitForSync);
  collectionsOptions.close();
  result = gmngr.findOrCreateCollectionsByEdgeDefinition(_graph, edgeDefinition, waitForSync,
                                                         collectionsOptions.slice());
  if (result.fail()) {
    return result;
  }

  if (!_graph.hasEdgeCollection(edgeDefinition.getName())) {
    return OperationResult(TRI_ERROR_GRAPH_EDGE_COLLECTION_NOT_USED);
  }

  // change definition for ALL graphs
  VPackBuilder graphsBuilder;
  gmngr.readGraphs(graphsBuilder);
  VPackSlice graphs = graphsBuilder.slice();

  if (!graphs.get("graphs").isArray()) {
    return OperationResult{TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT};
  }

  SingleCollectionTransaction trx(ctx(), StaticStrings::GraphCollection,
                                  AccessMode::Type::WRITE);

  Result res = trx.begin();

  if (!res.ok()) {
    return OperationResult(res);
  }

  for (auto singleGraph : VPackArrayIterator(graphs.get("graphs"))) {
    std::unique_ptr<Graph> graph =
        Graph::fromPersistence(_vocbase, singleGraph.resolveExternals());
    if (graph->hasEdgeCollection(edgeDefinition.getName())) {
      // only try to modify the edgeDefinition if it's available.
      result = changeEdgeDefinitionForGraph(*(graph.get()), edgeDefinition, waitForSync, trx);
      if (result.fail()) {
        return result;
      }
    }
  }

  res = trx.finish(TRI_ERROR_NO_ERROR);
  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }
  return result;
}

OperationResult GraphOperations::addOrphanCollection(VPackSlice document, bool waitForSync,
                                                     bool createCollection) {
  GraphManager gmngr{_vocbase};
  std::string collectionName = document.get("collection").copyString();
  std::shared_ptr<LogicalCollection> def;

  OperationResult result;

  if (_graph.hasVertexCollection(collectionName)) {
    if (_graph.hasOrphanCollection(collectionName)) {
      return OperationResult(TRI_ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS);
    }
    return OperationResult(
        Result(TRI_ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF,
               collectionName + " " +
                   std::string{TRI_errno_string(TRI_ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF)}));
  }

  // add orphan collection to graph
  _graph.addOrphanCollection(std::string(collectionName));

  def = GraphManager::getCollectionByName(_vocbase, collectionName);
  Result res;

  if (def == nullptr) {
    if (createCollection) {
      // ensure that all collections are available
      res = gmngr.ensureCollections(&_graph, waitForSync);

      if (res.fail()) {
        return OperationResult{std::move(res)};
      }
    } else {
      return OperationResult(
          Result(TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST,
                 collectionName + " " +
                     std::string{TRI_errno_string(TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST)}));
    }
  } else {
    // Hint: Now needed because of the initial property
    res = gmngr.ensureCollections(&_graph, waitForSync);
    if (res.fail()) {
      return OperationResult{std::move(res)};
    }

    if (def->type() != TRI_COL_TYPE_DOCUMENT) {
      return OperationResult(TRI_ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX);
    }

    res = _graph.validateCollection(*(def.get()));
    if (res.fail()) {
      return OperationResult{std::move(res)};
    }
  }

  VPackBuilder builder;
  builder.openObject();
  _graph.toPersistence(builder);
  builder.close();

  SingleCollectionTransaction trx(ctx(), StaticStrings::GraphCollection,
                                  AccessMode::Type::WRITE);

  res = trx.begin();

  if (!res.ok()) {
    return OperationResult(res);
  }

  OperationOptions options;
  options.waitForSync = waitForSync;
  result = trx.update(StaticStrings::GraphCollection, builder.slice(), options);

  res = trx.finish(result.result);
  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }
  return result;
}

OperationResult GraphOperations::eraseOrphanCollection(bool waitForSync,
                                                       std::string const& collectionName,
                                                       bool dropCollection) {
#ifdef USE_ENTERPRISE
  {
    if (dropCollection) {
      bool initial = isUsedAsInitialCollection(collectionName);
      if (initial) {
        return OperationResult(TRI_ERROR_GRAPH_COLLECTION_IS_INITIAL);
      }
    }
  }
#endif

  // check if collection exists within the orphan collections
  bool found = false;
  for (auto const& oName : _graph.orphanCollections()) {
    if (oName == collectionName) {
      found = true;
      break;
    }
  }
  if (!found) {
    return OperationResult(TRI_ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION);
  }

  // check if collection exists in the database
  OperationResult result = checkVertexCollectionAvailability(collectionName);
  if (result.fail()) {
    return result;
  }

  if (!hasRWPermissionsFor(collectionName)) {
    return OperationResult{TRI_ERROR_FORBIDDEN};
  }

  Result res = _graph.removeOrphanCollection(collectionName);
  if (res.fail()) {
    return OperationResult(res);
  }

  VPackBuilder builder;
  builder.openObject();
  _graph.toPersistence(builder);
  builder.close();

  {
    SingleCollectionTransaction trx(ctx(), StaticStrings::GraphCollection,
                                    AccessMode::Type::WRITE);
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

    res = trx.begin();

    if (!res.ok()) {
      return OperationResult(res);
    }
    OperationOptions options;
    options.waitForSync = waitForSync;

    result = trx.update(StaticStrings::GraphCollection, builder.slice(), options);
    res = trx.finish(result.result);
  }

  if (dropCollection) {
    std::unordered_set<std::string> collectionsToBeRemoved;
    GraphManager gmngr{_vocbase};
    res = gmngr.pushCollectionIfMayBeDropped(collectionName, "", collectionsToBeRemoved).result;
      
    if (res.fail()) {
      return OperationResult(res);
    }

    for (auto const& cname : collectionsToBeRemoved) {
      std::shared_ptr<LogicalCollection> coll;
      res = methods::Collections::lookup(_vocbase, cname, coll);
      if (res.ok()) {
        TRI_ASSERT(coll);
        res = methods::Collections::drop(*coll, false, -1.0);
      }
      if (res.fail()) {
        return OperationResult(res);
      }
    }
  }

  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }

  return result;
}

OperationResult GraphOperations::addEdgeDefinition(VPackSlice edgeDefinitionSlice,
                                                   bool waitForSync) {
  ResultT<EdgeDefinition const*> defRes = _graph.addEdgeDefinition(edgeDefinitionSlice);
  if (defRes.fail()) {
    return OperationResult(std::move(defRes).result());
  }
  // Guaranteed to be non nullptr
  TRI_ASSERT(defRes.get() != nullptr);

  // ... in different graph
  GraphManager gmngr{_vocbase};

  OperationResult result{
      gmngr.checkForEdgeDefinitionConflicts(*(defRes.get()), _graph.name())};
  if (result.fail()) {
    // If this fails we will not persist.
    return result;
  }

  Result res = gmngr.ensureCollections(&_graph, waitForSync);

  if (res.fail()) {
    return OperationResult{std::move(res)};
  }

  // finally save the graph
  return gmngr.storeGraph(_graph, waitForSync, true);
}

// vertices

// TODO check if collection is a vertex collection in _graph?
// TODO are orphans allowed?
OperationResult GraphOperations::getVertex(std::string const& collectionName,
                                           std::string const& key,
                                           std::optional<TRI_voc_rid_t> rev) {
  return getDocument(collectionName, key, std::move(rev));
}

// TODO check if definitionName is an edge collection in _graph?
OperationResult GraphOperations::getEdge(const std::string& definitionName,
                                         const std::string& key,
                                         std::optional<TRI_voc_rid_t> rev) {
  return getDocument(definitionName, key, std::move(rev));
}

OperationResult GraphOperations::getDocument(std::string const& collectionName,
                                             std::string const& key,
                                             std::optional<TRI_voc_rid_t> rev) {
  OperationOptions options;
  options.ignoreRevs = !rev.has_value();

  VPackBufferPtr searchBuffer = _getSearchSlice(key, rev);
  VPackSlice search{searchBuffer->data()};

  // find and load collection given by name or identifier
  SingleCollectionTransaction trx(ctx(), collectionName, AccessMode::Type::READ);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    return OperationResult(res);
  }

  OperationResult result = trx.document(collectionName, search, options);

  res = trx.finish(result.result);

  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }
  return result;
}

GraphOperations::VPackBufferPtr GraphOperations::_getSearchSlice(
    std::string const& key, std::optional<TRI_voc_rid_t>& rev) const {
  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add(StaticStrings::KeyString, VPackValue(key));
    if (rev) {
      builder.add(StaticStrings::RevString, VPackValue(TRI_RidToString(rev.value())));
    }
  }

  return builder.steal();
}

OperationResult GraphOperations::removeEdge(std::string const& definitionName,
                                            std::string const& key,
                                            std::optional<TRI_voc_rid_t> rev,
                                            bool waitForSync, bool returnOld) {
  return removeEdgeOrVertex(definitionName, key, rev, waitForSync, returnOld);
}

OperationResult GraphOperations::modifyDocument(
    std::string const& collectionName, std::string const& key, VPackSlice document,
    bool isPatch, std::optional<TRI_voc_rid_t> rev, bool waitForSync,
    bool returnOld, bool returnNew, bool keepNull, transaction::Methods& trx) {
  // extract the revision, if single document variant and header given:
  std::unique_ptr<VPackBuilder> builder;

  VPackSlice keyInBody = document.get(StaticStrings::KeyString);
  if ((rev && TRI_ExtractRevisionId(document) != rev.value()) || keyInBody.isNone() ||
      keyInBody.isNull() || (keyInBody.isString() && keyInBody.copyString() != key)) {
    // We need to rewrite the document with the given revision and key:
    builder = std::make_unique<VPackBuilder>();
    {
      VPackObjectBuilder guard(builder.get());
      TRI_SanitizeObject(document, *builder);
      builder->add(StaticStrings::KeyString, VPackValue(key));
      if (rev) {
        builder->add(StaticStrings::RevString, VPackValue(TRI_RidToString(rev.value())));
      }
    }
    document = builder->slice();
  }

  OperationOptions options;
  options.ignoreRevs = !rev.has_value();
  options.waitForSync = waitForSync;
  options.returnNew = returnNew;
  options.returnOld = returnOld;
  OperationResult result;

  if (isPatch) {
    options.keepNull = keepNull;
    result = trx.update(collectionName, document, options);
  } else {
    result = trx.replace(collectionName, document, options);
  }

  Result res = trx.finish(result.result);

  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }
  return result;
}

OperationResult GraphOperations::createDocument(transaction::Methods* trx,
                                                std::string const& collectionName,
                                                VPackSlice document,
                                                bool waitForSync, bool returnNew) {
  OperationOptions options;
  options.waitForSync = waitForSync;
  options.returnNew = returnNew;

  OperationResult result = trx->insert(collectionName, document, options);
  result.result = trx->finish(result.result);

  return result;
}

OperationResult GraphOperations::updateEdge(const std::string& definitionName,
                                            const std::string& key, VPackSlice document,
                                            std::optional<TRI_voc_rid_t> rev,
                                            bool waitForSync, bool returnOld,
                                            bool returnNew, bool keepNull) {
  OperationResult res;
  std::unique_ptr<transaction::Methods> trx;
  std::tie(res, trx) = validateEdge(definitionName, document, waitForSync, true);
  if (res.fail()) {
    return res;
  }
  TRI_ASSERT(trx != nullptr);

  return modifyDocument(definitionName, key, document, true, std::move(rev),
                        waitForSync, returnOld, returnNew, keepNull, *trx.get());
}

OperationResult GraphOperations::replaceEdge(const std::string& definitionName,
                                             const std::string& key, VPackSlice document,
                                             std::optional<TRI_voc_rid_t> rev,
                                             bool waitForSync, bool returnOld,
                                             bool returnNew, bool keepNull) {
  OperationResult res;
  std::unique_ptr<transaction::Methods> trx;
  std::tie(res, trx) = validateEdge(definitionName, document, waitForSync, false);
  if (res.fail()) {
    return res;
  }
  TRI_ASSERT(trx != nullptr);

  return modifyDocument(definitionName, key, document, false, std::move(rev),
                        waitForSync, returnOld, returnNew, keepNull, *trx.get());
}

std::pair<OperationResult, std::unique_ptr<transaction::Methods>> GraphOperations::validateEdge(
    const std::string& definitionName, const VPackSlice& document,
    bool waitForSync, bool isUpdate) {
  std::string fromCollectionName;
  std::string fromCollectionKey;
  std::string toCollectionName;
  std::string toCollectionKey;

  OperationResult res;
  bool foundEdgeDefinition;

  std::tie(res, foundEdgeDefinition) =
      validateEdgeContent(document, fromCollectionName, fromCollectionKey,
                          toCollectionName, toCollectionKey, isUpdate);
  if (res.fail()) {
    return std::make_pair(std::move(res), nullptr);
  }

  std::vector<std::string> readCollections;
  std::vector<std::string> writeCollections;
  std::vector<std::string> exclusiveCollections;

  if (foundEdgeDefinition) {
    readCollections.emplace_back(fromCollectionName);
    readCollections.emplace_back(toCollectionName);
  }
  writeCollections.emplace_back(definitionName);

  transaction::Options trxOptions;
  trxOptions.waitForSync = waitForSync;

  auto trx = std::make_unique<transaction::Methods>(ctx(), readCollections, writeCollections,
                                                    exclusiveCollections, trxOptions);

  Result tRes = trx->begin();

  if (!tRes.ok()) {
    return std::make_pair(OperationResult(tRes), nullptr);
  }

  if (foundEdgeDefinition) {
    res = validateEdgeVertices(fromCollectionName, fromCollectionKey,
                               toCollectionName, toCollectionKey, *trx.get());

    if (res.fail()) {
      return std::make_pair(std::move(res), nullptr);
    }
  }

  return std::make_pair(std::move(res), std::move(trx));
}

OperationResult GraphOperations::validateEdgeVertices(
    const std::string& fromCollectionName, const std::string& fromCollectionKey,
    const std::string& toCollectionName, const std::string& toCollectionKey,
    transaction::Methods& trx) {
  VPackBuilder bT;
  {
    VPackObjectBuilder guard(&bT);
    bT.add(StaticStrings::KeyString, VPackValue(toCollectionKey));
  }

  VPackBuilder bF;
  {
    VPackObjectBuilder guard(&bF);
    bF.add(StaticStrings::KeyString, VPackValue(fromCollectionKey));
  }

  OperationOptions options;
  OperationResult resultFrom = trx.document(fromCollectionName, bF.slice(), options);
  OperationResult resultTo = trx.document(toCollectionName, bT.slice(), options);

  // actual result doesn't matter here
  if (!resultFrom.ok()) {
    trx.finish(resultFrom.result);
    return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  } else if (!resultTo.ok()) {
    trx.finish(resultTo.result);
    return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  }
  return OperationResult(TRI_ERROR_NO_ERROR);
}

std::pair<OperationResult, bool> GraphOperations::validateEdgeContent(
    const VPackSlice& document, std::string& fromCollectionName, std::string& fromCollectionKey,
    std::string& toCollectionName, std::string& toCollectionKey, bool isUpdate) {
  VPackSlice fromStringSlice = document.get(StaticStrings::FromString);
  VPackSlice toStringSlice = document.get(StaticStrings::ToString);

  if (fromStringSlice.isNone() || toStringSlice.isNone()) {
    if (isUpdate) {
      return std::make_pair(OperationResult(TRI_ERROR_NO_ERROR), false);
    }
    return std::make_pair(OperationResult(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE), false);
  }
  std::string fromString = fromStringSlice.copyString();
  std::string toString = toStringSlice.copyString();

  size_t pos = fromString.find('/');
  if (pos != std::string::npos) {
    fromCollectionName = fromString.substr(0, pos);
    fromCollectionKey = fromString.substr(pos + 1, fromString.length());
  } else {
    return std::make_pair(OperationResult(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE), true);
  }

  pos = toString.find('/');
  if (pos != std::string::npos) {
    toCollectionName = toString.substr(0, pos);
    toCollectionKey = toString.substr(pos + 1, toString.length());
  } else {
    return std::make_pair(OperationResult(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE), true);
  }

  // check if vertex collections are part of the graph definition
  auto it = _graph.vertexCollections().find(fromCollectionName);
  if (it == _graph.vertexCollections().end()) {
    // not found from vertex
    return std::make_pair(OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND), true);
  }
  it = _graph.vertexCollections().find(toCollectionName);
  if (it == _graph.vertexCollections().end()) {
    // not found to vertex
    return std::make_pair(OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND), true);
  }

  return std::make_pair(OperationResult(TRI_ERROR_NO_ERROR), true);
}

OperationResult GraphOperations::createEdge(const std::string& definitionName,
                                            VPackSlice document,
                                            bool waitForSync, bool returnNew) {
  OperationResult res;
  std::unique_ptr<transaction::Methods> trx;
  std::tie(res, trx) = validateEdge(definitionName, document, waitForSync, false);
  if (res.fail()) {
    return res;
  }
  TRI_ASSERT(trx != nullptr);

  return createDocument(&(*trx.get()), definitionName, document, waitForSync, returnNew);
}

OperationResult GraphOperations::updateVertex(const std::string& collectionName,
                                              const std::string& key, VPackSlice document,
                                              std::optional<TRI_voc_rid_t> rev,
                                              bool waitForSync, bool returnOld,
                                              bool returnNew, bool keepNull) {
  std::vector<std::string> writeCollections;
  writeCollections.emplace_back(collectionName);

  transaction::Options trxOptions;
  trxOptions.waitForSync = waitForSync;
  transaction::Methods trx(ctx(), {}, writeCollections, {}, trxOptions);

  Result tRes = trx.begin();

  if (!tRes.ok()) {
    return OperationResult(tRes);
  }
  return modifyDocument(collectionName, key, document, true, std::move(rev),
                        waitForSync, returnOld, returnNew, keepNull, trx);
}

OperationResult GraphOperations::replaceVertex(const std::string& collectionName,
                                               const std::string& key, VPackSlice document,
                                               std::optional<TRI_voc_rid_t> rev,
                                               bool waitForSync, bool returnOld,
                                               bool returnNew, bool keepNull) {
  std::vector<std::string> writeCollections;
  writeCollections.emplace_back(collectionName);

  transaction::Options trxOptions;
  trxOptions.waitForSync = waitForSync;
  transaction::Methods trx(ctx(), {}, writeCollections, {}, trxOptions);

  Result tRes = trx.begin();

  if (!tRes.ok()) {
    return OperationResult(tRes);
  }
  return modifyDocument(collectionName, key, document, false, std::move(rev),
                        waitForSync, returnOld, returnNew, keepNull, trx);
}

OperationResult GraphOperations::createVertex(const std::string& collectionName,
                                              VPackSlice document,
                                              bool waitForSync, bool returnNew) {
  transaction::Options trxOptions;

  std::vector<std::string> writeCollections;
  writeCollections.emplace_back(collectionName);
  transaction::Methods trx(ctx(), {}, writeCollections, {}, trxOptions);

  Result res = trx.begin();

  if (!res.ok()) {
    return OperationResult(res);
  }

  return createDocument(&trx, collectionName, document, waitForSync, returnNew);
}

OperationResult GraphOperations::removeEdgeOrVertex(const std::string& collectionName,
                                                    const std::string& key,
                                                    std::optional<TRI_voc_rid_t> rev,
                                                    bool waitForSync, bool returnOld) {
  OperationOptions options;
  options.waitForSync = waitForSync;
  options.returnOld = returnOld;
  options.ignoreRevs = !rev.has_value();

  VPackBufferPtr searchBuffer = _getSearchSlice(key, rev);
  VPackSlice search{searchBuffer->data()};

  // check for used edge definitions in ALL graphs
  GraphManager gmngr{_vocbase};

  std::unordered_set<std::string> possibleEdgeCollections;

  auto callback = [&](std::unique_ptr<Graph> graph) -> Result {
    checkForUsedEdgeCollections(*graph, collectionName, possibleEdgeCollections);
    return Result{};
  };
  Result res = gmngr.applyOnAllGraphs(callback);
  if (res.fail()) {
    return OperationResult(res);
  }

  auto edgeCollections = _graph.edgeCollections();
  std::vector<std::string> trxCollections;

  trxCollections.emplace_back(collectionName);

  CollectionNameResolver resolver{_vocbase};
  for (auto const& it : edgeCollections) {
    trxCollections.emplace_back(it);
    auto col = resolver.getCollection(it);
    if (col && col->isSmart() && col->type() == TRI_COL_TYPE_EDGE) {
      for (auto const& rn : col->realNames()) {
        trxCollections.emplace_back(rn);
      }
    }
  }
  for (auto const& it : possibleEdgeCollections) {
    trxCollections.emplace_back(it);  // add to trx collections
    edgeCollections.emplace(it);  // but also to edgeCollections for later iteration
  }

  transaction::Options trxOptions;
  trxOptions.waitForSync = waitForSync;
  transaction::Methods trx{ctx(), {}, trxCollections, {}, trxOptions};

  res = trx.begin();

  if (!res.ok()) {
    return OperationResult(res);
  }

  OperationResult result = trx.remove(collectionName, search, options);

  {
    aql::QueryString const queryString{
        "FOR e IN @@collection "
        "FILTER e._from == @toDeleteId "
        "OR e._to == @toDeleteId "
        "REMOVE e IN @@collection"};

    std::string const toDeleteId = collectionName + "/" + key;

    for (auto const& edgeCollection : edgeCollections) {
      auto bindVars = std::make_shared<VPackBuilder>();
      bindVars->add(VPackValue(VPackValueType::Object));
      bindVars->add("@collection", VPackValue(edgeCollection));
      bindVars->add("toDeleteId", VPackValue(toDeleteId));
      bindVars->close();

      arangodb::aql::Query query(ctx(), queryString, bindVars, nullptr);
      auto queryResult = query.executeSync();

      if (queryResult.result.fail()) {
        return OperationResult(std::move(queryResult.result));
      }
    }
  }

  res = trx.finish(result.result);

  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }
  return result;
}

OperationResult GraphOperations::removeVertex(const std::string& collectionName,
                                              const std::string& key,
                                              std::optional<TRI_voc_rid_t> rev,
                                              bool waitForSync, bool returnOld) {
  return removeEdgeOrVertex(collectionName, key, rev, waitForSync, returnOld);
}

bool GraphOperations::collectionExists(std::string const& collection) const {
  GraphManager gmngr{_vocbase};
  return gmngr.collectionExists(collection);
}

bool GraphOperations::hasROPermissionsFor(std::string const& collection) const {
  return hasPermissionsFor(collection, auth::Level::RO);
}

bool GraphOperations::hasRWPermissionsFor(std::string const& collection) const {
  return hasPermissionsFor(collection, auth::Level::RW);
}

bool GraphOperations::hasPermissionsFor(std::string const& collection, auth::Level level) const {
  std::string const& databaseName = _vocbase.name();

  std::stringstream stringstream;
  stringstream << "When checking " << convertFromAuthLevel(level)
               << " permissions for " << databaseName << "." << collection << ": ";
  std::string const logprefix = stringstream.str();

  ExecContext const& execContext = ExecContext::current();
  if (!ExecContext::isAuthEnabled()) {
    LOG_TOPIC("08e1f", DEBUG, Logger::GRAPHS) << logprefix << "Permissions are turned off.";
    return true;
  }

  if (execContext.canUseCollection(collection, level)) {
    return true;
  }

  LOG_TOPIC("ef8d1", DEBUG, Logger::GRAPHS) << logprefix << "Not allowed.";
  return false;
}

Result GraphOperations::checkEdgeDefinitionPermissions(EdgeDefinition const& edgeDefinition) const {
  std::string const& databaseName = _vocbase.name();

  std::stringstream stringstream;
  stringstream << "When checking permissions for edge definition `"
               << edgeDefinition.getName() << "` of graph `" << databaseName
               << "." << graph().name() << "`: ";
  std::string const logprefix = stringstream.str();

  ExecContext const& execContext = ExecContext::current();
  if (!ExecContext::isAuthEnabled()) {
    LOG_TOPIC("18e8e", DEBUG, Logger::GRAPHS) << logprefix << "Permissions are turned off.";
    return TRI_ERROR_NO_ERROR;
  }

  // collect all used collections in one container
  std::set<std::string> graphCollections;
  setUnion(graphCollections, edgeDefinition.getFrom());
  setUnion(graphCollections, edgeDefinition.getTo());
  graphCollections.emplace(edgeDefinition.getName());

  bool canUseDatabaseRW = execContext.canUseDatabase(auth::Level::RW);
  for (auto const& col : graphCollections) {
    // We need RO on all collections. And, in case any collection does not
    // exist, we need RW on the database.
    if (!execContext.canUseCollection(col, auth::Level::RO)) {
      LOG_TOPIC("e8a53", DEBUG, Logger::GRAPHS)
          << logprefix << "No read access to " << databaseName << "." << col;
      return TRI_ERROR_FORBIDDEN;
    }
    if (!collectionExists(col) && !canUseDatabaseRW) {
      LOG_TOPIC("2bcf2", DEBUG, Logger::GRAPHS)
          << logprefix << "Creation of " << databaseName << "." << col
          << " is not allowed.";
      return TRI_ERROR_FORBIDDEN;
    }
  }

  return TRI_ERROR_NO_ERROR;
}
