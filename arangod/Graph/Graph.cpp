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

std::unordered_map<std::string, EdgeDefinition> const& Graph::edgeDefinitions()
    const {
  return _edgeDefs;
}

uint64_t Graph::numberOfShards() const { return _numberOfShards; }

uint64_t Graph::replicationFactor() const { return _replicationFactor; }

std::string const& Graph::smartGraphAttribute() const {
  return _smartGraphAttribute;
}

std::string const& Graph::id() const { return _id; }

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

void Graph::setSmartGraphAttribute(std::string&& smartGraphAttribute) {
  _smartGraphAttribute = std::move(smartGraphAttribute);
}

void Graph::setNumberOfShards(uint64_t numberOfShards) {
  _numberOfShards = numberOfShards;
}

void Graph::setReplicationFactor(uint64_t replicationFactor) {
  _replicationFactor = replicationFactor;
}

void Graph::setId(std::string&& id) { _id = std::move(id); }

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

Graph::Graph(std::string&& graphName_, velocypack::Slice const& slice)
    : _graphName(graphName_), _vertexColls(), _edgeColls() {
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
  if (slice.hasKey(StaticStrings::GraphOrphans)) {
    auto orphans = slice.get(StaticStrings::GraphOrphans);
    insertVertexCollections(orphans);
    insertOrphanCollections(orphans);
  }

  #ifdef USE_ENTERPRISE
  if (slice.hasKey(StaticStrings::GraphIsSmart)) {
    setSmartState(slice.get(StaticStrings::GraphIsSmart).getBool());
  }
  if (slice.hasKey(StaticStrings::GraphSmartGraphAttribute)) {
    setSmartGraphAttribute(slice.get(StaticStrings::GraphSmartGraphAttribute).copyString());
  }
  #endif

  if (slice.hasKey(StaticStrings::GraphNumberOfShards)) {
    setNumberOfShards(slice.get(StaticStrings::GraphNumberOfShards).getUInt());
  }
  if (slice.hasKey(StaticStrings::GraphReplicationFactor)) {
    setReplicationFactor(slice.get(StaticStrings::GraphReplicationFactor).getUInt());
  }
  setId(StaticStrings::GraphCollection + "/" + graphName_);
  setRev(slice.get(StaticStrings::RevString).copyString());
}

void Graph::enhanceEngineInfo(VPackBuilder&) const {}

// validates the type:
// edgeDefinition : { collection : string, from : [string], to : [string] }
Result EdgeDefinition::validateEdgeDefinition(
    VPackSlice const& edgeDefinition) {
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
std::shared_ptr<velocypack::Buffer<uint8_t>> EdgeDefinition::sortEdgeDefinition(
    VPackSlice const& edgeDefinition) {
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

ResultT<EdgeDefinition> EdgeDefinition::createFromVelocypack(
    velocypack::Slice edgeDefinition) {
  Result res = EdgeDefinition::validateEdgeDefinition(edgeDefinition);
  // should always be valid unless someone broke his _graphs collection
  // manually
  TRI_ASSERT(res.ok());
  if (res.fail()) {
    return res;
  }
  std::string collection = edgeDefinition.get("collection").copyString();
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

  TRI_ASSERT(res.ok());
  if (res.fail()) {
    return res;
  }

  EdgeDefinition const& edgeDefinition = res.get();

  std::string const& collection = edgeDefinition.getName();

  _edgeDefsNames.emplace_back(collection);

  bool inserted;
  std::tie(std::ignore, inserted) =
      _edgeDefs.emplace(collection, edgeDefinition);

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

bool Graph::hasEdgeCollection(std::string const& collectionName) const {
  TRI_ASSERT(
      (edgeDefinitions().find(collectionName) != edgeDefinitions().end()) ==
      (edgeCollections().find(collectionName) != edgeCollections().end()));
  return edgeCollections().find(collectionName) != edgeCollections().end();
}

void Graph::graphToVpack(VPackBuilder& builder) const {
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
    builder.close();  // array from
    builder.add("to", VPackValue(VPackValueType::Array));
    for (auto const& to : def.getTo()) {
      builder.add(VPackValue(to));
    }
    builder.close();  // array to
    builder.close();  // object
  }
  builder.close();  // object edgedefs

  builder.add("orphanCollections", VPackValue(VPackValueType::Array));
  for (auto const& orphan : orphanCollections()) {
    builder.add(VPackValue(orphan));
  }
  builder.close();  // orphan array

  if (isSmart()) {
    builder.add("isSmart", VPackValue(isSmart()));
    builder.add("smartGraphAttribute", VPackValue(smartGraphAttribute()));
  }

  builder.add("numberOfShards", VPackValue(numberOfShards()));
  builder.add("replicationFactor", VPackValue(replicationFactor()));
  builder.add(StaticStrings::RevString, VPackValue(rev()));
  builder.add(StaticStrings::IdString, VPackValue(id()));

  builder.close();  // object
  builder.close();  // object
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
    builder.add(StaticStrings::GraphNumberOfShards, VPackValue(numberOfShards()));
    builder.add(StaticStrings::GraphReplicationFactor, VPackValue(replicationFactor()));
  }
  builder.close();
}

void Graph::createCollectionOptions(VPackBuilder& builder, bool waitForSync, VPackSlice options) {
  bool isCluster = arangodb::ServerState::instance()->isRunningInCluster();

  builder.openObject();
  builder.add(StaticStrings::WaitForSyncString, VPackValue(waitForSync));
  if (isCluster) {
    builder.add(StaticStrings::GraphNumberOfShards,
                VPackValue(options.get(StaticStrings::GraphNumberOfShards).getUInt()));
    builder.add(
            StaticStrings::GraphReplicationFactor,
        VPackValue(options.get(StaticStrings::GraphReplicationFactor).getUInt()));
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
