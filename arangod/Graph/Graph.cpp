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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "Graph.h"

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
#include "RestServer/QueryRegistryFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/Methods.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/Graphs.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

using namespace arangodb;
using namespace arangodb::graph;
using UserTransaction = transaction::Methods;

std::string const Graph::_graphs = "_graphs";
char const* Graph::_attrDropCollections = "dropCollections";
char const* Graph::_attrEdgeDefs = "edgeDefinitions";
char const* Graph::_attrOrphans = "orphanCollections";
char const* Graph::_attrIsSmart = "isSmart";
char const* Graph::_attrNumberOfShards = "numberOfShards";
char const* Graph::_attrReplicationFactor = "replicationFactor";
char const* Graph::_attrSmartGraphAttribute = "smartGraphAttribute";

void Graph::insertVertexCollections(VPackSlice const arr) {
  TRI_ASSERT(arr.isArray());
  for (auto const& c : VPackArrayIterator(arr)) {
    TRI_ASSERT(c.isString());
    addVertexCollection(c.copyString());
  }
}

void Graph::insertOrphanCollections(VPackSlice const arr) {
  TRI_ASSERT(arr.isArray());
  for (auto const& c : VPackArrayIterator(arr)) {
    TRI_ASSERT(c.isString());
    addOrphanCollection(c.copyString());
  }
}

std::unordered_set<std::string> const& Graph::vertexCollections() const {
  return _vertexColls;
}

std::unordered_set<std::string> const& Graph::orphanCollections() const {
  return _orphanColls;
}

std::unordered_set<std::string> const& Graph::edgeCollections() const {
  return _edgeColls;
}

std::vector<std::string> const& Graph::edgeDefinitionNames() const {
  return _edgeDefsNames;
}

std::unordered_map<std::string, EdgeDefinition> const& Graph::edgeDefinitions() const {
  return _edgeDefs;
}

bool const& Graph::isSmart() const {
  return _isSmart;
}

uint64_t Graph::numberOfShards() const {
  return _numberOfShards;
}

uint64_t Graph::replicationFactor() const {
  return _replicationFactor;
}

std::string const& Graph::smartGraphAttribute() const {
  return _smartGraphAttribute;
}

std::string const& Graph::id() const {
  return _id;
}

std::string const& Graph::rev() const {
  return _rev;
}

void Graph::addEdgeCollection(std::string&& name) {
  _edgeColls.emplace(std::move(name));
}

void Graph::addVertexCollection(std::string&& name) {
  _vertexColls.emplace(std::move(name));
}

void Graph::addOrphanCollection(std::string&& name) {
  _orphanColls.emplace(std::move(name));
}

void Graph::setSmartState(bool state) {
  _isSmart = state;
}

void Graph::setNumberOfShards(uint64_t numberOfShards) {
  _numberOfShards = numberOfShards;
}

void Graph::setReplicationFactor(uint64_t replicationFactor) {
  _replicationFactor = replicationFactor;
}

void Graph::setSmartGraphAttribute(std::string&& smartGraphAttribute) {
  _smartGraphAttribute = std::move(smartGraphAttribute);
}

void Graph::setId(std::string&& id) {
  _id = std::move(id);
}

void Graph::setRev(std::string&& rev) {
  _rev = std::move(rev);
}

void Graph::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);

  if (!_vertexColls.empty()) {
    builder.add(VPackValue("vertexCollectionNames"));
    VPackArrayBuilder guard2(&builder);
    for (auto const& cn : _vertexColls) {
      builder.add(VPackValue(cn));
    }
  }

  if (!_edgeColls.empty()) {
    builder.add(VPackValue("edgeCollectionNames"));
    VPackArrayBuilder guard2(&builder);
    for (auto const& cn : _edgeColls) {
      builder.add(VPackValue(cn));
    }
  }
}

Graph::Graph(std::string&& graphName_, velocypack::Slice const& slice)
    : _graphName(graphName_), _vertexColls(), _edgeColls() {

  if (slice.hasKey(_attrEdgeDefs)) {
    VPackSlice edgeDefs = slice.get(_attrEdgeDefs);
    TRI_ASSERT(edgeDefs.isArray());
    if (!edgeDefs.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_GRAPH_INVALID_GRAPH,
          "'edgeDefinitions' are not an array in the graph definition");
    }

    for (auto const& def : VPackArrayIterator(edgeDefs)) {
      Result res = addEdgeDefinition(def);
      if (res.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      }
      // TODO maybe the following code can be simplified using _edgeDefs
      // and/or after def is already validated.
      // Or, this can be done during addEdgeDefinition as well.
      TRI_ASSERT(def.isObject());
      try {
        std::string eCol =
            basics::VelocyPackHelper::getStringValue(def, "collection", "");
        addEdgeCollection(std::move(eCol));
      } catch (...) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_GRAPH_INVALID_GRAPH,
            "didn't find 'collection' in the graph definition");
      }
      // TODO what if graph is not in a valid format any more
      try {
        VPackSlice tmp = def.get("from");
        insertVertexCollections(tmp);
      } catch (...) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_GRAPH_INVALID_GRAPH,
            "didn't find from-collection in the graph definition");
      }
      try {
        VPackSlice tmp = def.get("to");
        insertVertexCollections(tmp);
      } catch (...) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_GRAPH_INVALID_GRAPH,
            "didn't find to-collection in the graph definition");
      }
    }
  }
  if (slice.hasKey(_attrOrphans)) {
    auto orphans = slice.get(_attrOrphans);
    insertVertexCollections(orphans);
    insertOrphanCollections(orphans);
  }
  if (slice.hasKey(_attrIsSmart)) {
    setSmartState(slice.get(_attrIsSmart).getBool());
  }
  if (slice.hasKey(_attrNumberOfShards)) {
    setNumberOfShards(slice.get(_attrNumberOfShards).getUInt());
  }
  if (slice.hasKey(_attrReplicationFactor)) {
    setReplicationFactor(slice.get(_attrReplicationFactor).getUInt());
  }
  if (slice.hasKey(_attrSmartGraphAttribute)) {
    setSmartGraphAttribute(slice.get(_attrSmartGraphAttribute).copyString());
  }
  setId("_graphs/" + graphName_); // TODO: how to fetch id properly?
  setRev(slice.get(StaticStrings::RevString).copyString());
}

void Graph::enhanceEngineInfo(VPackBuilder&) const {}

// validates the type:
// edgeDefinition : { collection : string, from : [string], to : [string] }
Result Graph::ValidateEdgeDefinition(VPackSlice const& edgeDefinition) {
  TRI_ASSERT(edgeDefinition.isObject());
  if (!edgeDefinition.isObject()) {
    return Result(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT,
                  "edge definition is not an object!");
  }

  for (auto const& key :
       std::array<std::string, 3>{{"collection", "from", "to"}}) {
    if (!edgeDefinition.hasKey(key)) {
      return Result(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT,
                    "Attribute '" + key + "' missing in edge definition!");
    }
  }

  if (!edgeDefinition.get("collection").isString()) {
    return Result(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT,
                  "edge definition is not a string!");
  }

  for (auto const& key : std::array<std::string, 2>{{"from", "to"}}) {
    if (!edgeDefinition.get(key).isArray()) {
      return Result(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT,
                    "Edge definition '" + key + "' is not an array!");
    }

    for (auto const& it : VPackArrayIterator(edgeDefinition.get(key))) {
      if (!it.isString()) {
        return Result(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT,
                      std::string("Edge definition '") + key +
                          "' does not only contain strings!");
      }
    }
  }

  return Result();
}

// TODO: maybe create a class instance here + func as class func
// sort an edgeDefinition:
// edgeDefinition : { collection : string, from : [string], to : [string] }
std::shared_ptr<velocypack::Buffer<uint8_t>> Graph::SortEdgeDefinition(VPackSlice const& edgeDefinition) {
  arangodb::basics::VelocyPackHelper::VPackLess<true> sorter;
  VPackBuilder from = VPackCollection::sort(edgeDefinition.get("from"), sorter);
  VPackBuilder to = VPackCollection::sort(edgeDefinition.get("to"), sorter);
  VPackBuilder sortedBuilder;
  sortedBuilder.openObject();
  sortedBuilder.add("collection", edgeDefinition.get("collection"));
  sortedBuilder.add("from", from.slice());
  sortedBuilder.add("to", to.slice());
  sortedBuilder.close();

  return sortedBuilder.steal();
}

// validates the type:
// orphanDefinition : string <collectionName>
Result Graph::ValidateOrphanCollection(VPackSlice const& orphanCollection) {
  if (!orphanCollection.isString()) {
    return Result(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT,
                  "orphan collection is not a string!");
  }
  return Result();
}

Result Graph::addEdgeDefinition(VPackSlice const& edgeDefinition) {
  Result res = ValidateEdgeDefinition(edgeDefinition);
  // should always be valid unless someone broke his _graphs collection
  // manually
  TRI_ASSERT(res.ok());
  if (res.fail()) {
    return res;
  }
  std::string collection = edgeDefinition.get("collection").copyString();
  _edgeDefsNames.emplace_back(collection);
  VPackSlice from = edgeDefinition.get("from");
  VPackSlice to = edgeDefinition.get("to");

  std::unordered_set<std::string> fromSet;
  std::unordered_set<std::string> toSet;

  // duplicates in from and to shouldn't occur, but are safely ignored here
  for (auto const& it : VPackArrayIterator(from)) {
    fromSet.emplace(it.copyString());
  }
  for (auto const& it : VPackArrayIterator(to)) {
    toSet.emplace(it.copyString());
  }

  bool inserted;
  std::tie(std::ignore, inserted) = _edgeDefs.emplace(
      collection,
      EdgeDefinition{collection, std::move(fromSet), std::move(toSet)});

  // This can only happen if addEdgeDefinition is called without clearing
  // _edgeDefs first (which would be a logical error), or if the same collection
  // is defined multiple times (which the user should not be allowed to do).
  TRI_ASSERT(inserted);
  if (!inserted) {
    return Result(TRI_ERROR_GRAPH_INTERNAL_EDGE_COLLECTION_ALREADY_SET,
                  "Collection '" + collection + "' already added!");
  }

  return Result();
}

std::ostream& Graph::operator<<(std::ostream& ostream) {
  ostream << "Graph \"" << name() << "\" {\n";
  for (auto const& it : _edgeDefs) {
    EdgeDefinition const& def = it.second;
    ostream << "  collection \"" << def.getName() << "\" {\n";
    ostream << "    from [";
    bool first = true;
    for (auto const& from : def.getFrom()) {
      if (!first) {
        ostream << ", ";
      }
      first = false;
      ostream << from;
    }
    ostream << "    to [";
    first = true;
    for (auto const& to : def.getTo()) {
      if (!first) {
        ostream << ", ";
      }
      first = false;
      ostream << to;
    }
    ostream << "  }\n";
  }
  ostream << "}";

  return ostream;
}

bool Graph::hasEdgeCollection(std::string const &collectionName) const {
  for (auto const& edgeCollection : edgeCollections()) {
    if (collectionName == edgeCollection) {
      return true;
    }
  }

  return false;
}

// edges

OperationResult GraphOperations::changeEdgeDefinitionsForGraph(
    VPackSlice graph, VPackSlice edgeDefinition,
    std::unordered_set<std::string> newCollections, std::unordered_set<std::string> possibleOrphans,
    bool waitForSync, std::string thisGraphName, transaction::Methods& trx) {

  std::unordered_set<std::string> graphCollections;
  std::string edgeDefinitionName = edgeDefinition.get("collection").copyString();
  bool gotAHit = false;

  VPackSlice eDs;

  if (graph.get(Graph::_attrEdgeDefs).isArray()) {
    eDs = graph.get(Graph::_attrEdgeDefs);
  }

  OperationResult result;

  OperationOptions options;
  options.waitForSync = waitForSync;

  std::unordered_map<std::string, EdgeDefinition> edgeDefs = _graph.edgeDefinitions();

  // replace edgeDefinition
  for (auto const& eD : VPackArrayIterator(eDs)) {
    if (eD.get("collection").copyString() == edgeDefinitionName) {
      gotAHit = true;
      // now we have to build a new VPackObject for updating the edgeDefinition
      VPackBuilder builder;
      builder.openObject();

      // build edge definitions start
      builder.add(StaticStrings::KeyString, VPackValue(graph.get(StaticStrings::KeyString).copyString()));
      builder.add(Graph::_attrEdgeDefs, VPackValue(VPackValueType::Array));
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
          builder.close(); // from
        }
        if (it.second.getName() == edgeDefinitionName) {
          builder.add("to", edgeDefinition.get("to"));
        } else {
          // to
          builder.add("to", VPackValue(VPackValueType::Array));
          for (auto const& to : it.second.getTo()) {
            builder.add(VPackValue(to));
          }
          builder.close(); // to
        }
        builder.close(); // obj
      }
      builder.close(); // array
      // build edge definitions end

      builder.close(); // object

      // now write to database
      result = trx.update(Graph::_graphs, builder.slice(), options);
    } else {
      // collect all used collections
      for (auto const& from: VPackArrayIterator(eD.get("from"))) {
        graphCollections.emplace(from.copyString());
      }
      for (auto const& to: VPackArrayIterator(eD.get("to"))) {
        graphCollections.emplace(to.copyString());
      }
    }
  }

  if (!gotAHit) {
    return result;
  }

  GraphManager gmngr{ctx()};
  // collection creation
  for (auto const& nc : newCollections) {
    gmngr.removeVertexCollection(nc, false);
  }
  for (auto const& po : possibleOrphans) {
    gmngr.createVertexCollection(po);
  }

  return result;
}

ResultT<std::pair<OperationResult, Result>> GraphOperations::eraseEdgeDefinition(
  bool waitForSync, std::string edgeDefinitionName, bool dropCollection
) {

  // check if edgeCollection is available
  checkEdgeCollectionAvailability(edgeDefinitionName);

  std::unordered_set<std::string> possibleOrphans;
  std::unordered_set<std::string> usedVertexCollections;
  std::unordered_map<std::string, EdgeDefinition> edgeDefs = _graph.edgeDefinitions();

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
      newEdgeDefs.add("collection", VPackValue(edgeDefinition.second.getName()));
      newEdgeDefs.add("from", VPackValue(VPackValueType::Array));
      for (auto const& from : edgeDefinition.second.getFrom()) {
        newEdgeDefs.add(VPackValue(from));
      }
      newEdgeDefs.close(); // array
      newEdgeDefs.add("to", VPackValue(VPackValueType::Array));
      for (auto const& to : edgeDefinition.second.getTo()) {
        newEdgeDefs.add(VPackValue(to));
      }
      newEdgeDefs.close(); // array
      newEdgeDefs.close(); // object
    }
  }

  newEdgeDefs.close(); // array

  // build orphan array
  VPackBuilder newOrphColls;
  newOrphColls.add(VPackValue(VPackValueType::Array));
  for (auto const& orph : _graph.orphanCollections()) {
    newOrphColls.add(VPackValue(orph));
  }
  for (auto const& po : possibleOrphans) {
    if (std::find(usedVertexCollections.begin(), usedVertexCollections.end(), po) != usedVertexCollections.end()) {
      newOrphColls.add(VPackValue(po));
    }
  }
  newOrphColls.close(); // array

  // remove edgeDefinition from graph config

  OperationOptions options;
  options.waitForSync = waitForSync;

  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(_graph.name()));
  builder.add(_graph._attrEdgeDefs, newEdgeDefs.slice());
  builder.add(_graph._attrOrphans, newOrphColls.slice());
  builder.close();

  SingleCollectionTransaction trx(ctx(), Graph::_graphs,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    return res;
  }
  OperationResult result = trx.update(Graph::_graphs, builder.slice(), options);
  res = trx.finish(result.result);

  if (dropCollection) {
    // TODO: also drop collection if not used
    // old gharial api is dropping without a check, docu says we need to check if other graphs are using
    // the collection. what should we do?! 
  }

  return std::make_pair(std::move(result), std::move(res));
}

void GraphOperations::checkEdgeCollectionAvailability(std::string edgeDefinitionName) {
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

void GraphOperations::checkVertexCollectionAvailability(std::string vertexDefinitionName) {
  GraphManager gmngr{ctx()};

  std::shared_ptr<LogicalCollection> def = gmngr.getCollectionByName(
    ctx()->vocbase(), vertexDefinitionName
  );
  if (def == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST);
  }
}

ResultT<std::pair<OperationResult, Result>> GraphOperations::editEdgeDefinition(
    VPackSlice edgeDefinition, bool waitForSync, std::string edgeDefinitionName) {

  std::shared_ptr<velocypack::Buffer<uint8_t>> buffer = Graph::SortEdgeDefinition(edgeDefinition);
  edgeDefinition = velocypack::Slice(buffer->data());

  // check if edgeCollection is available
  checkEdgeCollectionAvailability(edgeDefinitionName);

  VPackBuilder builder;
  builder.add(VPackValue(VPackValueType::Array));
  builder.add(edgeDefinition);
  builder.close();

  GraphManager gmngr{ctx()};
  gmngr.findOrCreateCollectionsByEdgeDefinitions(builder.slice());

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
    THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_EDGE_COLLECTION_NOT_USED);
  }

  std::unordered_map<std::string, EdgeDefinition> edgeDefs = _graph.edgeDefinitions();
  EdgeDefinition const& currentEdgeDefinition = edgeDefs.at(currentEdgeDefinitionName);

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
    if (std::find(newCollections.begin(), newCollections.end(), colName) == newCollections.end()) {
      possibleOrphans.emplace(colName);
    }
  }
  // change definition for ALL graphs
  VPackBuilder graphsBuilder;
  gmngr.readGraphs(graphsBuilder, arangodb::aql::PART_DEPENDENT);
  VPackSlice graphs = graphsBuilder.slice();

  SingleCollectionTransaction trx(ctx(), Graph::_graphs,
      AccessMode::Type::WRITE);

  Result res = trx.begin();

  if (!res.ok()) {
    trx.finish(TRI_ERROR_NO_ERROR);
    THROW_ARANGO_EXCEPTION(res);
  }

  OperationResult result;
  if (graphs.get("graphs").isArray()) {
    for (auto singleGraph : VPackArrayIterator(graphs.get("graphs"))) {
      result = changeEdgeDefinitionsForGraph(singleGraph.resolveExternals(), edgeDefinition,
              newCollections, possibleOrphans, waitForSync, _graph.name(), trx);
    }
  }

  res = trx.finish(TRI_ERROR_NO_ERROR);

  return std::make_pair(std::move(result), std::move(res));
};

ResultT<std::pair<OperationResult, Result>>
GraphOperations::addOrphanCollection(VPackSlice document, bool waitForSync) {
  GraphManager gmngr{ctx()};
  std::string collectionName = document.get("collection").copyString();
  std::shared_ptr<LogicalCollection> def;

  def = gmngr.getCollectionByName(ctx()->vocbase(), collectionName);
  if (def == nullptr) {
    gmngr.createVertexCollection(collectionName);
  } else {
    if (def->type() != 2) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX);
    }
  }

  for (auto const& vertexCollection : _graph.vertexCollections()) {
    if (collectionName == vertexCollection) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF);
    }
  }
  for (auto const& orphanCollection : _graph.orphanCollections()) {
    if (collectionName == orphanCollection) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS);
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
  builder.close(); // array
  builder.close(); // object

  SingleCollectionTransaction trx(ctx(), Graph::_graphs,
                                  AccessMode::Type::WRITE);

  Result res = trx.begin();

  if (!res.ok()) {
    trx.finish(TRI_ERROR_NO_ERROR);
    THROW_ARANGO_EXCEPTION(res);
  }

  OperationOptions options;
  options.waitForSync = waitForSync;
  OperationResult result = trx.update(Graph::_graphs, builder.slice(), options);

  res = trx.finish(result.result);
  return std::make_pair(std::move(result), std::move(res));
}

ResultT<std::pair<OperationResult, Result>> GraphOperations::eraseOrphanCollection(
  bool waitForSync, std::string collectionName, bool dropCollection
) {

  // check if collection exists
  checkVertexCollectionAvailability(collectionName);

  std::unordered_set<std::string> orphanCollections = _graph.orphanCollections();

  bool found = false;
  for (auto const& oName : _graph.orphanCollections()) {
    if (oName == collectionName) {
      found = true;
    }
  }
  if (!found) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION);
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
  builder.close(); // array
  builder.close(); // object

  SingleCollectionTransaction trx(ctx(), Graph::_graphs,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    return res;
  }
  OperationOptions options;
  options.waitForSync = waitForSync;

  OperationResult result = trx.update(Graph::_graphs, builder.slice(), options);
  res = trx.finish(result.result);

  if (dropCollection) {
    std::vector<std::string> collectionsToBeRemoved;
    pushCollectionIfMayBeDropped(collectionName, "", collectionsToBeRemoved);

    for (auto const& collection : collectionsToBeRemoved) {
      Result resIn;
      Result found = methods::Collections::lookup(
              &ctx()->vocbase(),
              collection,
              [&](LogicalCollection* coll) {
                  resIn = methods::Collections::drop(&ctx()->vocbase(), coll, false, -1.0);
              }
      );

      if (found.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      } else if (resIn.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }
  }

  return std::make_pair(std::move(result), std::move(res));
}

ResultT<std::pair<OperationResult, Result>> GraphOperations::addEdgeDefinition(
  VPackSlice edgeDefinition, bool waitForSync
) {
  Result res = Graph::ValidateEdgeDefinition(edgeDefinition);

  if (res.fail()) {
    return {res};
  }

  OperationOptions options;
  options.waitForSync = waitForSync;

  std::shared_ptr<velocypack::Buffer<uint8_t>> buffer =
      Graph::SortEdgeDefinition(edgeDefinition);
  edgeDefinition = velocypack::Slice(buffer->data());

  std::string eC = edgeDefinition.get("collection").copyString();
  if (_graph.hasEdgeCollection(eC)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_COLLECTION_MULTI_USE);
  }

  // ... in different graph
  GraphManager gmngr{ctx()};

  res = gmngr.checkForEdgeDefinitionConflicts(eC, edgeDefinition);
  if (res.fail()) {
    return {res};
  }

  VPackBuilder builder;
  builder.add(VPackValue(VPackValueType::Array));
  builder.add(edgeDefinition);
  builder.close();

  gmngr.findOrCreateCollectionsByEdgeDefinitions(builder.slice());

  std::unordered_map<std::string, EdgeDefinition> edgeDefs =
      _graph.edgeDefinitions();
  // build the updated document, reuse the builder
  builder.clear();
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(_graph.name()));
  builder.add(Graph::_attrEdgeDefs, VPackValue(VPackValueType::Array));
  for (auto const& it : edgeDefs) {
    builder.add(VPackValue(VPackValueType::Object));
    builder.add("collection", VPackValue(it.second.getName()));
    // from
    builder.add("from", VPackValue(VPackValueType::Array));
    for (auto const& from : it.second.getFrom()) {
      builder.add(VPackValue(from));
    }
    builder.close(); // from
    // to
    builder.add("to", VPackValue(VPackValueType::Array));
    for (auto const& to : it.second.getTo()) {
      builder.add(VPackValue(to));
    }
    builder.close(); // to
    builder.close(); // obj
  }
  builder.add(edgeDefinition);
  builder.close(); // array
  builder.close(); // object

  OperationResult result;
  {
    SingleCollectionTransaction trx(ctx(), Graph::_graphs,
      AccessMode::Type::WRITE);
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

    res = trx.begin();

    if (!res.ok()) {
      return res;
    }

    result = trx.update(Graph::_graphs, builder.slice(), options);

    res = trx.finish(result.result);
  }

  return std::make_pair(std::move(result), std::move(res));
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
  writeCollections.emplace_back(Graph::_graphs);

  std::vector<std::string> collectionsToBeRemoved;
  if (dropCollections) {
    for (auto const& vertexCollection : _graph.vertexCollections()) {
      pushCollectionIfMayBeDropped(
        vertexCollection, _graph.name(), collectionsToBeRemoved
      );
    }
    for (auto const& orphanCollection : _graph.orphanCollections()) {
      pushCollectionIfMayBeDropped(
        orphanCollection, _graph.name(), collectionsToBeRemoved
      );
    }
    for (auto const& edgeCollection : _graph.edgeCollections()) {
      pushCollectionIfMayBeDropped(
        edgeCollection, _graph.name(), collectionsToBeRemoved
      );
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
    std::unique_ptr<transaction::Methods> trx(
      new UserTransaction(ctx(), trxCollections, writeCollections, {}, trxOptions));

    res = trx->begin();
    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, "not found");
    }
    VPackSlice search = builder.slice();
    result = trx->remove(Graph::_graphs, search, options);

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
        &ctx()->vocbase(),
        collection,
        [&](LogicalCollection* coll) {
          resIn = methods::Collections::drop(&ctx()->vocbase(), coll, false, -1.0);
        }
      );

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
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, "not found");
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
    const std::string& definitionName,
    VPackSlice document, bool waitForSync, bool returnNew) {

  // TODO create edge only if vertices id's are existent
  auto const& vertexCollections = _graph.vertexCollections();

  VPackSlice fromStringSlice = document.get(StaticStrings::FromString);
  VPackSlice toStringSlice = document.get(StaticStrings::ToString);

  if (fromStringSlice.isNone() || toStringSlice.isNone()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, "not found");
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
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, "not found");
  }

  pos = toString.find('/');
  std::string toCollectionName;
  std::string toCollectionKey;
  if (pos != std::string::npos) {
    toCollectionName = toString.substr(0, pos);
    toCollectionKey = toString.substr(pos + 1, toString.length());
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, "not found");
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

  std::vector<std::string> trxCollections;
  std::vector<std::string> writeCollections;
  trxCollections.emplace_back(fromCollectionName);
  trxCollections.emplace_back(toCollectionName);
  writeCollections.emplace_back(definitionName);

  transaction::Options trxOptions;
  trxOptions.waitForSync = waitForSync;

  std::unique_ptr<transaction::Methods> trx(
    new UserTransaction(ctx(), trxCollections, writeCollections, {}, trxOptions));

  Result res = trx->begin();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, "not found");
  }
  OperationResult resultFrom = trx->document(fromCollectionName, bF.slice(), options);
  OperationResult resultTo = trx->document(toCollectionName, bT.slice(), options);

  if (!resultFrom.ok()) {
    trx->finish(resultFrom.result);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, "not found");
  }
  if (!resultTo.ok()) {
    trx->finish(resultTo.result);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, "not found");
  }
  return createDocument(trx.get(), definitionName, document, waitForSync, returnNew);
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
    const std::string& collectionName,
    VPackSlice document, bool waitForSync, bool returnNew) {

    transaction::Options trxOptions;

    std::vector<std::string> writeCollections;
    writeCollections.emplace_back(collectionName);
    std::unique_ptr<transaction::Methods> trx(
      new UserTransaction(ctx(), {}, writeCollections, {}, trxOptions));

    Result res = trx->begin();

    if (!res.ok()) {
      return res;
    }

  return createDocument(trx.get(), collectionName, document, waitForSync, returnNew);
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
        aql::QueryString({"FOR e IN @@collection "
                          "FILTER e._from == @vertexId "
                          "OR e._to == @vertexId "
                          "REMOVE e IN @@collection"});

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
    VPackSlice edgeDefinitions = graph.get(Graph::_attrEdgeDefs);
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
    VPackSlice orphanCollections = graph.get(Graph::_attrOrphans);
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

void Graph::graphToVpack(VPackBuilder &builder) const {
  builder.add(VPackValue(VPackValueType::Object));
  builder.add("graph", VPackValue(VPackValueType::Object));

  builder.add("name", VPackValue(name()));

  builder.add("edgeDefinitions", VPackValue(VPackValueType::Array));
  auto edgeDefs = edgeDefinitions();
  for (auto const& name : edgeDefinitionNames()) {
    EdgeDefinition const& def = edgeDefs.at(name);
    builder.add(VPackValue(VPackValueType::Object));
    builder.add("collection", VPackValue(def.getName()));
    builder.add("from", VPackValue(VPackValueType::Array));
    for (auto const& from : def.getFrom()) {
      builder.add(VPackValue(from));
    }
    builder.close(); // array from
    builder.add("to", VPackValue(VPackValueType::Array));
    for (auto const& to : def.getTo()) {
      builder.add(VPackValue(to));
    }
    builder.close(); // array to
    builder.close(); // object
  }
  builder.close(); // object edgedefs

  builder.add("orphanCollections", VPackValue(VPackValueType::Array));
  for (auto const& orphan : orphanCollections()) {
    builder.add(VPackValue(orphan));
  }
  builder.close(); // orphan array

  builder.add("isSmart", VPackValue(isSmart()));
  builder.add("numberOfShards", VPackValue(numberOfShards()));
  builder.add("replicationFactor", VPackValue(replicationFactor()));
  builder.add("smartGraphAttribute", VPackValue(smartGraphAttribute()));
  builder.add(StaticStrings::RevString, VPackValue(rev()));
  builder.add(StaticStrings::IdString, VPackValue(id()));

  builder.close(); // object
  builder.close(); // object
}

void Graph::edgesToVpack(VPackBuilder &builder) const {
  builder.add(VPackValue(VPackValueType::Object));
  builder.add("collections", VPackValue(VPackValueType::Array));

  for (auto const& edgeCollection : edgeCollections()) {
    builder.add(VPackValue(edgeCollection));
  }
  builder.close();

  builder.close();
}

void Graph::verticesToVpack(VPackBuilder &builder) const {
  builder.add(VPackValue(VPackValueType::Object));
  builder.add("collections", VPackValue(VPackValueType::Array));

  for (auto const& vertexCollection : vertexCollections()) {
    builder.add(VPackValue(vertexCollection));
  }
  builder.close();

  builder.close();
}

namespace getGraphFromCacheResult {
struct Success {
  std::shared_ptr<Graph const> graph;

  explicit Success(std::shared_ptr<Graph const> graph_)
      : graph(std::move(graph_)){};
  Success& operator=(Success const& other) = default;
  Success() = delete;
};
struct Outdated {};
struct NotFound {};
struct Exception {};
}

using GetGraphFromCacheResult = boost::variant<
    getGraphFromCacheResult::Success, getGraphFromCacheResult::Outdated,
    getGraphFromCacheResult::NotFound, getGraphFromCacheResult::Exception>;

GetGraphFromCacheResult _getGraphFromCache(GraphCache::CacheType const& _cache,
                                           std::string const& name,
                                           std::chrono::seconds maxAge) {
  using namespace getGraphFromCacheResult;

  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

  GraphCache::CacheType::const_iterator entryIt;
  bool entryFound;
  try {
    entryIt = _cache.find(name);
    entryFound = entryIt != _cache.end();
  } catch (...) {
    return Exception{};
  }

  if (!entryFound) {
    return NotFound{};
  }

  GraphCache::EntryType const& entry = entryIt->second;
  std::chrono::steady_clock::time_point const& insertedAt = entry.first;

  if (now - insertedAt > maxAge) {
    return Outdated{};
  }

  return Success{entry.second};
}

const std::shared_ptr<const Graph> GraphCache::getGraph(
    std::shared_ptr<transaction::Context> ctx, std::string const& name,
    std::chrono::seconds maxAge) {
  using namespace getGraphFromCacheResult;

  GetGraphFromCacheResult cacheResult = Exception{};

  // try to lookup the graph in the cache first
  {
    READ_LOCKER(guard, _lock);
    cacheResult = _getGraphFromCache(_cache, name, maxAge);
  }

  /*
  if (typeid(Success) == cacheResult.type()) {
    LOG_TOPIC(TRACE, Logger::GRAPHS) << "GraphCache::getGraph('" << name
                                     << "'): Found entry in cache";

    return boost::get<Success>(cacheResult).graph;
  } else if (typeid(Outdated) == cacheResult.type()) {
    LOG_TOPIC(TRACE, Logger::GRAPHS) << "GraphCache::getGraph('" << name
                                     << "'): Cached entry outdated";
  } else if (typeid(NotFound) == cacheResult.type()) {
    LOG_TOPIC(TRACE, Logger::GRAPHS) << "GraphCache::getGraph('" << name
                                     << "'): No cache entry";
  } else if (typeid(Exception) == cacheResult.type()) {
    LOG_TOPIC(ERR, Logger::GRAPHS)
        << "GraphCache::getGraph('" << name
        << "'): An exception occured during cache lookup";
  } else {
    LOG_TOPIC(FATAL, Logger::GRAPHS) << "GraphCache::getGraph('" << name
                                     << "'): Unhandled result type "
                                     << cacheResult.type().name();

    return nullptr;
  }*/

  // if the graph wasn't found in the cache, lookup the graph and insert or
  // replace the entry. if the graph doesn't exist, erase a possible entry from
  // the cache.
  std::shared_ptr<Graph const> graph;
  try {
    WRITE_LOCKER(guard, _lock);

    std::chrono::steady_clock::time_point now =
        std::chrono::steady_clock::now();

    bool graphNotFound = false;

    try {
      graph.reset(lookupGraphByName(ctx, name));
    } catch (arangodb::basics::Exception& exception) {
      switch (exception.code()) {
        case TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND:
        case TRI_ERROR_GRAPH_NOT_FOUND:
          graphNotFound = true;
        default:
            // something else went wrong
            ;
      }
    }

    if (graphNotFound) {
      _cache.erase(name);
    }

    if (graph == nullptr) {
      return nullptr;
    }

    CacheType::iterator it;
    bool insertSuccess;
    std::tie(it, insertSuccess) =
        _cache.emplace(name, std::make_pair(now, graph));

    if (!insertSuccess) {
      it->second.first = now;
      it->second.second = graph;
    }

  } catch (...) {
  };

  // graph is never set to an invalid or outdated value. So even in case of an
  // exception, if graph was set, it may be returned.
  return graph;
}

void GraphManager::removeVertexCollection(
    std::string const& collectionName, bool dropCollection) {

  Result resIn;
  OperationResult result;

  if (dropCollection) {
    Result found = methods::Collections::lookup(
      &ctx()->vocbase(),
      collectionName,
      [&](LogicalCollection* coll) {
        resIn = methods::Collections::drop(&ctx()->vocbase(), coll, false, -1.0);
      }
    );

    if (found.fail()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST);
    } else if (resIn.fail()) {
      THROW_ARANGO_EXCEPTION(resIn);
    }
  }
}


void GraphManager::createEdgeCollection(std::string const& name) {
  createCollection(name, TRI_COL_TYPE_EDGE);
}

void GraphManager::createVertexCollection(std::string const& name) {
  createCollection(name, TRI_COL_TYPE_DOCUMENT);
}

void GraphManager::createCollection(std::string const& name,
    TRI_col_type_e colType) {
  TRI_ASSERT(colType == TRI_COL_TYPE_DOCUMENT || colType == TRI_COL_TYPE_EDGE);

  VPackBuilder collection;
  {
    VPackObjectBuilder guard(&collection);
    collection.add(StaticStrings::DataSourceType, VPackValue(colType));
    collection.add(StaticStrings::DataSourceName, VPackValue(name));
  }
  ctx()->vocbase().createCollection(collection.slice());
}

void GraphManager::findOrCreateVertexCollectionByName(
    std::string&& name) {
  std::shared_ptr<LogicalCollection> def;

  def = getCollectionByName(ctx()->vocbase(), name);
  if (def == nullptr) {
    createVertexCollection(std::move(name));
  }
  
}

void GraphManager::checkDuplicateEdgeDefinitions(
    VPackSlice edgeDefinitions) {

  std::vector<std::string> tmpCollections;
  std::map<std::string, VPackSlice> tmpEdgeDefinitions;

  if (edgeDefinitions.isArray()) {
    for (auto const& edgeDefinition : VPackArrayIterator(edgeDefinitions)) {
      std::string col;
      if (edgeDefinition.get("collection").isString()) {
        col = edgeDefinition.get("collection").copyString();
      } else {
        return;
      }

      bool found = std::find(tmpCollections.begin(), tmpCollections.end(), col) != tmpCollections.end();
      if (found) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_COLLECTION_MULTI_USE);
      }
      tmpCollections.emplace_back(col);
      tmpEdgeDefinitions.emplace(std::make_pair("col", edgeDefinition));
    }
  }

  VPackBuilder graphsBuilder;
  readGraphs(graphsBuilder, arangodb::aql::PART_DEPENDENT);
  VPackSlice graphs = graphsBuilder.slice();

  if (graphs.get("graphs").isArray()) {
    for (auto singleGraph : VPackArrayIterator(graphs.get("graphs"))) {
      singleGraph = singleGraph.resolveExternals();
      VPackSlice sGEDs = singleGraph.get("edgeDefinitions");
      if (!sGEDs.isArray()) {
        return;
      }
      for (auto const& sGED : VPackArrayIterator(sGEDs)) {
        std::string col;
        if (sGED.get("collection").isString()) {
          col = sGED.get("collection").copyString();
        } else {
          return;
        }
        bool foundS = std::find(tmpCollections.begin(), tmpCollections.end(), col) != tmpCollections.end();
        if (foundS) {
          try {
            if (sGED.toString() != tmpEdgeDefinitions.at(col).toString()) {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_COLLECTION_MULTI_USE);
            }
          } catch (...) {
          }
        }
      }
    }
  }
}

void GraphManager::findOrCreateCollectionsByEdgeDefinitions(
    VPackSlice edgeDefinitions) {
  std::shared_ptr<LogicalCollection> def;

  for (auto const& edgeDefinition : VPackArrayIterator(edgeDefinitions)) {
    Result res = Graph::ValidateEdgeDefinition(edgeDefinition); // TODO forward res correctly

    std::string collection = edgeDefinition.get("collection").copyString();
    VPackSlice from = edgeDefinition.get("from");
    VPackSlice to = edgeDefinition.get("to");

    std::unordered_set<std::string> fromSet;
    std::unordered_set<std::string> toSet;

    def = getCollectionByName(ctx()->vocbase(), collection);

    if (def == nullptr) {
      createEdgeCollection(std::move(collection));
    }

    // duplicates in from and to shouldn't occur, but are safely ignored here
    for (auto const& edge : VPackArrayIterator(from)) {
      def = getCollectionByName(ctx()->vocbase(), edge.copyString());
      if (def == nullptr) {
        createVertexCollection(edge.copyString());
      }
    }
    for (auto const& edge : VPackArrayIterator(to)) {
      def = getCollectionByName(ctx()->vocbase(), edge.copyString());
      if (def == nullptr) {
        createVertexCollection(edge.copyString());
      }
    }
  }
}

/// @brief extract the collection by either id or name, may return nullptr!
std::shared_ptr<LogicalCollection> GraphManager::getCollectionByName(
    TRI_vocbase_t& vocbase,
    std::string const& name
) {
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

ResultT<std::pair<OperationResult, Result>> GraphManager::createGraph(VPackSlice document, bool waitForSync) {

  VPackSlice graphName = document.get("name");
  if (graphName.isNone()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }

  // validate edgeDefinitions
  VPackSlice edgeDefinitions = document.get(Graph::_attrEdgeDefs);
  if (edgeDefinitions.isArray()) {
    for (auto const& def : VPackArrayIterator(edgeDefinitions)) {
      Result res = Graph::ValidateEdgeDefinition(def);
      if (res.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }
  }

  // validate orphans
  VPackSlice orphanCollections = document.get(Graph::_attrOrphans);
  if (orphanCollections.isArray()) {
    for (auto const& def : VPackArrayIterator(orphanCollections)) {
      Result res = Graph::ValidateOrphanCollection(def);
      if (res.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }
  }

  int replicationFactor;
  try {
    replicationFactor = document.get(Graph::_attrReplicationFactor).getInt();
  } catch (...) {
    replicationFactor = 1;
  }

  int numberOfShards;
  try {
    numberOfShards = document.get(Graph::_attrNumberOfShards).getInt();
  } catch (...) {
    numberOfShards = 1;
  }

  bool isSmart;
  try {
    isSmart = document.get(Graph::_attrIsSmart).getBool();
  } catch (...) {
    isSmart = false;
  }

  // check for multiple collection graph usage

  
  // check if graph already exists
  VPackBuilder checkDocument;
  {
    VPackObjectBuilder guard(&checkDocument);
    checkDocument.add(StaticStrings::KeyString, graphName);
  }

  std::vector<std::string> readCollections;
  std::vector<std::string> writeCollections;
  writeCollections.emplace_back(Graph::_graphs);

  transaction::Options trxOptions;
  trxOptions.waitForSync = waitForSync;

  std::unique_ptr<transaction::Methods> trx(
    new UserTransaction(ctx(), readCollections, writeCollections, {}, trxOptions));

  Result res = trx->begin();

  if (!res.ok()) {
    return res;
  }

  OperationOptions options;
  options.waitForSync = waitForSync;

  // TODO: verify graph collection usage in multiple graphs
  // TODO: arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.code
  // TODO: not implemented yet
  // check, if a collection is already used in a different edgeDefinition
  if (edgeDefinitions.isArray()) {
    checkDuplicateEdgeDefinitions(edgeDefinitions);
  }

  OperationResult checkDoc = trx->document(Graph::_graphs, checkDocument.slice(), options);
  if (checkDoc.ok()) {
    trx->finish(checkDoc.result);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_DUPLICATE, "graph already exists");
  }

  // find or create the collections given by the edge definition 
  findOrCreateCollectionsByEdgeDefinitions(edgeDefinitions);

  // find or create the collections given by the edge definition
  if (orphanCollections.isArray()) {
    for (auto const& orphan : VPackArrayIterator(orphanCollections)) {
      findOrCreateVertexCollectionByName(orphan.copyString());
    }
  }

  // finally save the graph
  VPackBuilder builder;
  builder.add(VPackValue(VPackValueType::Object));
  builder.add(StaticStrings::KeyString, graphName);
  if (edgeDefinitions.isArray()) {
    builder.add(Graph::_attrEdgeDefs, edgeDefinitions);
  }
  if (orphanCollections.isArray()) {
    builder.add(Graph::_attrOrphans, orphanCollections);
  }
  builder.add(Graph::_attrReplicationFactor, VPackValue(replicationFactor));
  builder.add(Graph::_attrNumberOfShards, VPackValue(numberOfShards));
  builder.add(Graph::_attrIsSmart, VPackValue(isSmart));
  builder.close();

  OperationResult result = trx->insert(Graph::_graphs, builder.slice(), options);

  res = trx->finish(result.result);
  return std::make_pair(std::move(result), std::move(res));
}

void GraphManager::readGraphs(velocypack::Builder& builder, aql::QueryPart QueryPart) {
  std::string const queryStr("FOR g IN _graphs RETURN g");
  auto emptyBuilder = std::make_shared<VPackBuilder>();
  arangodb::aql::Query query(
    false,
    ctx()->vocbase(),
    arangodb::aql::QueryString(queryStr),
    emptyBuilder,
    emptyBuilder,
    QueryPart
  );

  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
      << "starting to load graphs information";
  auto queryResult = query.execute(QueryRegistryFeature::QUERY_REGISTRY);

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
        (queryResult.code == TRI_ERROR_QUERY_KILLED)) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(
        queryResult.code, "Error executing graphs query: " + queryResult.details);
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
