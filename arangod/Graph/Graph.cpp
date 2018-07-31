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

Graph::Graph(std::string&& graphName_, velocypack::Slice const& slice)
    : _graphName(graphName_),
      _vertexColls(),
      _edgeColls(),
      _numberOfShards(basics::VelocyPackHelper::readNumericValue<uint64_t>(
          slice, StaticStrings::NumberOfShards, 1)),
      _replicationFactor(basics::VelocyPackHelper::readNumericValue<uint64_t>(
          slice, StaticStrings::ReplicationFactor, 1)),
      _rev(basics::VelocyPackHelper::getStringValue(
          slice, StaticStrings::RevString, "")) {
  if (slice.hasKey(StaticStrings::GraphEdgeDefinitions)) {
    VPackSlice edgeDefs = slice.get(StaticStrings::GraphEdgeDefinitions);
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
        VPackSlice tmp = def.get(StaticStrings::GraphFrom);
        insertVertexCollections(tmp);
      } catch (...) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_GRAPH_INVALID_GRAPH,
            "didn't find from-collection in the graph definition");
      }
      try {
        VPackSlice tmp = def.get(StaticStrings::GraphTo);
        insertVertexCollections(tmp);
      } catch (...) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_GRAPH_INVALID_GRAPH,
            "didn't find to-collection in the graph definition");
      }
    }
  }
  if (slice.hasKey(StaticStrings::GraphOrphans)) {
    auto orphans = slice.get(StaticStrings::GraphOrphans);
    insertVertexCollections(orphans);
    insertOrphanCollections(orphans);
  }

  /*
#ifdef USE_ENTERPRISE
  if (slice.hasKey(StaticStrings::GraphIsSmart)) {
    setSmartState(slice.get(StaticStrings::GraphIsSmart).getBool());
  }
  if (slice.hasKey(StaticStrings::GraphSmartGraphAttribute)) {
    setSmartGraphAttribute(
        slice.get(StaticStrings::GraphSmartGraphAttribute).copyString());
  }
#endif
*/

}

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

const std::set<std::string> & Graph::orphanCollections() const {
  return _orphanColls;
}

std::set<std::string> const& Graph::edgeCollections() const {
  return _edgeColls;
}

std::vector<std::string> const& Graph::edgeDefinitionNames() const {
  return _edgeDefsNames;
}

std::map<std::string, EdgeDefinition> const& Graph::edgeDefinitions()
    const {
  return _edgeDefs;
}

uint64_t Graph::numberOfShards() const { return _numberOfShards; }

uint64_t Graph::replicationFactor() const { return _replicationFactor; }

std::string const Graph::id() const {
  return std::string(StaticStrings::GraphCollection + "/" + _graphName);
}

std::string const& Graph::rev() const { return _rev; }

void Graph::addEdgeCollection(std::string&& name) {
  _edgeColls.emplace(std::move(name));
}

void Graph::addVertexCollection(std::string&& name) {
  _vertexColls.emplace(std::move(name));
}

void Graph::addOrphanCollection(std::string&& name) {
  _orphanColls.emplace(std::move(name));
}

void Graph::setSmartState(bool state) { _isSmart = state; }

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
  builder.close(); // EdgeDefinitions

  // Orphan Collections
  builder.add(VPackValue(StaticStrings::GraphOrphans));
  builder.openArray();
  for (auto const& on : _orphanColls) {
    builder.add(VPackValue(on));
  }
  builder.close(); // Orphans
}

void Graph::enhanceEngineInfo(VPackBuilder&) const {}

// validates the type:
// edgeDefinition : { collection : string, from : [string], to : [string] }
Result EdgeDefinition::validateEdgeDefinition(
    VPackSlice const& edgeDefinition) {
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

  for (auto const& key : std::array<std::string, 2>{
           {StaticStrings::GraphFrom, StaticStrings::GraphTo}}) {
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
std::shared_ptr<velocypack::Buffer<uint8_t>> EdgeDefinition::sortEdgeDefinition(
    VPackSlice const& edgeDefinition) {
  arangodb::basics::VelocyPackHelper::VPackLess<true> sorter;
  VPackBuilder from = VPackCollection::sort(edgeDefinition.get(StaticStrings::GraphFrom), sorter);
  VPackBuilder to = VPackCollection::sort(edgeDefinition.get(StaticStrings::GraphTo), sorter);
  VPackBuilder sortedBuilder;
  sortedBuilder.openObject();
  sortedBuilder.add("collection", edgeDefinition.get("collection"));
  sortedBuilder.add(StaticStrings::GraphFrom, from.slice());
  sortedBuilder.add(StaticStrings::GraphTo, to.slice());
  sortedBuilder.close();

  return sortedBuilder.steal();
}

ResultT<EdgeDefinition> EdgeDefinition::createFromVelocypack(
    VPackSlice edgeDefinition) {
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
  return this->getName() == other.getName() &&
         this->getFrom() == other.getFrom() && this->getTo() == other.getTo();
}

bool EdgeDefinition::operator!=(EdgeDefinition const& other) const {
  return this->getName() != other.getName() ||
         this->getFrom() != other.getFrom() || this->getTo() != other.getTo();
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

bool EdgeDefinition::hasFrom(std::string const &vertexCollection) const {
  return getFrom().find(vertexCollection) != getFrom().end();
}

bool EdgeDefinition::hasTo(std::string const &vertexCollection) const {
  return getTo().find(vertexCollection) != getTo().end();
}

bool EdgeDefinition::hasVertexCollection(const std::string &vertexCollection) const {
  return hasFrom(vertexCollection) || hasTo(vertexCollection);
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

Result Graph::addEdgeDefinition(VPackSlice const& edgeDefinitionSlice) {
  auto res = EdgeDefinition::createFromVelocypack(edgeDefinitionSlice);

  if (res.fail()) {
    return res.copy_result();
  }
  TRI_ASSERT(res.ok());

  EdgeDefinition const& edgeDefinition = res.get();

  std::string const& collection = edgeDefinition.getName();

  _edgeDefsNames.emplace_back(collection);

  bool inserted;
  std::tie(std::ignore, inserted) =
      _edgeDefs.emplace(collection, edgeDefinition);

  // This can only happen if addEdgeDefinition is called without clearing
  // _edgeDefs first (which would be a logical error), or if the same collection
  // is defined multiple times (which the user should not be allowed to do).
  if (!inserted) {
    return Result(TRI_ERROR_GRAPH_COLLECTION_MULTI_USE,
                  "Relation '" + collection + "' defined twice in same graph!");
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

bool Graph::hasEdgeCollection(std::string const& collectionName) const {
  TRI_ASSERT(
      (edgeDefinitions().find(collectionName) != edgeDefinitions().end()) ==
      (edgeCollections().find(collectionName) != edgeCollections().end()));
  return edgeCollections().find(collectionName) != edgeCollections().end();
}

bool Graph::hasVertexCollection(std::string const& collectionName) const {
  return vertexCollections().find(collectionName) != vertexCollections().end();
}

bool Graph::hasOrphanCollection(std::string const& collectionName) const {
  return orphanCollections().find(collectionName) != orphanCollections().end();
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

Result Graph::validateCollection(LogicalCollection* col) const {
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

bool Graph::isSmart() const {
  return false;
}

void Graph::createCollectionOptions(VPackBuilder& builder, bool waitForSync) const {
  bool isCluster = arangodb::ServerState::instance()->isRunningInCluster();

  builder.openObject();
  builder.add(StaticStrings::WaitForSyncString, VPackValue(waitForSync));
  if (isCluster) {
    builder.add(StaticStrings::NumberOfShards, VPackValue(numberOfShards()));
    builder.add(StaticStrings::ReplicationFactor, VPackValue(replicationFactor()));
  }
  builder.close();
}

void Graph::createCollectionOptions(VPackBuilder& builder, bool waitForSync, VPackSlice options) {
  bool isCluster = arangodb::ServerState::instance()->isRunningInCluster();

  builder.openObject();
  builder.add(StaticStrings::WaitForSyncString, VPackValue(waitForSync));
  if (isCluster) {
    builder.add(StaticStrings::NumberOfShards,
                VPackValue(options.get(StaticStrings::NumberOfShards).getUInt()));
    builder.add(
            StaticStrings::ReplicationFactor,
        VPackValue(options.get(StaticStrings::ReplicationFactor).getUInt()));
  }
  builder.close();
}


boost::optional<const EdgeDefinition&> Graph::getEdgeDefinition(
    std::string const& collectionName) const {
  auto it = edgeDefinitions().find(collectionName);
  if (it == edgeDefinitions().end()) {
    TRI_ASSERT(!hasEdgeCollection(collectionName));
    return boost::none;
  }

  TRI_ASSERT(hasEdgeCollection(collectionName));
  return {it->second};
}

Graph Graph::fromSlice(velocypack::Slice const &info) {
  if (!info.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_GRAPH_INVALID_GRAPH,
      "invalid graph: expected an object");
  }
  if (!info.get(StaticStrings::KeyString).isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_GRAPH_INVALID_GRAPH,
      "invalid graph: _key is not a string");
  }

  return Graph(info.get(StaticStrings::KeyString).copyString(), info);
}
