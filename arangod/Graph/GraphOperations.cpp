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
#include <boost/variant.hpp>
#include <utility>

#include "Aql/AstNode.h"
#include "Aql/Graphs.h"
#include "Aql/Query.h"
#include "Basics/ReadLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Graph/GraphManager.h"
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

std::string const GraphOperations::_graphs = "_graphs";
char const* GraphOperations::_attrEdgeDefs = "edgeDefinitions";
char const* GraphOperations::_attrOrphans = "orphanCollections";

OperationResult GraphOperations::changeEdgeDefinitionsForGraph(
    VPackSlice graph, VPackSlice edgeDefinition,
    std::unordered_set<std::string> possibleOrphans, bool waitForSync,
    transaction::Methods& trx) {
  std::unordered_set<std::string> graphCollections;
  std::string edgeDefinitionName =
      edgeDefinition.get("collection").copyString();
  bool gotAHit = false;

  VPackSlice eDs;

  if (graph.get(GraphOperations::_attrEdgeDefs).isArray()) {
    eDs = graph.get(GraphOperations::_attrEdgeDefs);
  }

  OperationResult result;

  OperationOptions options;
  options.waitForSync = waitForSync;

  std::unordered_map<std::string, EdgeDefinition> edgeDefs =
      _graph.edgeDefinitions();

  // replace edgeDefinition
  for (auto const& eD : VPackArrayIterator(eDs)) {
    if (eD.get("collection").copyString() == edgeDefinitionName) {
      gotAHit = true;
      // now we have to build a new VPackObject for updating the edgeDefinition
      VPackBuilder builder;
      builder.openObject();

      // build edge definitions start
      builder.add(StaticStrings::KeyString,
                  VPackValue(graph.get(StaticStrings::KeyString).copyString()));
      builder.add(GraphOperations::_attrEdgeDefs, VPackValue(VPackValueType::Array));
      for (auto const& it : edgeDefs) {
        builder.add(VPackValue(VPackValueType::Object));
        builder.add("collection", VPackValue(it.second.getName()));
        if (it.second.getName() == edgeDefinitionName) {
          builder.add("from", edgeDefinition.get("from"));
        } else {
          // from
          builder.add("from", VPackValue(VPackValueType::Array));
          for (auto const& from : it.second.getFrom()) {
            builder.add(VPackValue(from));
          }
          builder.close();  // from
        }
        if (it.second.getName() == edgeDefinitionName) {
          builder.add("to", edgeDefinition.get("to"));
        } else {
          // to
          builder.add("to", VPackValue(VPackValueType::Array));
          for (auto const& to : it.second.getTo()) {
            builder.add(VPackValue(to));
          }
          builder.close();  // to
        }
        builder.close();  // obj
      }
      builder.close();  // array
      // build edge definitions end

      builder.close();  // object

      // now write to database
      result = trx.update(GraphOperations::_graphs, builder.slice(), options);
    } else {
      // collect all used collections
      for (auto const& from : VPackArrayIterator(eD.get("from"))) {
        graphCollections.emplace(from.copyString());
      }
      for (auto const& to : VPackArrayIterator(eD.get("to"))) {
        graphCollections.emplace(to.copyString());
      }
    }
  }

  if (!gotAHit) {
    return result;
  }

  GraphManager gmngr{ctx()};
  for (auto const& po : possibleOrphans) {
    gmngr.createVertexCollection(po, waitForSync);
  }

  return result;
}

OperationResult GraphOperations::eraseEdgeDefinition(
    bool waitForSync, std::string edgeDefinitionName, bool dropCollection) {
  // check if edgeCollection is available
  assertEdgeCollectionAvailability(edgeDefinitionName);

  std::unordered_set<std::string> possibleOrphans;
  std::unordered_set<std::string> usedVertexCollections;
  std::unordered_map<std::string, EdgeDefinition> edgeDefs =
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
    if (std::find(usedVertexCollections.begin(), usedVertexCollections.end(),
                  po) != usedVertexCollections.end()) {
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
  builder.add(_graph._attrEdgeDefs, newEdgeDefs.slice());
  builder.add(_graph._attrOrphans, newOrphColls.slice());
  builder.close();

  SingleCollectionTransaction trx(ctx(), GraphOperations::_graphs,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    return OperationResult(res);
  }
  OperationResult result = trx.update(GraphOperations::_graphs, builder.slice(), options);
  res = trx.finish(result.result);

  if (dropCollection) {
    // TODO: also drop collection if not used
    // old gharial api is dropping without a check, docu says we need to check
    // if other graphs are using
    // the collection. what should we do?!
  }

  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }
  return result;
}

void GraphOperations::assertEdgeCollectionAvailability(
    std::string edgeDefinitionName) {
  bool found = false;
  for (auto const& edgeCollection : _graph.edgeCollections()) {
    if (edgeCollection == edgeDefinitionName) {
      found = true;
    }
  }

  if (!found) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_EDGE_COLLECTION_NOT_USED);
  }
}

void GraphOperations::assertVertexCollectionAvailability(
    std::string vertexDefinitionName) {
  GraphManager gmngr{ctx()};

  std::shared_ptr<LogicalCollection> def =
      gmngr.getCollectionByName(ctx()->vocbase(), vertexDefinitionName);
  if (def == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST);
  }
}

OperationResult GraphOperations::editEdgeDefinition(
    VPackSlice edgeDefinition, bool waitForSync,
    const std::string& edgeDefinitionName) {
  std::shared_ptr<velocypack::Buffer<uint8_t>> buffer =
      EdgeDefinition::sortEdgeDefinition(edgeDefinition);
  edgeDefinition = velocypack::Slice(buffer->data());

  // check if edgeCollection is available
  assertEdgeCollectionAvailability(edgeDefinitionName);

  VPackBuilder builder;
  builder.add(VPackValue(VPackValueType::Array));
  builder.add(edgeDefinition);
  builder.close();

  GraphManager gmngr{ctx()};
  OperationResult result = gmngr.findOrCreateCollectionsByEdgeDefinitions(builder.slice(), waitForSync);
  if (result.fail()) {
    return result;
  }

  std::unordered_set<std::string> possibleOrphans;
  std::string currentEdgeDefinitionName;
  for (auto const& ed : _graph.edgeCollections()) {
    if (edgeDefinition.get("collection").isString()) {
      if (edgeDefinition.get("collection").copyString() == ed) {
        currentEdgeDefinitionName = ed;
      }
    }
  }
  if (currentEdgeDefinitionName.empty()) {
    return OperationResult(TRI_ERROR_GRAPH_EDGE_COLLECTION_NOT_USED);
  }

  std::unordered_map<std::string, EdgeDefinition> edgeDefs =
      _graph.edgeDefinitions();
  EdgeDefinition const& currentEdgeDefinition =
      edgeDefs.at(currentEdgeDefinitionName);

  std::unordered_set<std::string> currentCollections;
  std::unordered_set<std::string> newCollections;
  for (auto const& from : currentEdgeDefinition.getFrom()) {
    currentCollections.emplace(from);
  }
  for (auto const& from : currentEdgeDefinition.getTo()) {
    currentCollections.emplace(from);
  }
  for (auto const& it : VPackArrayIterator(edgeDefinition.get("from"))) {
    if (it.isString()) {
      newCollections.emplace(it.copyString());
    }
  }
  for (auto const& it : VPackArrayIterator(edgeDefinition.get("to"))) {
    if (it.isString()) {
      newCollections.emplace(it.copyString());
    }
  }

  for (auto const& colName : currentCollections) {
    if (newCollections.find(colName) == newCollections.end()) {
      possibleOrphans.emplace(colName);
    }
  }
  // change definition for ALL graphs
  VPackBuilder graphsBuilder;
  gmngr.readGraphs(graphsBuilder, arangodb::aql::PART_DEPENDENT);
  VPackSlice graphs = graphsBuilder.slice();

  SingleCollectionTransaction trx(ctx(), GraphOperations::_graphs,
                                  AccessMode::Type::WRITE);

  Result res = trx.begin();

  if (!res.ok()) {
    trx.finish(TRI_ERROR_NO_ERROR);
    return OperationResult(res);
  }

  if (graphs.get("graphs").isArray()) {
    for (auto singleGraph : VPackArrayIterator(graphs.get("graphs"))) {
      result = changeEdgeDefinitionsForGraph(singleGraph.resolveExternals(),
                                             edgeDefinition, possibleOrphans,
                                             waitForSync, trx);
    }
  }

  res = trx.finish(TRI_ERROR_NO_ERROR);
  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }
  return result;
};

OperationResult GraphOperations::addOrphanCollection(VPackSlice document, bool waitForSync) {
  GraphManager gmngr{ctx()};
  std::string collectionName = document.get("collection").copyString();
  std::shared_ptr<LogicalCollection> def;

  OperationResult result;
  def = gmngr.getCollectionByName(ctx()->vocbase(), collectionName);
  if (def == nullptr) {
    result = gmngr.createVertexCollection(collectionName, waitForSync);
    if (result.fail()) {
      return result;
    }
  } else {
    if (def->type() != 2) {
      return OperationResult(TRI_ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX);
    }
  }

  for (auto const& vertexCollection : _graph.vertexCollections()) {
    if (collectionName == vertexCollection) {
      return OperationResult(TRI_ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF);
    }
  }
  for (auto const& orphanCollection : _graph.orphanCollections()) {
    if (collectionName == orphanCollection) {
      return OperationResult(TRI_ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS);
    }
  }

  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(_graph.name()));
  builder.add(_graph._attrOrphans, VPackValue(VPackValueType::Array));
  for (auto const& orph : _graph.orphanCollections()) {
    builder.add(VPackValue(orph));
  }
  builder.add(VPackValue(collectionName));
  builder.close();  // array
  builder.close();  // object

  SingleCollectionTransaction trx(ctx(), GraphOperations::_graphs,
                                  AccessMode::Type::WRITE);

  Result res = trx.begin();

  if (!res.ok()) {
    trx.finish(TRI_ERROR_NO_ERROR);
    return OperationResult(res);
  }

  OperationOptions options;
  options.waitForSync = waitForSync;
  result = trx.update(GraphOperations::_graphs, builder.slice(), options);

  res = trx.finish(result.result);
  if (result.ok() && res.fail()) {
    return OperationResult(res);
  }
  return result;
}

OperationResult GraphOperations::eraseOrphanCollection(
    bool waitForSync, std::string collectionName, bool dropCollection) {
  // check if collection exists
  assertVertexCollectionAvailability(collectionName);

  std::unordered_set<std::string> orphanCollections =
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

  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(_graph.name()));
  builder.add(_graph._attrOrphans, VPackValue(VPackValueType::Array));
  for (auto const& orph : _graph.orphanCollections()) {
    if (orph != collectionName) {
      builder.add(VPackValue(orph));
    }
  }
  builder.close();  // array
  builder.close();  // object

  SingleCollectionTransaction trx(ctx(), GraphOperations::_graphs,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    return OperationResult(res);
  }
  OperationOptions options;
  options.waitForSync = waitForSync;

  OperationResult result = trx.update(GraphOperations::_graphs, builder.slice(), options);
  res = trx.finish(result.result);

  if (dropCollection) {
    std::vector<std::string> collectionsToBeRemoved;
    pushCollectionIfMayBeDropped(collectionName, "", collectionsToBeRemoved);

    for (auto const& collection : collectionsToBeRemoved) {
      Result resIn;
      Result found = methods::Collections::lookup(
          &ctx()->vocbase(), collection, [&](LogicalCollection* coll) {
            resIn = methods::Collections::drop(&ctx()->vocbase(), coll, false,
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
    VPackSlice edgeDefinition, bool waitForSync) {
  Result res = EdgeDefinition::validateEdgeDefinition(edgeDefinition);

  if (res.fail()) {
    return OperationResult(res);
  }

  OperationOptions options;
  options.waitForSync = waitForSync;

  std::shared_ptr<velocypack::Buffer<uint8_t>> buffer =
      EdgeDefinition::sortEdgeDefinition(edgeDefinition);
  edgeDefinition = velocypack::Slice(buffer->data());

  std::string eC = edgeDefinition.get("collection").copyString();
  if (_graph.hasEdgeCollection(eC)) {
    return OperationResult(TRI_ERROR_GRAPH_COLLECTION_MULTI_USE);
  }

  // ... in different graph
  GraphManager gmngr{ctx()};

  OperationResult result = gmngr.checkForEdgeDefinitionConflicts(eC, edgeDefinition);
  if (result.fail()) {
    return result;
  }

  VPackBuilder builder;
  builder.add(VPackValue(VPackValueType::Array));
  builder.add(edgeDefinition);
  builder.close();

  result = gmngr.findOrCreateCollectionsByEdgeDefinitions(builder.slice(), waitForSync);
  if (result.fail()) {
    return result;
  }

  std::unordered_map<std::string, EdgeDefinition> edgeDefs =
      _graph.edgeDefinitions();
  // build the updated document, reuse the builder
  builder.clear();
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(_graph.name()));
  builder.add(GraphOperations::_attrEdgeDefs, VPackValue(VPackValueType::Array));
  for (auto const& it : edgeDefs) {
    builder.add(VPackValue(VPackValueType::Object));
    builder.add("collection", VPackValue(it.second.getName()));
    // from
    builder.add("from", VPackValue(VPackValueType::Array));
    for (auto const& from : it.second.getFrom()) {
      builder.add(VPackValue(from));
    }
    builder.close();  // from
    // to
    builder.add("to", VPackValue(VPackValueType::Array));
    for (auto const& to : it.second.getTo()) {
      builder.add(VPackValue(to));
    }
    builder.close();  // to
    builder.close();  // obj
  }
  builder.add(edgeDefinition);
  builder.close();  // array
  builder.close();  // object

  {
    SingleCollectionTransaction trx(ctx(), GraphOperations::_graphs,
                                    AccessMode::Type::WRITE);
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

    res = trx.begin();

    if (!res.ok()) {
      return OperationResult(res);
    }

    result = trx.update(GraphOperations::_graphs, builder.slice(), options);

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
ResultT<std::pair<OperationResult, Result>> GraphOperations::getVertex(
    std::string const& collectionName, std::string const& key,
    boost::optional<TRI_voc_rid_t> rev) {
  return getDocument(collectionName, key, std::move(rev));
};

// TODO check if definitionName is an edge collection in _graph?
ResultT<std::pair<OperationResult, Result>> GraphOperations::getEdge(
    const std::string& definitionName, const std::string& key,
    boost::optional<TRI_voc_rid_t> rev) {
  return getDocument(definitionName, key, std::move(rev));
}

ResultT<std::pair<OperationResult, Result>> GraphOperations::getDocument(
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
    return res;
  }

  OperationResult result = trx.document(collectionName, search, options);

  res = trx.finish(result.result);

  return std::make_pair(std::move(result), std::move(res));
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

ResultT<std::pair<OperationResult, Result>> GraphOperations::removeGraph(
    bool waitForSync, bool dropCollections) {
  std::vector<std::string> trxCollections;
  std::vector<std::string> writeCollections;
  writeCollections.emplace_back(GraphOperations::_graphs);

  std::vector<std::string> collectionsToBeRemoved;
  if (dropCollections) {
    for (auto const& vertexCollection : _graph.vertexCollections()) {
      pushCollectionIfMayBeDropped(vertexCollection, _graph.name(),
                                   collectionsToBeRemoved);
    }
    for (auto const& orphanCollection : _graph.orphanCollections()) {
      pushCollectionIfMayBeDropped(orphanCollection, _graph.name(),
                                   collectionsToBeRemoved);
    }
    for (auto const& edgeCollection : _graph.edgeCollections()) {
      pushCollectionIfMayBeDropped(edgeCollection, _graph.name(),
                                   collectionsToBeRemoved);
    }
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add(StaticStrings::KeyString, VPackValue(_graph.name()));
  }

  OperationOptions options;
  options.waitForSync = waitForSync;

  transaction::Options trxOptions;
  trxOptions.waitForSync = waitForSync;

  Result res;
  OperationResult result;
  {
    std::unique_ptr<transaction::Methods> trx(new UserTransaction(
        ctx(), trxCollections, writeCollections, {}, trxOptions));

    res = trx->begin();
    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                     "not found");
    }
    VPackSlice search = builder.slice();
    result = trx->remove(GraphOperations::_graphs, search, options);

    res = trx->finish(result.result);
    if (result.fail()) {
      THROW_ARANGO_EXCEPTION(result.result);
    }
  }
  // remove graph related collections
  // we are not able to do this in a transaction, so doing it afterwards
  if (dropCollections) {
    for (auto const& collection : collectionsToBeRemoved) {
      Result resIn;
      Result found = methods::Collections::lookup(
          &ctx()->vocbase(), collection, [&](LogicalCollection* coll) {
            resIn = methods::Collections::drop(&ctx()->vocbase(), coll, false,
                                               -1.0);
          });

      if (found.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      } else if (resIn.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }
  }

  return std::make_pair(std::move(result), std::move(res));
}

ResultT<std::pair<OperationResult, Result>> GraphOperations::removeEdge(
    const std::string& definitionName, const std::string& key,
    boost::optional<TRI_voc_rid_t> rev, bool waitForSync, bool returnOld) {
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
    return res;
  }

  OperationResult result = trx.remove(definitionName, search, options);

  res = trx.finish(result.result);

  return std::make_pair(std::move(result), std::move(res));
}

ResultT<std::pair<OperationResult, Result>> GraphOperations::modifyDocument(
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
    return res;
  }

  OperationResult result;

  if (isPatch) {
    result = trx.update(collectionName, document, options);
  } else {
    result = trx.replace(collectionName, document, options);
  }

  res = trx.finish(result.result);

  return std::make_pair(std::move(result), std::move(res));
}

ResultT<std::pair<OperationResult, Result>> GraphOperations::createDocument(
    transaction::Methods* trx, std::string const& collectionName,
    VPackSlice document, bool waitForSync, bool returnNew) {
  OperationOptions options;
  options.waitForSync = waitForSync;
  options.returnNew = returnNew;

  OperationResult result;
  result = trx->insert(collectionName, document, options);

  if (!result.ok()) {
    trx->finish(result.result);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                   "not found");
  }
  Result res = trx->finish(result.result);

  return std::make_pair(std::move(result), std::move(res));
}

ResultT<std::pair<OperationResult, Result>> GraphOperations::updateEdge(
    const std::string& definitionName, const std::string& key,
    VPackSlice document, boost::optional<TRI_voc_rid_t> rev, bool waitForSync,
    bool returnOld, bool returnNew, bool keepNull) {
  return modifyDocument(definitionName, key, document, true, std::move(rev),
                        waitForSync, returnOld, returnNew, keepNull);
}

ResultT<std::pair<OperationResult, Result>> GraphOperations::replaceEdge(
    const std::string& definitionName, const std::string& key,
    VPackSlice document, boost::optional<TRI_voc_rid_t> rev, bool waitForSync,
    bool returnOld, bool returnNew, bool keepNull) {
  return modifyDocument(definitionName, key, document, false, std::move(rev),
                        waitForSync, returnOld, returnNew, keepNull);
}

ResultT<std::pair<OperationResult, Result>> GraphOperations::createEdge(
    const std::string& definitionName, VPackSlice document, bool waitForSync,
    bool returnNew) {

  VPackSlice fromStringSlice = document.get(StaticStrings::FromString);
  VPackSlice toStringSlice = document.get(StaticStrings::ToString);

  if (fromStringSlice.isNone() || toStringSlice.isNone()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE,
                                   "not found");
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
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE,
                                   "not found");
  }

  pos = toString.find('/');
  std::string toCollectionName;
  std::string toCollectionKey;
  if (pos != std::string::npos) {
    toCollectionName = toString.substr(0, pos);
    toCollectionKey = toString.substr(pos + 1, toString.length());
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE,
                                   "not found");
  }

  // check if vertex collections are part of the graph definition
  auto it = _graph.vertexCollections().find(fromCollectionName);
  if (it == _graph.vertexCollections().end()) {
    // not found from vertex
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                   "not found");
  }
  it = _graph.vertexCollections().find(toCollectionName);
  if (it == _graph.vertexCollections().end()) {
    // not found to vertex
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                   "not found");
  }

  OperationOptions options;
  VPackSlice search;

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
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                   "not found");
  }
  OperationResult resultFrom =
      trx->document(fromCollectionName, bF.slice(), options);
  OperationResult resultTo =
      trx->document(toCollectionName, bT.slice(), options);

  if (!resultFrom.ok()) {
    trx->finish(resultFrom.result);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                   "not found");
  }
  if (!resultTo.ok()) {
    trx->finish(resultTo.result);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                   "not found");
  }
  return createDocument(trx.get(), definitionName, document, waitForSync,
                        returnNew);
}

ResultT<std::pair<OperationResult, Result>> GraphOperations::updateVertex(
    const std::string& collectionName, const std::string& key,
    VPackSlice document, boost::optional<TRI_voc_rid_t> rev, bool waitForSync,
    bool returnOld, bool returnNew, bool keepNull) {
  return modifyDocument(collectionName, key, document, true, std::move(rev),
                        waitForSync, returnOld, returnNew, keepNull);
}

ResultT<std::pair<OperationResult, Result>> GraphOperations::replaceVertex(
    const std::string& collectionName, const std::string& key,
    VPackSlice document, boost::optional<TRI_voc_rid_t> rev, bool waitForSync,
    bool returnOld, bool returnNew, bool keepNull) {
  return modifyDocument(collectionName, key, document, false, std::move(rev),
                        waitForSync, returnOld, returnNew, keepNull);
}

ResultT<std::pair<OperationResult, Result>> GraphOperations::createVertex(
    const std::string& collectionName, VPackSlice document, bool waitForSync,
    bool returnNew) {
  transaction::Options trxOptions;

  std::vector<std::string> writeCollections;
  writeCollections.emplace_back(collectionName);
  std::unique_ptr<transaction::Methods> trx(
      new UserTransaction(ctx(), {}, writeCollections, {}, trxOptions));

  Result res = trx->begin();

  if (!res.ok()) {
    return res;
  }

  return createDocument(trx.get(), collectionName, document, waitForSync,
                        returnNew);
}

ResultT<std::pair<OperationResult, Result>> GraphOperations::removeVertex(
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
  UserTransaction trx{ctx(), {}, trxCollections, {}, trxOptions};

  Result res = trx.begin();

  if (!res.ok()) {
    return res;
  }

  OperationResult result = trx.remove(collectionName, search, options);

  {
    aql::QueryString const queryString =
        aql::QueryString{"FOR e IN @@collection "
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

      arangodb::aql::Query query(true, ctx()->vocbase(), queryString, bindVars,
                                 nullptr, arangodb::aql::PART_DEPENDENT);

      auto queryResult = query.execute(QueryRegistryFeature::QUERY_REGISTRY);

      if (queryResult.code != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
      }
    }
  }

  // TODO result.result is not the only result that counts here: must abort
  // if any AQL query fails, too.
  res = trx.finish(result.result);

  return std::make_pair(std::move(result), std::move(res));
}

void GraphOperations::pushCollectionIfMayBeDropped(
    const std::string& colName, const std::string& graphName,
    std::vector<std::string>& toBeRemoved) {
  GraphManager gmngr{ctx()};
  VPackBuilder graphsBuilder;
  gmngr.readGraphs(graphsBuilder, arangodb::aql::PART_DEPENDENT);
  VPackSlice graphs = graphsBuilder.slice();
  bool collectionUnused = true;

  TRI_ASSERT(graphs.get("graphs").isArray());

  if (!graphs.get("graphs").isArray()) {
    return;
  }

  for (auto graph : VPackArrayIterator(graphs.get("graphs"))) {
    graph = graph.resolveExternals();
    if (!collectionUnused) {
      // Short circuit
      break;
    }
    if (graph.get("_key").copyString() == graphName) {
      break;
    }

    // check edge definitions
    VPackSlice edgeDefinitions = graph.get(GraphOperations::_attrEdgeDefs);
    if (edgeDefinitions.isArray()) {
      for (auto const& edgeDefinition : VPackArrayIterator(edgeDefinitions)) {
        std::string from =
            edgeDefinition.get(StaticStrings::FromString).copyString();
        std::string to =
            edgeDefinition.get(StaticStrings::ToString).copyString();
        std::string collection = edgeDefinition.get("collection").copyString();

        if (collection == colName || from.find(colName) || to.find(colName)) {
          collectionUnused = false;
        }
      }
    }

    // check orphan collections
    VPackSlice orphanCollections = graph.get(GraphOperations::_attrOrphans);
    if (orphanCollections.isArray()) {
      for (auto const& orphanCollection :
           VPackArrayIterator(orphanCollections)) {
        if (orphanCollection.copyString().find(colName)) {
          collectionUnused = false;
        }
      }
    }
  }

  if (collectionUnused) {
    toBeRemoved.emplace_back(colName);
  }
}
