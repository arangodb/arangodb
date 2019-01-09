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
#include "Cluster/ServerState.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

using namespace arangodb;
using namespace arangodb::graph;
using VelocyPackHelper = basics::VelocyPackHelper;

#ifndef USE_ENTERPRISE
// Factory methods
std::unique_ptr<Graph> Graph::fromPersistence(VPackSlice document, TRI_vocbase_t& vocbase) {
  if (document.isExternal()) {
    document = document.resolveExternal();
  }
  std::unique_ptr<Graph> result{new Graph{document}};
  return result;
}

std::unique_ptr<Graph> Graph::fromUserInput(std::string&& name, VPackSlice document,
                                            VPackSlice options) {
  if (document.isExternal()) {
    document = document.resolveExternal();
  }
  std::unique_ptr<Graph> result{new Graph{std::move(name), document, options}};
  return result;
}
#endif

std::unique_ptr<Graph> Graph::fromUserInput(std::string const& name,
                                            VPackSlice document, VPackSlice options) {
  return Graph::fromUserInput(std::string{name}, document, options);
}

// From persistence
Graph::Graph(velocypack::Slice const& slice)
    : _graphName(
          VelocyPackHelper::getStringValue(slice, StaticStrings::KeyString, "")),
      _vertexColls(),
      _edgeColls(),
      _numberOfShards(basics::VelocyPackHelper::readNumericValue<uint64_t>(slice, StaticStrings::NumberOfShards,
                                                                           1)),
      _replicationFactor(basics::VelocyPackHelper::readNumericValue<uint64_t>(
          slice, StaticStrings::ReplicationFactor, 1)),
      _rev(basics::VelocyPackHelper::getStringValue(slice, StaticStrings::RevString,
                                                    "")) {
  // If this happens we have a document without an _key Attribute.
  TRI_ASSERT(!_graphName.empty());
  if (_graphName.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "Persisted graph is invalid. It does not "
                                   "have a _key set. Please contact support.");
  }

  // If this happens we have a document without an _rev Attribute.
  TRI_ASSERT(!_rev.empty());
  if (_rev.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "Persisted graph is invalid. It does not "
                                   "have a _rev set. Please contact support.");
  }

  if (slice.hasKey(StaticStrings::GraphEdgeDefinitions)) {
    parseEdgeDefinitions(slice.get(StaticStrings::GraphEdgeDefinitions));
  }
  if (slice.hasKey(StaticStrings::GraphOrphans)) {
    insertOrphanCollections(slice.get(StaticStrings::GraphOrphans));
  }
}

// From user input
Graph::Graph(std::string&& graphName, VPackSlice const& info, VPackSlice const& options)
    : _graphName(graphName),
      _vertexColls(),
      _edgeColls(),
      _numberOfShards(1),
      _replicationFactor(1),
      _rev("") {
  if (_graphName.empty()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_CREATE_MISSING_NAME);
  }
  TRI_ASSERT(_rev.empty());

  if (info.hasKey(StaticStrings::GraphEdgeDefinitions)) {
    parseEdgeDefinitions(info.get(StaticStrings::GraphEdgeDefinitions));
  }
  if (info.hasKey(StaticStrings::GraphOrphans)) {
    insertOrphanCollections(info.get(StaticStrings::GraphOrphans));
  }
  if (options.isObject()) {
    _numberOfShards =
        VelocyPackHelper::readNumericValue<uint64_t>(options, StaticStrings::NumberOfShards, 1);
    _replicationFactor =
        VelocyPackHelper::readNumericValue<uint64_t>(options, StaticStrings::ReplicationFactor, 1);
  }
}

void Graph::parseEdgeDefinitions(VPackSlice edgeDefs) {
  TRI_ASSERT(edgeDefs.isArray());
  if (!edgeDefs.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_GRAPH_INVALID_GRAPH,
        "'edgeDefinitions' are not an array in the graph definition");
  }

  for (auto const& def : VPackArrayIterator(edgeDefs)) {
    auto edgeDefRes = addEdgeDefinition(def);
    if (edgeDefRes.fail()) {
      THROW_ARANGO_EXCEPTION(edgeDefRes.copy_result());
    }
  }
}

void Graph::insertOrphanCollections(VPackSlice const arr) {
  TRI_ASSERT(arr.isArray());
  for (auto const& c : VPackArrayIterator(arr)) {
    TRI_ASSERT(c.isString());
    validateOrphanCollection(c);
    addOrphanCollection(c.copyString());
  }
}

std::set<std::string> const& Graph::vertexCollections() const {
  return _vertexColls;
}

std::set<std::string> const& Graph::orphanCollections() const {
  return _orphanColls;
}

std::set<std::string> const& Graph::edgeCollections() const {
  return _edgeColls;
}

std::map<std::string, EdgeDefinition> const& Graph::edgeDefinitions() const {
  return _edgeDefs;
}

std::map<std::string, EdgeDefinition>& Graph::edgeDefinitions() {
  return _edgeDefs;
}

uint64_t Graph::numberOfShards() const { return _numberOfShards; }

uint64_t Graph::replicationFactor() const { return _replicationFactor; }

std::string const Graph::id() const {
  return std::string(StaticStrings::GraphCollection + "/" + _graphName);
}

std::string const& Graph::rev() const { return _rev; }

void Graph::addVertexCollection(std::string const& name) {
  // Promote Orphans to vertices
  _orphanColls.erase(name);
  _vertexColls.emplace(name);
}

void Graph::rebuildOrphans(EdgeDefinition const& oldEdgeDefinition) {
  std::set<std::string> potentialNewOrphans;

  // build the potential new orphans
  for (auto const& from : oldEdgeDefinition.getFrom()) {
    potentialNewOrphans.emplace(from);
  }
  for (auto const& to : oldEdgeDefinition.getTo()) {
    potentialNewOrphans.emplace(to);
  }

  // check if potential new orphans are still available in other
  // edgeDefinitions from's or to's or not.
  for (auto potOrphan : potentialNewOrphans) {
    bool found = false;
    for (auto const& edgeDefinition : edgeDefinitions()) {
      if (edgeDefinition.second.isVertexCollectionUsed(potOrphan)) {
        found = true;
        break;
      }
    }
    if (!found) {
      _vertexColls.erase(potOrphan);
      addOrphanCollection(std::move(potOrphan));
    }
  }
}

Result Graph::removeOrphanCollection(std::string&& name) {
  TRI_ASSERT(_vertexColls.find(name) != _vertexColls.end());
  if (_orphanColls.find(name) != _orphanColls.end()) {
    _orphanColls.erase(name);
    _vertexColls.erase(name);
    return TRI_ERROR_NO_ERROR;
  } else {
    return TRI_ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION;
  }
}

Result Graph::addOrphanCollection(std::string&& name) {
  if (!_vertexColls.insert(name).second) {
    return TRI_ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF;
  }
  TRI_ASSERT(_orphanColls.find(name) == _orphanColls.end());
  _orphanColls.insert(std::move(name));
  return TRI_ERROR_NO_ERROR;
}

void Graph::setNumberOfShards(uint64_t numberOfShards) {
  _numberOfShards = numberOfShards;
}

void Graph::setReplicationFactor(uint64_t replicationFactor) {
  _replicationFactor = replicationFactor;
}

void Graph::setRev(std::string&& rev) { _rev = std::move(rev); }

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

void Graph::toPersistence(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());

  // The name
  builder.add(StaticStrings::KeyString, VPackValue(_graphName));

  // Cluster Information
  builder.add(StaticStrings::NumberOfShards, VPackValue(_numberOfShards));
  builder.add(StaticStrings::ReplicationFactor, VPackValue(_replicationFactor));
  builder.add(StaticStrings::GraphIsSmart, VPackValue(isSmart()));

  // EdgeDefinitions
  builder.add(VPackValue(StaticStrings::GraphEdgeDefinitions));
  builder.openArray();
  for (auto const& it : edgeDefinitions()) {
    it.second.addToBuilder(builder);
  }
  builder.close();  // EdgeDefinitions

  // Orphan Collections
  builder.add(VPackValue(StaticStrings::GraphOrphans));
  builder.openArray();
  for (auto const& on : _orphanColls) {
    builder.add(VPackValue(on));
  }
  builder.close();  // Orphans
}

void Graph::enhanceEngineInfo(VPackBuilder&) const {}

// validates the type:
// edgeDefinition : { collection : string, from : [string], to : [string] }
Result EdgeDefinition::validateEdgeDefinition(VPackSlice const& edgeDefinition) {
  if (!edgeDefinition.isObject()) {
    return Result(TRI_ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION);
  }

  for (auto const& key : std::array<std::string, 3>{
           {"collection", StaticStrings::GraphFrom, StaticStrings::GraphTo}}) {
    if (!edgeDefinition.hasKey(key)) {
      return Result(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT,
                    "Attribute '" + key + "' missing in edge definition!");
    }
  }

  if (!edgeDefinition.get("collection").isString()) {
    return Result(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT,
                  "edge definition is not a string!");
  }

  for (auto const& key :
       std::array<std::string, 2>{{StaticStrings::GraphFrom, StaticStrings::GraphTo}}) {
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

void EdgeDefinition::toVelocyPack(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());

  builder.add("collection", VPackValue(getName()));
  builder.add("from", VPackValue(VPackValueType::Array));
  for (auto const& from : getFrom()) {
    builder.add(VPackValue(from));
  }
  builder.close();  // array
  builder.add("to", VPackValue(VPackValueType::Array));
  for (auto const& to : getTo()) {
    builder.add(VPackValue(to));
  }
  builder.close();  // array
}

ResultT<EdgeDefinition> EdgeDefinition::createFromVelocypack(VPackSlice edgeDefinition) {
  Result res = EdgeDefinition::validateEdgeDefinition(edgeDefinition);
  if (res.fail()) {
    return res;
  }
  std::string collection = edgeDefinition.get("collection").copyString();
  VPackSlice from = edgeDefinition.get(StaticStrings::GraphFrom);
  VPackSlice to = edgeDefinition.get(StaticStrings::GraphTo);

  std::set<std::string> fromSet;
  std::set<std::string> toSet;

  // duplicates in from and to shouldn't occur, but are safely ignored here
  for (auto const& it : VPackArrayIterator(from)) {
    fromSet.emplace(it.copyString());
  }
  for (auto const& it : VPackArrayIterator(to)) {
    toSet.emplace(it.copyString());
  }

  return EdgeDefinition{collection, std::move(fromSet), std::move(toSet)};
}

bool EdgeDefinition::operator==(EdgeDefinition const& other) const {
  return getName() == other.getName() && getFrom() == other.getFrom() &&
         getTo() == other.getTo();
}

bool EdgeDefinition::operator!=(EdgeDefinition const& other) const {
  return getName() != other.getName() || getFrom() != other.getFrom() ||
         getTo() != other.getTo();
}

bool EdgeDefinition::renameCollection(std::string const& oldName, std::string const& newName) {
  bool renamed = false;

  // from
  if (isFromVertexCollectionUsed(oldName)) {
    _from.erase(oldName);
    _from.insert(newName);
    renamed = true;
  }

  // to
  if (isToVertexCollectionUsed(oldName)) {
    _to.erase(oldName);
    _to.insert(newName);
    renamed = true;
  }

  // edge collection
  if (getName() == oldName) {
    setName(newName);
    renamed = true;
  }

  return renamed;
}

bool EdgeDefinition::isFromVertexCollectionUsed(std::string const& collectionName) const {
  if (getFrom().find(collectionName) != getFrom().end()) {
    return true;
  }
  return false;
}

bool EdgeDefinition::isToVertexCollectionUsed(std::string const& collectionName) const {
  if (getTo().find(collectionName) != getTo().end()) {
    return true;
  }
  return false;
}

bool EdgeDefinition::isVertexCollectionUsed(std::string const& collectionName) const {
  if (getFrom().find(collectionName) != getFrom().end() ||
      getTo().find(collectionName) != getTo().end()) {
    return true;
  }
  return false;
}

void EdgeDefinition::addToBuilder(VPackBuilder& builder) const {
  builder.add(VPackValue(VPackValueType::Object));
  builder.add("collection", VPackValue(getName()));

  builder.add("from", VPackValue(VPackValueType::Array));
  for (auto const& from : getFrom()) {
    builder.add(VPackValue(from));
  }
  builder.close();  // from

  // to
  builder.add("to", VPackValue(VPackValueType::Array));
  for (auto const& to : getTo()) {
    builder.add(VPackValue(to));
  }
  builder.close();  // to

  builder.close();  // obj
}

bool EdgeDefinition::hasFrom(std::string const& vertexCollection) const {
  return getFrom().find(vertexCollection) != getFrom().end();
}

bool EdgeDefinition::hasTo(std::string const& vertexCollection) const {
  return getTo().find(vertexCollection) != getTo().end();
}

// validates the type:
// orphanDefinition : string <collectionName>
Result Graph::validateOrphanCollection(VPackSlice const& orphanCollection) {
  if (!orphanCollection.isString()) {
    return Result(TRI_ERROR_GRAPH_CREATE_MALFORMED_ORPHAN_LIST,
                  "orphan collection is not a string!");
  }
  return Result();
}

bool Graph::removeEdgeDefinition(std::string const& edgeDefinitionName) {
  auto maybeOldEdgeDef = getEdgeDefinition(edgeDefinitionName);
  if (!maybeOldEdgeDef) {
    // Graph doesn't contain this edge definition, no need to do anything.
    return false;
  }
  EdgeDefinition const oldEdgeDef = maybeOldEdgeDef.get();

  _edgeColls.erase(edgeDefinitionName);
  _edgeDefs.erase(edgeDefinitionName);
  rebuildOrphans(oldEdgeDef);
  return true;
}

Result Graph::replaceEdgeDefinition(EdgeDefinition const& edgeDefinition) {
  if (removeEdgeDefinition(edgeDefinition.getName())) {
    return addEdgeDefinition(edgeDefinition);
  }
  // Graph doesn't contain this edge definition, no need to do anything.
  return TRI_ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST;
}

ResultT<EdgeDefinition const*> Graph::addEdgeDefinition(EdgeDefinition const& edgeDefinition) {
  std::string const& collection = edgeDefinition.getName();
  if (hasEdgeCollection(collection)) {
    return {Result(TRI_ERROR_GRAPH_COLLECTION_MULTI_USE,
                   collection + " " +
                       std::string{TRI_errno_string(TRI_ERROR_GRAPH_COLLECTION_MULTI_USE)})};
  }

  _edgeColls.emplace(collection);
  _edgeDefs.emplace(collection, edgeDefinition);
  TRI_ASSERT(hasEdgeCollection(collection));
  for (auto const& it : edgeDefinition.getFrom()) {
    addVertexCollection(it);
  }
  for (auto const& it : edgeDefinition.getTo()) {
    addVertexCollection(it);
  }

  return &_edgeDefs.find(collection)->second;
}

ResultT<EdgeDefinition const*> Graph::addEdgeDefinition(VPackSlice const& edgeDefinitionSlice) {
  auto res = EdgeDefinition::createFromVelocypack(edgeDefinitionSlice);

  if (res.fail()) {
    return res.copy_result();
  }
  TRI_ASSERT(res.ok());

  EdgeDefinition const& edgeDefinition = res.get();

  return addEdgeDefinition(edgeDefinition);
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

bool Graph::hasEdgeCollection(std::string const& collectionName) const {
  TRI_ASSERT((edgeDefinitions().find(collectionName) != edgeDefinitions().end()) ==
             (edgeCollections().find(collectionName) != edgeCollections().end()));
  return edgeCollections().find(collectionName) != edgeCollections().end();
}

bool Graph::hasVertexCollection(std::string const& collectionName) const {
  return vertexCollections().find(collectionName) != vertexCollections().end();
}

bool Graph::hasOrphanCollection(std::string const& collectionName) const {
  return orphanCollections().find(collectionName) != orphanCollections().end();
}

bool Graph::renameCollections(std::string const& oldName, std::string const& newName) {
  // rename not allowed in a smart collection
  if (isSmart()) {
    return false;
  }

  bool renamed = false;

  // rename collections found in edgeDefinitions
  for (auto& it : edgeDefinitions()) {
    if (!renamed) {
      renamed = it.second.renameCollection(oldName, newName);
    } else {
      it.second.renameCollection(oldName, newName);
    }
  }

  // orphans
  if (hasOrphanCollection(oldName)) {
    _orphanColls.erase(oldName);
    _orphanColls.insert(newName);
    renamed = true;
  }

  return renamed;
}

void Graph::graphForClient(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add(VPackValue("graph"));
  builder.openObject();

  toPersistence(builder);
  TRI_ASSERT(builder.isOpenObject());
  builder.add(StaticStrings::RevString, VPackValue(rev()));
  builder.add(StaticStrings::IdString, VPackValue(id()));
  builder.add(StaticStrings::GraphName, VPackValue(_graphName));
  builder.close();  // graph object
}

Result Graph::validateCollection(LogicalCollection& col) const {
  return {TRI_ERROR_NO_ERROR};
}

void Graph::edgesToVpack(VPackBuilder& builder) const {
  builder.add(VPackValue(VPackValueType::Object));
  builder.add("collections", VPackValue(VPackValueType::Array));

  for (auto const& edgeCollection : edgeCollections()) {
    builder.add(VPackValue(edgeCollection));
  }
  builder.close();

  builder.close();
}

void Graph::verticesToVpack(VPackBuilder& builder) const {
  builder.add(VPackValue(VPackValueType::Object));
  builder.add("collections", VPackValue(VPackValueType::Array));

  for (auto const& vertexCollection : vertexCollections()) {
    builder.add(VPackValue(vertexCollection));
  }
  builder.close();

  builder.close();
}

bool Graph::isSmart() const { return false; }

void Graph::createCollectionOptions(VPackBuilder& builder, bool waitForSync) const {
  TRI_ASSERT(builder.isOpenObject());

  builder.add(StaticStrings::WaitForSyncString, VPackValue(waitForSync));
  builder.add(StaticStrings::NumberOfShards, VPackValue(numberOfShards()));
  builder.add(StaticStrings::ReplicationFactor, VPackValue(replicationFactor()));
}

boost::optional<const EdgeDefinition&> Graph::getEdgeDefinition(std::string const& collectionName) const {
  auto it = edgeDefinitions().find(collectionName);
  if (it == edgeDefinitions().end()) {
    TRI_ASSERT(!hasEdgeCollection(collectionName));
    return boost::none;
  }

  TRI_ASSERT(hasEdgeCollection(collectionName));
  return {it->second};
}
