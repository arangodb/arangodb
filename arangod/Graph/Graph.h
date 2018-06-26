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

#ifndef ARANGOD_GRAPH_GRAPH_H
#define ARANGOD_GRAPH_GRAPH_H

#include <velocypack/Buffer.h>
#include <chrono>
#include <utility>

#include "Aql/Query.h"
#include "Aql/VariableGenerator.h"
#include "Basics/ReadWriteLock.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ResultT.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationResult.h"

namespace arangodb {
namespace graph {

class EdgeDefinition {
 public:
  EdgeDefinition(std::string edgeCollection_,
                 std::unordered_set<std::string>&& from_,
                 std::unordered_set<std::string>&& to_)
      : _edgeCollection(std::move(edgeCollection_)), _from(from_), _to(to_) {}

  std::string const& getName() const { return _edgeCollection; }
  std::unordered_set<std::string> const& getFrom() const { return _from; }
  std::unordered_set<std::string> const& getTo() const { return _to; }

  // TODO implement these
  bool isFrom(std::string const& vertexCollection) const;
  bool isTo(std::string const& vertexCollection) const;

  /// @brief validate the structure of edgeDefinition, i.e.
  /// that it contains the correct attributes, and that they contain the correct
  /// types of values.
  static Result validateEdgeDefinition(const velocypack::Slice& edgeDefinition);
  static std::shared_ptr<velocypack::Buffer<uint8_t>> sortEdgeDefinition(
      const velocypack::Slice& edgeDefinition);

  static ResultT<EdgeDefinition> createFromVelocypack(
      velocypack::Slice edgeDefinition);

  bool operator==(EdgeDefinition const& other) const;
  bool operator!=(EdgeDefinition const& other) const;

 private:
  std::string _edgeCollection;
  std::unordered_set<std::string> _from;
  std::unordered_set<std::string> _to;
};

class Graph {
 public:
  explicit Graph(std::string&& graphName, velocypack::Slice const& info);

  explicit Graph(std::string const& graphName, velocypack::Slice const& info)
      : Graph(std::string{graphName}, info) {}

  virtual ~Graph() = default;

 private:
  /// @brief name of this graph
  std::string const _graphName;

  /// @brief the cids of all vertexCollections
  std::unordered_set<std::string> _vertexColls;

  /// @brief the cids of all orphanCollections
  std::unordered_set<std::string> _orphanColls;

  /// @brief the cids of all edgeCollections
  std::unordered_set<std::string> _edgeColls;

  /// @brief edge definition names of this graph, ordered as in the database
  std::vector<std::string> _edgeDefsNames;

  /// @brief edge definitions of this graph
  std::unordered_map<std::string, EdgeDefinition> _edgeDefs;

  /// @brief state if smart graph enabled
  bool _isSmart;

  /// @brief number of shards of this graph
  uint64_t _numberOfShards;

  /// @brief replication factor of this graph
  uint64_t _replicationFactor;

  /// @brief smarGraphAttribute of this graph
  std::string _smartGraphAttribute;

  /// @brief id of this graph
  std::string _id;

  /// @brief revision of this graph
  std::string _rev;

 public:
  /// @brief Graph collection name
  static std::string const _graphs;

  /// @brief Graph collection edge definition attribute name
  static char const* _attrDropCollections;

  /// @brief Graph collection edge definition attribute name
  static char const* _attrEdgeDefs;

  /// @brief Graph collection orphan list arribute name
  static char const* _attrOrphans;

  /// @brief Graph collection smart state attribute name
  static char const* _attrIsSmart;

  /// @brief Graph collection number of shards attribute name
  static char const* _attrNumberOfShards;

  /// @brief Graph collection replication factor attribute name
  static char const* _attrReplicationFactor;

  /// @brief Graph collection smartgraph attribute name
  static char const* _attrSmartGraphAttribute;

  static Result validateOrphanCollection(
      const velocypack::Slice& orphanDefinition);

 public:
  /// @brief get the cids of all vertexCollections
  std::unordered_set<std::string> const& vertexCollections() const;

  /// @brief get the cids of all orphanCollections
  std::unordered_set<std::string> const& orphanCollections() const;

  /// @brief get the cids of all edgeCollections
  std::unordered_set<std::string> const& edgeCollections() const;

  /// @brief get the cids of all edgeCollections
  std::unordered_map<std::string, EdgeDefinition> const& edgeDefinitions()
      const;

  /// @brief get the cids of all edgeCollections
  std::vector<std::string> const& edgeDefinitionNames() const;

  bool hasEdgeCollection(std::string const& collectionName) const;

  bool const& isSmart() const;
  uint64_t numberOfShards() const;
  uint64_t replicationFactor() const;
  std::string const& smartGraphAttribute() const;
  std::string const& id() const;
  std::string const& rev() const;

  std::string const& name() const { return _graphName; }

  /// @brief return a VelocyPack representation of the graph
  void toVelocyPack(velocypack::Builder&) const;

  void graphToVpack(VPackBuilder& builder) const;
  void edgesToVpack(VPackBuilder& builder) const;
  void verticesToVpack(VPackBuilder& builder) const;

  virtual void enhanceEngineInfo(velocypack::Builder&) const;

  std::ostream& operator<<(std::ostream& ostream);

 private:
  /// @brief adds one edge definition. the edge definition must not have been
  /// added before.
  Result addEdgeDefinition(velocypack::Slice const& edgeDefinitionSlice);

  /// @brief Add an edge collection to this graphs definition
  void addEdgeCollection(std::string&&);

  /// @brief Add a vertex collection to this graphs definition
  void addVertexCollection(std::string&&);

  /// @brief Add an oprha nvertex collection to this graphs definition
  void addOrphanCollection(std::string&&);

  /// @brief Add Collections to the object
  void insertVertexCollections(velocypack::Slice arr);

  /// @brief Add orphanCollections to the object
  void insertOrphanCollections(velocypack::Slice arr);

  /// @brief Set isSmart to the graph definition
  void setSmartState(bool state);

  /// @brief Set numberOfShards to the graph definition
  void setNumberOfShards(uint64_t numberOfShards);

  /// @brief Set replicationFactor to the graph definition
  void setReplicationFactor(uint64_t setReplicationFactor);

  /// @brief Set smartGraphAttribute to the graph definition
  void setSmartGraphAttribute(std::string&& smartGraphAttribute);

  /// @brief Set id to the graph definition
  void setId(std::string&& id);

  /// @brief Set rev to the graph definition
  void setRev(std::string&& rev);
};
}  // namespace graph
}  // namespace arangodb

#endif  // ARANGOD_GRAPH_GRAPH_H
