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
#include "Graph/GraphManager.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/SmartContext.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Aql/SmartGraph.h"

#endif

using namespace arangodb;
using namespace arangodb::graph;
using UserTransaction = transaction::Methods;

std::shared_ptr<transaction::Context> GraphOperations::ctx() const {
  return transaction::SmartContext::Create(_vocbase);
};

OperationResult GraphOperations::changeEdgeDefinitionForGraph(
    const Graph& graph, const EdgeDefinition& newEdgeDef, bool waitForSync,
    transaction::Methods& trx) {
  std::string const& edgeDefinitionName = newEdgeDef.getName();

  auto maybeOldEdgeDef = graph.getEdgeDefinition(edgeDefinitionName);
  if (!maybeOldEdgeDef) {
    // Graph doesn't contain this edge definition, no need to do anything.
    return OperationResult{};
  }
  EdgeDefinition const& oldEdgeDef = maybeOldEdgeDef.get();

  // replace edgeDefinition
  VPackBuilder builder;
  builder.openObject();
  // build edge definitions start
  builder.add(StaticStrings::KeyString, VPackValue(graph.name()));
  builder.add(StaticStrings::GraphEdgeDefinitions,
              VPackValue(VPackValueType::Array));

  // now we have to build a new VPackObject for updating the
  // edgeDefinition
  for (auto const &it : graph.edgeDefinitions()) {
    auto const& edgeDef = it.second;

    if (edgeDef.getName() == newEdgeDef.getName()) {
      newEdgeDef.addToBuilder(builder);
    } else {
      edgeDef.addToBuilder(builder);
    }
  }
  builder.close();  // array 'edgeDefinitions'

  GraphManager gmngr{_vocbase};

  { // add orphans:

    // previous orphans may still be orphans...
    std::set<std::string> orphans{graph.orphanCollections()};

    // previous vertex collections from the overwritten may be orphaned...
    setUnion(orphans, oldEdgeDef.getFrom());
    setUnion(orphans, oldEdgeDef.getTo());

    // ...except they occur in any other edge definition, including the new one.
    for (auto const &it : graph.edgeDefinitions()) {
      std::string const &edgeCollection = it.first;

      EdgeDefinition const &edgeDef =
        edgeCollection == newEdgeDef.getName() ? newEdgeDef : it.second;

      setMinus(orphans, edgeDef.getFrom());
      setMinus(orphans, edgeDef.getTo());
    }

    builder.add(StaticStrings::GraphOrphans, VPackValue(VPackValueType::Array));
    for (auto const &orphan : orphans) {
      builder.add(VPackValue(orphan));
    }
    builder.close(); // array 'orphanCollections'
  }

  builder.close();  // object (the graph)

  std::set<std::string> newCollections;

  // add collections that didn't exist in the graph before to newCollections:
  for (auto const &it : boost::join(newEdgeDef.getFrom(), newEdgeDef.getTo())) {
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

    OperationResult result = gmngr.createVertexCollection(
        newCollection, waitForSync, collectionOptions.slice());
    if (result.fail()) {
      return result;
    }
  }

  OperationOptions options;
  options.waitForSync = waitForSync;
  // now write to database
  return trx.update(StaticStrings::GraphCollection, builder.slice(), options);
}

OperationResult GraphOperations::eraseEdgeDefinition(
    bool waitForSync, std::string edgeDefinitionName, bool dropCollection) {
  // check if edgeCollection is available
  OperationResult result = checkEdgeCollectionAvailability(edgeDefinitionName);
  if (result.fail()) {
    return result;
  }

  if (dropCollection && !hasRWPermissionsFor(edgeDefinitionName)) {
    return OperationResult{TRI_ERROR_FORBIDDEN};
  }

  std::unordered_set<std::string> possibleOrphans;
  std::unordered_set<std::string> usedVertexCollections;
  std::map<std::string, EdgeDefinition> edgeDefs =
      _graph.edgeDefinitions();

  VPackBuilder newEdgeDefs;
  newEdgeDefs.add(VPackValue(VPackValueType::Array));

  for (auto const& edgeDefinition : edgeDefs) {
    if (edgeDefinition.second.getName() == edgeDefinitionName) {
      for (auto const& from : edgeDefinition.second.getFrom()) {
        possibleOrphans.emplace(from);
      }
      for (auto const& to : edgeDefinition.second.getTo()) {
        possibleOrphans.emplace(to);
      }
    } else {
      for (auto const& from : edgeDefinition.second.getFrom()) {
        usedVertexCollections.emplace(from);
      }
      for (auto const& to : edgeDefinition.second.getTo()) {
        usedVertexCollections.emplace(to);
      }

      // add still existing edgeDefinition to builder for update commit
      newEdgeDefs.openObject();
      newEdgeDefs.add("collection",
                      VPackValue(edgeDefinition.second.getName()));
      newEdgeDefs.add("from", VPackValue(VPackValueType::Array));
      for (auto const& from : edgeDefinition.second.getFrom()) {
        newEdgeDefs.add(VPackValue(from));
      }
      newEdgeDefs.close();  // array
      newEdgeDefs.add("to", VPackValue(VPackValueType::Array));
      for (auto const& to : edgeDefinition.second.getTo()) {
        newEdgeDefs.add(VPackValue(to));
      }
      newEdgeDefs.close();  // array
      newEdgeDefs.close();  // object
    }
  }

  newEdgeDefs.close();  // array

  // build orphan array
  VPackBuilder newOrphColls;
  newOrphColls.add(VPackValue(VPackValueType::Array));
  for (auto const& orph : _graph.orphanCollections()) {
    newOrphColls.add(VPackValue(orph));
  }

  for (auto const& po : possibleOrphans) {
    if (usedVertexCollections.find(po) == usedVertexCollections.end()) {
      newOrphColls.add(VPackValue(po));
    }
  }
  newOrphColls.close();  // array

  // remove edgeDefinition from graph config

  OperationOptions options;
  options.waitForSync = waitForSync;

  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(_graph.name()));
  builder.add(StaticStrings::GraphEdgeDefinitions, newEdgeDefs.slice());
  builder.add(StaticStrings::GraphOrphans, newOrphColls.slice());
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
    gmngr.pushCollectionIfMayBeDropped(edgeDefinitionName, _graph.name(),
                                       collectionsToBeRemoved);
    for (auto const& collection : collectionsToBeRemoved) {
      Result resIn;
      Result found = methods::Collections::lookup(
          &_vocbase, collection, [&](LogicalCollection* coll) {
            resIn = methods::Collections::drop(&_vocbase, coll, false,
                                               -1.0);
          });

      if (found.fail()) {
        res = trx.finish(result.result);
        return OperationResult(res);
      } else if (resIn.fail()) {
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

OperationResult GraphOperations::checkEdgeCollectionAvailability(
    std::string edgeCollectionName) {
  bool found = _graph.edgeCollections().find(edgeCollectionName) !=
               _graph.edgeCollections().end();

  if (!found) {
    return OperationResult(TRI_ERROR_GRAPH_EDGE_COLLECTION_NOT_USED);
  }

  return OperationResult(TRI_ERROR_NO_ERROR);
}

OperationResult GraphOperations::checkVertexCollectionAvailability(
    std::string vertexCollectionName) {
  std::shared_ptr<LogicalCollection> def =
      GraphManager::getCollectionByName(_vocbase, vertexCollectionName);

  if (def == nullptr) {
    return OperationResult(Result(
        TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST,
        vertexCollectionName + " " +
            std::string{
                TRI_errno_string(TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST)}));
  }

  return OperationResult(TRI_ERROR_NO_ERROR);
}

OperationResult GraphOperations::editEdgeDefinition(
    VPackSlice edgeDefinitionSlice, bool waitForSync,
    const std::string& edgeDefinitionName) {
  auto maybeEdgeDef = EdgeDefinition::createFromVelocypack(edgeDefinitionSlice);
  if (!maybeEdgeDef) {
    return OperationResult{maybeEdgeDef};
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
  result = gmngr.findOrCreateCollectionsByEdgeDefinition(
      edgeDefinition, waitForSync, collectionsOptions.slice());
  if (result.fail()) {
    return result;
  }

  if (!_graph.hasEdgeCollection(edgeDefinition.getName())) {
    return OperationResult(TRI_ERROR_GRAPH_EDGE_COLLECTION_NOT_USED);
  }

  // change definition for ALL graphs
  VPackBuilder graphsBuilder;
  gmngr.readGraphs(graphsBuilder, arangodb::aql::PART_DEPENDENT);
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
    std::unique_ptr<Graph> graph = Graph::fromPersistence(singleGraph.resolveExternals());
    result =
        changeEdgeDefinitionForGraph(*(graph.get()), edgeDefinition, waitForSync, trx);
  }
  if (result.fail()) {
    return result;
  }

  res = trx.finish(TRI_ERROR_NO_ERROR);
  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }
  return result;
};

OperationResult GraphOperations::addOrphanCollection(VPackSlice document,
                                                     bool waitForSync,
                                                     bool createCollection) {
  GraphManager gmngr{_vocbase};
  std::string collectionName = document.get("collection").copyString();
  std::shared_ptr<LogicalCollection> def;

  OperationResult result;
  VPackBuilder collectionsOptions;
  collectionsOptions.openObject();
  _graph.createCollectionOptions(collectionsOptions, waitForSync);
  collectionsOptions.close();
  if (result.fail()) {
    return result;
  }
  def = GraphManager::getCollectionByName(_vocbase, collectionName);
  if (def == nullptr && createCollection) {
    result = gmngr.createVertexCollection(collectionName, waitForSync,
                                          collectionsOptions.slice());
    if (result.fail()) {
      return result;
    }
  } else if (def == nullptr && !createCollection) {
    return OperationResult(
            Result(TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST,
                   collectionName + " " +
                   std::string{TRI_errno_string(
                           TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST)}));
  } else {
    if (def->type() != 2) {
      return OperationResult(TRI_ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX);
    }
  }

  for (auto const& orphanCollection : _graph.orphanCollections()) {
    if (collectionName == orphanCollection) {
      return OperationResult(TRI_ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS);
    }
  }
  for (auto const& vertexCollection : _graph.vertexCollections()) {
    if (std::find(_graph.orphanCollections().begin(), _graph.orphanCollections().end(),
                  vertexCollection) == _graph.orphanCollections().end()) {
      if (collectionName == vertexCollection) {
        return OperationResult(
            Result(TRI_ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF,
                   collectionName + " " +
                       std::string{TRI_errno_string(
                           TRI_ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF)}));
      }
    }
  }

  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(_graph.name()));
  builder.add(StaticStrings::GraphOrphans, VPackValue(VPackValueType::Array));
  for (auto const& orph : _graph.orphanCollections()) {
    if (orph != collectionName) {
      builder.add(VPackValue(orph));
    }
  }
  builder.add(VPackValue(collectionName));
  builder.close();  // array
  builder.close();  // object

  SingleCollectionTransaction trx(ctx(), StaticStrings::GraphCollection,
                                  AccessMode::Type::WRITE);

  Result res = trx.begin();

  if (!res.ok()) {
    trx.finish(TRI_ERROR_NO_ERROR);
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

OperationResult GraphOperations::eraseOrphanCollection(
    bool waitForSync, std::string collectionName, bool dropCollection) {
  // check if collection exists within the orphan collections
  std::set<std::string> orphanCollections =
          _graph.orphanCollections();

  bool found = false;
  for (auto const& oName : _graph.orphanCollections()) {
    if (oName == collectionName) {
      found = true;
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

  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(_graph.name()));
  builder.add(StaticStrings::GraphOrphans, VPackValue(VPackValueType::Array));
  for (auto const& orph : _graph.orphanCollections()) {
    if (orph != collectionName) {
      builder.add(VPackValue(orph));
    }
  }
  builder.close();  // array
  builder.close();  // object

  SingleCollectionTransaction trx(ctx(), StaticStrings::GraphCollection,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    return OperationResult(res);
  }
  OperationOptions options;
  options.waitForSync = waitForSync;

  result = trx.update(StaticStrings::GraphCollection, builder.slice(), options);
  res = trx.finish(result.result);

  if (dropCollection) {
    std::unordered_set<std::string> collectionsToBeRemoved;
    GraphManager gmngr{_vocbase};
    gmngr.pushCollectionIfMayBeDropped(collectionName, "", collectionsToBeRemoved);

    for (auto const& collection : collectionsToBeRemoved) {
      Result resIn;
      Result found = methods::Collections::lookup(
          &_vocbase, collection, [&](LogicalCollection* coll) {
            resIn = methods::Collections::drop(&_vocbase, coll, false,
                                               -1.0);
          });

      if (found.fail()) {
        return OperationResult(res);
      } else if (resIn.fail()) {
        return OperationResult(res);
      }
    }
  }

  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }
  return result;
}

OperationResult GraphOperations::addEdgeDefinition(
    VPackSlice edgeDefinitionSlice, bool waitForSync) {
  std::shared_ptr<velocypack::Buffer<uint8_t>> buffer =
      EdgeDefinition::sortEdgeDefinition(edgeDefinitionSlice);
  edgeDefinitionSlice = velocypack::Slice(buffer->data());

  auto edgeDefOrError = ResultT<EdgeDefinition>{
      EdgeDefinition::createFromVelocypack(edgeDefinitionSlice)};

  if (edgeDefOrError.fail()) {
    return OperationResult{edgeDefOrError.copy_result()};
  }

  EdgeDefinition& edgeDefinition = edgeDefOrError.get();

  OperationOptions options;
  options.waitForSync = waitForSync;

  std::string const& edgeCollection = edgeDefinition.getName();
  if (_graph.hasEdgeCollection(edgeCollection)) {
    return OperationResult(Result(
        TRI_ERROR_GRAPH_COLLECTION_MULTI_USE,
        edgeCollection + " " + std::string{TRI_errno_string(
                                   TRI_ERROR_GRAPH_COLLECTION_MULTI_USE)}));
  }

  // ... in different graph
  GraphManager gmngr{_vocbase};

  OperationResult result{gmngr.checkForEdgeDefinitionConflicts(edgeDefinition)};
  if (result.fail()) {
    return result;
  }

  Result permRes = checkEdgeDefinitionPermissions(edgeDefinition);
  if (permRes.fail()) {
    return OperationResult{permRes};
  }

  VPackBuilder collectionsOptions;
  collectionsOptions.openObject();
  _graph.createCollectionOptions(collectionsOptions, waitForSync);
  collectionsOptions.close();

  result = gmngr.findOrCreateCollectionsByEdgeDefinition(
      edgeDefinition, waitForSync, collectionsOptions.slice());
  if (result.fail()) {
    return result;
  }
  std::unordered_set<std::string> usedVertexCollections;

  std::map<std::string, EdgeDefinition> edgeDefs = _graph.edgeDefinitions();
  // build the updated document, reuse the builder
  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(_graph.name()));
  builder.add(StaticStrings::GraphEdgeDefinitions,
              VPackValue(VPackValueType::Array));
  for (auto const& it : edgeDefs) {
    builder.add(VPackValue(VPackValueType::Object));
    builder.add("collection", VPackValue(it.second.getName()));
    // from
    builder.add("from", VPackValue(VPackValueType::Array));
    for (auto const& from : it.second.getFrom()) {
      usedVertexCollections.emplace(from);
      builder.add(VPackValue(from));
    }
    builder.close();  // from
    // to
    builder.add("to", VPackValue(VPackValueType::Array));
    for (auto const& to : it.second.getTo()) {
      usedVertexCollections.emplace(to);
      builder.add(VPackValue(to));
    }
    builder.close();  // to
    builder.close();  // obj
  }
  edgeDefinition.addToBuilder(builder);
  builder.close();  // array

  builder.add(StaticStrings::GraphOrphans, VPackValue(VPackValueType::Array));
  for (auto const& orphan : _graph.orphanCollections()) {
    auto it = usedVertexCollections.find(orphan);
    if (it != usedVertexCollections.end()) {
      // not found, so use in orphans
      builder.add(VPackValue(orphan));
    }
  }
  builder.close();  // array

  builder.close();  // object

  Result res;
  {
    SingleCollectionTransaction trx(ctx(), StaticStrings::GraphCollection,
                                    AccessMode::Type::WRITE);
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

    res = trx.begin();

    if (!res.ok()) {
      return OperationResult(res);
    }

    result =
        trx.update(StaticStrings::GraphCollection, builder.slice(), options);

    res = trx.finish(result.result);
  }

  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }

  return result;
};

// vertices

// TODO check if collection is a vertex collection in _graph?
// TODO are orphans allowed?
OperationResult GraphOperations::getVertex(std::string const& collectionName,
                                           std::string const& key,
                                           boost::optional<TRI_voc_rid_t> rev) {
  return getDocument(collectionName, key, std::move(rev));
};

// TODO check if definitionName is an edge collection in _graph?
OperationResult GraphOperations::getEdge(const std::string& definitionName,
                                         const std::string& key,
                                         boost::optional<TRI_voc_rid_t> rev) {
  return getDocument(definitionName, key, std::move(rev));
}

OperationResult GraphOperations::getDocument(
    std::string const& collectionName, std::string const& key,
    boost::optional<TRI_voc_rid_t> rev) {
  OperationOptions options;
  options.ignoreRevs = !rev.is_initialized();

  VPackBufferPtr searchBuffer = _getSearchSlice(key, rev);
  VPackSlice search{searchBuffer->data()};

  // find and load collection given by name or identifier
  SingleCollectionTransaction trx(ctx(), collectionName,
                                  AccessMode::Type::READ);
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
    const std::string& key, boost::optional<TRI_voc_rid_t>& rev) const {
  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add(StaticStrings::KeyString, VPackValue(key));
    if (rev) {
      builder.add(StaticStrings::RevString,
                  VPackValue(TRI_RidToString(rev.get())));
    }
  }

  return builder.buffer();
}

OperationResult GraphOperations::removeEdge(const std::string& definitionName,
                                            const std::string& key,
                                            boost::optional<TRI_voc_rid_t> rev,
                                            bool waitForSync, bool returnOld) {
  OperationOptions options;
  options.waitForSync = waitForSync;
  options.returnOld = returnOld;
  options.ignoreRevs = !rev.is_initialized();

  VPackBufferPtr searchBuffer = _getSearchSlice(key, rev);
  VPackSlice search{searchBuffer->data()};

  SingleCollectionTransaction trx{ctx(), definitionName,
                                  AccessMode::Type::WRITE};
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    return OperationResult(res);
  }

  OperationResult result = trx.remove(definitionName, search, options);

  res = trx.finish(result.result);

  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }
  return result;
}

OperationResult GraphOperations::modifyDocument(
    std::string const& collectionName, std::string const& key,
    VPackSlice document, bool isPatch, boost::optional<TRI_voc_rid_t> rev,
    bool waitForSync, bool returnOld, bool returnNew, bool keepNull) {
  OperationOptions options;
  options.ignoreRevs = !rev.is_initialized();
  options.waitForSync = waitForSync;
  options.returnNew = returnNew;
  options.returnOld = returnOld;
  if (isPatch) {
    options.keepNull = keepNull;
    // options.mergeObjects = true;
  }

  // extract the revision, if single document variant and header given:
  std::unique_ptr<VPackBuilder> builder;

  VPackSlice keyInBody = document.get(StaticStrings::KeyString);
  if ((rev && TRI_ExtractRevisionId(document) != rev.get()) ||
      keyInBody.isNone() || keyInBody.isNull() ||
      (keyInBody.isString() && keyInBody.copyString() != key)) {
    // We need to rewrite the document with the given revision and key:
    builder = std::make_unique<VPackBuilder>();
    {
      VPackObjectBuilder guard(builder.get());
      TRI_SanitizeObject(document, *builder);
      builder->add(StaticStrings::KeyString, VPackValue(key));
      if (rev) {
        builder->add(StaticStrings::RevString,
                     VPackValue(TRI_RidToString(rev.get())));
      }
    }
    document = builder->slice();
  }

  // find and load collection given by name or identifier
  SingleCollectionTransaction trx(ctx(), collectionName,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    return OperationResult(res);
  }

  OperationResult result;

  if (isPatch) {
    result = trx.update(collectionName, document, options);
  } else {
    result = trx.replace(collectionName, document, options);
  }

  res = trx.finish(result.result);

  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }
  return result;
}

OperationResult GraphOperations::createDocument(
    transaction::Methods* trx, std::string const& collectionName,
    VPackSlice document, bool waitForSync, bool returnNew) {
  OperationOptions options;
  options.waitForSync = waitForSync;
  options.returnNew = returnNew;

  OperationResult result;
  result = trx->insert(collectionName, document, options);

  if (!result.ok()) {
    trx->finish(result.result);
    return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  }
  Result res = trx->finish(result.result);

  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }
  return result;
}

OperationResult GraphOperations::updateEdge(const std::string& definitionName,
                                            const std::string& key,
                                            VPackSlice document,
                                            boost::optional<TRI_voc_rid_t> rev,
                                            bool waitForSync, bool returnOld,
                                            bool returnNew, bool keepNull) {
  return modifyDocument(definitionName, key, document, true, std::move(rev),
                        waitForSync, returnOld, returnNew, keepNull);
}

OperationResult GraphOperations::replaceEdge(const std::string& definitionName,
                                             const std::string& key,
                                             VPackSlice document,
                                             boost::optional<TRI_voc_rid_t> rev,
                                             bool waitForSync, bool returnOld,
                                             bool returnNew, bool keepNull) {
  return modifyDocument(definitionName, key, document, false, std::move(rev),
                        waitForSync, returnOld, returnNew, keepNull);
}

OperationResult GraphOperations::createEdge(const std::string& definitionName,
                                            VPackSlice document,
                                            bool waitForSync, bool returnNew) {
  VPackSlice fromStringSlice = document.get(StaticStrings::FromString);
  VPackSlice toStringSlice = document.get(StaticStrings::ToString);

  if (fromStringSlice.isNone() || toStringSlice.isNone()) {
    return OperationResult(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
  }
  std::string fromString = fromStringSlice.copyString();
  std::string toString = toStringSlice.copyString();

  size_t pos = fromString.find('/');
  std::string fromCollectionName;
  std::string fromCollectionKey;
  if (pos != std::string::npos) {
    fromCollectionName = fromString.substr(0, pos);
    fromCollectionKey = fromString.substr(pos + 1, fromString.length());
  } else {
    return OperationResult(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
  }

  pos = toString.find('/');
  std::string toCollectionName;
  std::string toCollectionKey;
  if (pos != std::string::npos) {
    toCollectionName = toString.substr(0, pos);
    toCollectionKey = toString.substr(pos + 1, toString.length());
  } else {
    return OperationResult(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
  }

  // check if vertex collections are part of the graph definition
  auto it = _graph.vertexCollections().find(fromCollectionName);
  if (it == _graph.vertexCollections().end()) {
    // not found from vertex
    return OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }
  it = _graph.vertexCollections().find(toCollectionName);
  if (it == _graph.vertexCollections().end()) {
    // not found to vertex
    return OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  OperationOptions options;

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

  std::vector<std::string> readCollections;
  std::vector<std::string> writeCollections;
  readCollections.emplace_back(fromCollectionName);
  readCollections.emplace_back(toCollectionName);
  writeCollections.emplace_back(definitionName);

  transaction::Options trxOptions;
  trxOptions.waitForSync = waitForSync;

  std::unique_ptr<transaction::Methods> trx(new UserTransaction(
      ctx(), readCollections, writeCollections, {}, trxOptions));

  Result res = trx->begin();
  if (!res.ok()) {
    return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  }
  OperationResult resultFrom =
      trx->document(fromCollectionName, bF.slice(), options);
  OperationResult resultTo =
      trx->document(toCollectionName, bT.slice(), options);

  if (!resultFrom.ok()) {
    trx->finish(resultFrom.result);
    return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  }
  if (!resultTo.ok()) {
    trx->finish(resultTo.result);
    return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  }
  return createDocument(trx.get(), definitionName, document, waitForSync,
                        returnNew);
}

OperationResult GraphOperations::updateVertex(
    const std::string& collectionName, const std::string& key,
    VPackSlice document, boost::optional<TRI_voc_rid_t> rev, bool waitForSync,
    bool returnOld, bool returnNew, bool keepNull) {
  return modifyDocument(collectionName, key, document, true, std::move(rev),
                        waitForSync, returnOld, returnNew, keepNull);
}

OperationResult GraphOperations::replaceVertex(
    const std::string& collectionName, const std::string& key,
    VPackSlice document, boost::optional<TRI_voc_rid_t> rev, bool waitForSync,
    bool returnOld, bool returnNew, bool keepNull) {
  return modifyDocument(collectionName, key, document, false, std::move(rev),
                        waitForSync, returnOld, returnNew, keepNull);
}

OperationResult GraphOperations::createVertex(const std::string& collectionName,
                                              VPackSlice document,
                                              bool waitForSync,
                                              bool returnNew) {
  transaction::Options trxOptions;

  std::vector<std::string> writeCollections;
  writeCollections.emplace_back(collectionName);
  std::unique_ptr<transaction::Methods> trx(
      new UserTransaction(ctx(), {}, writeCollections, {}, trxOptions));

  Result res = trx->begin();

  if (!res.ok()) {
    return OperationResult(res);
  }

  return createDocument(trx.get(), collectionName, document, waitForSync,
                        returnNew);
}

OperationResult GraphOperations::removeVertex(
    const std::string& collectionName, const std::string& key,
    boost::optional<TRI_voc_rid_t> rev, bool waitForSync, bool returnOld) {
  OperationOptions options;
  options.waitForSync = waitForSync;
  options.returnOld = returnOld;
  options.ignoreRevs = !rev.is_initialized();

  VPackBufferPtr searchBuffer = _getSearchSlice(key, rev);
  VPackSlice search{searchBuffer->data()};

  auto const& edgeCollections = _graph.edgeCollections();
  std::vector<std::string> trxCollections;

  trxCollections.emplace_back(collectionName);

  for (auto const& it : edgeCollections) {
    trxCollections.emplace_back(it);
  }

  transaction::Options trxOptions;
  trxOptions.waitForSync = waitForSync;
  auto context = ctx();
  UserTransaction trx{context, {}, trxCollections, {}, trxOptions};

  Result res = trx.begin();

  if (!res.ok()) {
    return OperationResult(res);
  }

  OperationResult result = trx.remove(collectionName, search, options);

  {
    aql::QueryString const queryString = aql::QueryString{
        "FOR e IN @@collection "
        "FILTER e._from == @vertexId "
        "OR e._to == @vertexId "
        "REMOVE e IN @@collection"};

    std::string const vertexId = collectionName + "/" + key;

    for (auto const& edgeCollection : edgeCollections) {
      std::shared_ptr<VPackBuilder> bindVars{std::make_shared<VPackBuilder>()};

      bindVars->add(VPackValue(VPackValueType::Object));
      bindVars->add("@collection", VPackValue(edgeCollection));
      bindVars->add("vertexId", VPackValue(vertexId));
      bindVars->close();

      arangodb::aql::Query query(false, _vocbase, queryString, bindVars,
                                 nullptr, arangodb::aql::PART_DEPENDENT);
      query.setTransactionContext(context);

      auto queryResult = query.executeSync(QueryRegistryFeature::QUERY_REGISTRY);

      if (queryResult.code != TRI_ERROR_NO_ERROR) {
        return OperationResult(queryResult.code);
      }
    }
  }

  res = trx.finish(result.result);

  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }
  return result;
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

bool GraphOperations::hasPermissionsFor(std::string const& collection,
                                        auth::Level level) const {
  std::string const& databaseName = _vocbase.name();

  std::stringstream stringstream;
  stringstream << "When checking " << convertFromAuthLevel(level)
               << " permissions for " << databaseName << "." << collection
               << ": ";
  std::string const logprefix = stringstream.str();

  ExecContext const* execContext = ExecContext::CURRENT;
  if (execContext == nullptr) {
    LOG_TOPIC(DEBUG, Logger::GRAPHS) << logprefix
                                     << "Permissions are turned off.";
    return true;
  }

  if (execContext->canUseCollection(collection, level)) {
    return true;
  }

  LOG_TOPIC(DEBUG, Logger::GRAPHS) << logprefix << "Not allowed.";
  return false;
}

Result GraphOperations::checkEdgeDefinitionPermissions(
    EdgeDefinition const& edgeDefinition) const {
  std::string const& databaseName = _vocbase.name();

  std::stringstream stringstream;
  stringstream << "When checking permissions for edge definition `"
               << edgeDefinition.getName() << "` of graph `" << databaseName
               << "." << graph().name() << "`: ";
  std::string const logprefix = stringstream.str();

  ExecContext const* execContext = ExecContext::CURRENT;
  if (execContext == nullptr) {
    LOG_TOPIC(DEBUG, Logger::GRAPHS) << logprefix
                                     << "Permissions are turned off.";
    return TRI_ERROR_NO_ERROR;
  }

  // collect all used collections in one container
  std::set<std::string> graphCollections;
  setUnion(graphCollections, edgeDefinition.getFrom());
  setUnion(graphCollections, edgeDefinition.getTo());
  graphCollections.emplace(edgeDefinition.getName());

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

  return TRI_ERROR_NO_ERROR;
}
