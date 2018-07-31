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
  EdgeDefinition(std::string edgeCollection_, std::set<std::string>&& from_,
                 std::set<std::string>&& to_)
      : _edgeCollection(std::move(edgeCollection_)), _from(from_), _to(to_) {}

  std::string const& getName() const { return _edgeCollection; }
  std::set<std::string> const& getFrom() const { return _from; }
  std::set<std::string> const& getTo() const { return _to; }

  /// @brief Adds the edge definition as a new object {collection, from, to}
  /// to the builder.
  void addToBuilder(velocypack::Builder& builder) const;

  bool hasFrom(std::string const& vertexCollection) const;
  bool hasTo(std::string const& vertexCollection) const;

  bool hasVertexCollection(std::string const& vertexCollection) const;

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
  std::set<std::string> _from;
  std::set<std::string> _to;
};

class Graph {
 public:
  Graph(std::string&& graphName, velocypack::Slice const& info);

  Graph(std::string const& graphName, velocypack::Slice const& info)
      : Graph(std::string{graphName}, info) {}

  virtual ~Graph() = default;

  /// @brief named constructor. extracts the graph's name from the _key
  /// attribute.
  static Graph fromSlice(const velocypack::Slice &info);

 public:
  static Result validateOrphanCollection(
      const velocypack::Slice& orphanDefinition);

  virtual void createCollectionOptions(VPackBuilder& builder,
                                       bool waitForSync) const;

  static void createCollectionOptions(VPackBuilder& builder, bool waitForSync,
                                      VPackSlice options);

 public:
  /// @brief get the cids of all vertexCollections
  std::unordered_set<std::string> const& vertexCollections() const;

  /// @brief get the cids of all orphanCollections
  std::set<std::string> const& orphanCollections() const;

  /// @brief get the cids of all edgeCollections
  std::set<std::string> const& edgeCollections() const;

  /// @brief get the cids of all edgeCollections
  std::map<std::string, EdgeDefinition> const& edgeDefinitions() const;

  /// @brief get the cids of all edgeCollections
  std::vector<std::string> const& edgeDefinitionNames() const;

  bool hasEdgeCollection(std::string const& collectionName) const;
  bool hasVertexCollection(std::string const& collectionName) const;
  bool hasOrphanCollection(std::string const& collectionName) const;

  boost::optional<EdgeDefinition const&> getEdgeDefinition(
      std::string const& collectionName) const;

  virtual bool isSmart() const;

  uint64_t numberOfShards() const;
  uint64_t replicationFactor() const;
  std::string const id() const;
  std::string const& rev() const;

  std::string const& name() const { return _graphName; }

  /// @brief return a VelocyPack representation of the graph
  void toVelocyPack(velocypack::Builder&) const;

  /**
   * @brief Create the GraphDocument to be stored in the database.
   *
   * @param builder The builder the result should be written in. Expects an open object.
   */
  virtual void toPersistence(velocypack::Builder& builder) const;

  /**
   * @brief Create the Graph Json Representation to be given to the client.
   *        Uses toPersistence, but also includes _rev and _id values and encapsulates
   *        the date into a graph attribute.
   *
   * @param builder The builder the result should be written in. Expects an open object.
   */
  void graphToVpack(VPackBuilder& builder) const;


  /**
   * @brief Check if the collection is allowed to be used
   * within this graph
   *
   * @param col The collection
   *
   * @return TRUE if we are safe to use it.
   */
  virtual Result validateCollection(LogicalCollection* col) const;

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

  /// @brief Set numberOfShards to the graph definition
  void setNumberOfShards(uint64_t numberOfShards);

  /// @brief Set replicationFactor to the graph definition
  void setReplicationFactor(uint64_t setReplicationFactor);

  /// @brief Set isSmart to the graph definition
  void setSmartState(bool state);

  /// @brief Set rev to the graph definition
  void setRev(std::string&& rev);

/////////////////////////////////////////////////////////////////////////////////
//
// SECTION: Variables
//
/////////////////////////////////////////////////////////////////////////////////
 private:
  /// @brief name of this graph
  std::string const _graphName;

  /// @brief the names of all vertexCollections
  std::unordered_set<std::string> _vertexColls;

  /// @brief the names of all orphanCollections
  std::set<std::string> _orphanColls;

  /// @brief the names of all edgeCollections
  std::set<std::string> _edgeColls;

  /// @brief edge definition names of this graph, ordered as in the database
  std::vector<std::string> _edgeDefsNames;

  /// @brief edge definitions of this graph
  std::map<std::string, EdgeDefinition> _edgeDefs;

  /// @brief state if smart graph enabled
  bool _isSmart;

  /// @brief number of shards of this graph
  uint64_t _numberOfShards;

  /// @brief replication factor of this graph
  uint64_t _replicationFactor;

  /// @brief revision of this graph
  std::string _rev;
};

// helper functions
template<class T, class C>
void setUnion(std::set<T> &set, C const &container) {
  for(auto const& it : container) {
    set.insert(it);
  }
}

template<class T, class C>
void setMinus(std::set<T> &set, C const &container) {
  for(auto const& it : container) {
    set.erase(it);
  }
}

}  // namespace graph
}  // namespace arangodb

#endif  // ARANGOD_GRAPH_GRAPH_H
