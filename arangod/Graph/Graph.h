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
#include <set>
#include <utility>

#include "Aql/Query.h"
#include "Aql/VariableGenerator.h"
#include "Basics/ReadWriteLock.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ResultT.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationResult.h"

struct TRI_vocbase_t;

namespace arangodb {
struct ServerDefaults;

namespace graph {

class EdgeDefinition {
 public:
  EdgeDefinition(std::string edgeCollection_, std::set<std::string>&& from_,
                 std::set<std::string>&& to_)
      : _edgeCollection(std::move(edgeCollection_)),
        _from(std::move(from_)),
        _to(std::move(to_)) {}

  std::string const& getName() const { return _edgeCollection; }
  void setName(std::string const& newName) { _edgeCollection = newName; }
  std::set<std::string> const& getFrom() const { return _from; }
  std::set<std::string> const& getTo() const { return _to; }

  /// @brief Adds the edge definition as a new object {collection, from, to}
  /// to the builder.
  void addToBuilder(velocypack::Builder& builder) const;

  bool hasFrom(std::string const& vertexCollection) const;
  bool hasTo(std::string const& vertexCollection) const;

  /// @brief validate the structure of edgeDefinition, i.e.
  /// that it contains the correct attributes, and that they contain the correct
  /// types of values.
  static Result validateEdgeDefinition(const velocypack::Slice& edgeDefinition);

  static ResultT<EdgeDefinition> createFromVelocypack(velocypack::Slice edgeDefinition);

  void toVelocyPack(velocypack::Builder&) const;

  bool operator==(EdgeDefinition const& other) const;
  bool operator!=(EdgeDefinition const& other) const;
  bool isVertexCollectionUsed(std::string const& collectionName) const;
  bool isFromVertexCollectionUsed(std::string const& collectionName) const;
  bool isToVertexCollectionUsed(std::string const& collectionName) const;

  bool renameCollection(std::string const& oldName, std::string const& newName);

 private:
  std::string _edgeCollection;
  std::set<std::string> _from;
  std::set<std::string> _to;
};

class Graph {
 public:
  /**
   * @brief Create graph from persistence.
   *
   * @param document The stored document
   *
   * @return A graph object corresponding to this document
   */
  static std::unique_ptr<Graph> fromPersistence(TRI_vocbase_t& vocbase,
                                                velocypack::Slice document);

  /**
   * @brief Create graph from user input.
   *        NOTE: This is purely in memory and will NOT persist anything.
   *
   * @param name The name of the Graph
   * @param collectionInformation Collection information about relations and
   * orphans
   * @param options The collection creation options.
   *
   * @return A graph object corresponding to the user input
   */
  static std::unique_ptr<Graph> fromUserInput(TRI_vocbase_t& vocbase, std::string&& name,
                                              velocypack::Slice collectionInformation,
                                              velocypack::Slice options);

  // Wrapper for Move constructor
  static std::unique_ptr<Graph> fromUserInput(TRI_vocbase_t& vocbase,
                                              std::string const& name,
                                              velocypack::Slice collectionInformation,
                                              velocypack::Slice options);

 protected:
  /**
   * @brief Create graph from persistence.
   *
   * @param slice The stored document
   */
  explicit Graph(velocypack::Slice const& slice, ServerDefaults const& serverDefaults);

  /**
   * @brief Create graph from user input.
   *
   * @param graphName The name of the graph
   * @param info Collection information, including relations and orphans
   * @param options The options to be used for collections
   */
  Graph(TRI_vocbase_t& vocbase, std::string&& graphName,
        velocypack::Slice const& info, velocypack::Slice const& options);

  /**
   * @brief virtual copy constructor
   */
  virtual auto clone() const -> std::unique_ptr<Graph>;

 public:
  virtual ~Graph() = default;

  [[nodiscard]] static Result validateOrphanCollection(velocypack::Slice const& orphanDefinition);

  virtual void createCollectionOptions(VPackBuilder& builder, bool waitForSync) const;

 public:
  /// @brief get the cids of all vertexCollections
  std::set<std::string> const& vertexCollections() const;

  /// @brief get the cids of all orphanCollections
  std::set<std::string> const& orphanCollections() const;

  /// @brief get the cids of all edgeCollections
  std::set<std::string> const& edgeCollections() const;

  /// @brief get the cids of all edgeCollections
  std::map<std::string, EdgeDefinition> const& edgeDefinitions() const;

  /// @brief get the cids of all edgeCollections as reference
  std::map<std::string, EdgeDefinition>& edgeDefinitions();

  bool hasEdgeCollection(std::string const& collectionName) const;
  bool hasVertexCollection(std::string const& collectionName) const;
  bool hasOrphanCollection(std::string const& collectionName) const;
  bool renameCollections(std::string const& oldName, std::string const& newName);

  std::optional<std::reference_wrapper<EdgeDefinition const>> getEdgeDefinition(
      std::string const& collectionName) const;

  virtual bool isSmart() const;
  virtual bool isDisjoint() const;
  virtual bool isSatellite() const;

  uint64_t numberOfShards() const;
  uint64_t replicationFactor() const;
  uint64_t writeConcern() const;
  std::string const id() const;
  std::string const& rev() const;

  std::string const& name() const { return _graphName; }

  /// @brief return a VelocyPack representation of the graph
  void toVelocyPack(velocypack::Builder&) const;

  /**
   * @brief Create the GraphDocument to be stored in the database.
   *
   * @param builder The builder the result should be written in. Expects an open
   * object.
   */
  virtual void toPersistence(velocypack::Builder& builder) const;

  /**
   * @brief Create the Graph Json Representation to be given to the client.
   *        Uses toPersistence, but also includes _rev and _id values and
   * encapsulates the date into a graph attribute.
   *
   * @param builder The builder the result should be written in. Expects an open
   * object.
   */
  void graphForClient(VPackBuilder& builder) const;

  /**
   * @brief Check if the collection is allowed to be used
   * within this graph
   *
   * @param col The collection
   *
   * @return TRUE if we are safe to use it.
   */
  virtual Result validateCollection(LogicalCollection& col) const;
  virtual void ensureInitial(const LogicalCollection& col);

  void edgesToVpack(VPackBuilder& builder) const;
  void verticesToVpack(VPackBuilder& builder) const;

  virtual void enhanceEngineInfo(velocypack::Builder&) const;

  /// @brief adds one edge definition. Returns an error if the edgeDefinition
  ///        is already added to this graph.
  ResultT<EdgeDefinition const*> addEdgeDefinition(velocypack::Slice const& edgeDefinitionSlice);

  /// @brief adds one edge definition. Returns an error if the edgeDefinition
  ///        is already added to this graph.
  ResultT<EdgeDefinition const*> addEdgeDefinition(EdgeDefinition const& edgeDefinition);

  /// @brief removes one edge definition. Returns an error if the edgeDefinition
  ///        is not included in this graph.
  bool removeEdgeDefinition(std::string const& edgeDefinitionName);

  /// @brief replaces one edge definition. Returns an error if the
  /// edgeDefinition
  ///        is not included in this graph.
  Result replaceEdgeDefinition(EdgeDefinition const& edgeDefinition);

  /// @brief Rebuild orphan collections. Needs to be called after every
  /// removal or change of an existing an edgeDefinition.
  void rebuildOrphans(EdgeDefinition const& oldEdgeDefinition);

  /// @brief Removes an orphan vertex collection from the graphs definition
  Result removeOrphanCollection(std::string const&);

  /// @brief Add an orphan vertex collection to this graphs definition
  Result addOrphanCollection(std::string&&);

  std::ostream& operator<<(std::ostream& ostream);

 private:
  /// @brief Parse the edgeDefinition slice and inject it into this graph
  void parseEdgeDefinitions(velocypack::Slice edgeDefs);

  /// @brief Add a vertex collection to this graphs definition
  void addVertexCollection(std::string const&);

  /// @brief Add orphanCollections to the object
  void insertOrphanCollections(velocypack::Slice arr);

  /// @brief Set numberOfShards to the graph definition
  void setNumberOfShards(uint64_t numberOfShards);

  /// @brief Set replicationFactor to the graph definition
  void setReplicationFactor(uint64_t replicationFactor);

  /// @brief Set writeConcern to the graph definition
  void setWriteConcern(uint64_t writeConcern);

  /// @brief Set rev to the graph definition
  void setRev(std::string&& rev);

  /////////////////////////////////////////////////////////////////////////////////
  //
  // SECTION: Variables
  //
  /////////////////////////////////////////////////////////////////////////////////
 protected:
  /// @brief name of this graph
  std::string const _graphName;

  /// @brief the names of all vertexCollections
  ///        This includes orphans.
  std::set<std::string> _vertexColls;

  /// @brief the names of all orphanCollections
  std::set<std::string> _orphanColls;

  /// @brief the names of all edgeCollections
  std::set<std::string> _edgeColls;

  /// @brief edge definitions of this graph
  std::map<std::string, EdgeDefinition> _edgeDefs;

  /// @brief number of shards of this graph
  uint64_t _numberOfShards;

  /// @brief replication factor of this graph
  uint64_t _replicationFactor;

  /// @brief write concern for this graph
  uint64_t _writeConcern;

  /// @brief revision of this graph
  std::string _rev;

  /// @brief whether this graph is a SatelliteGraph
  bool _isSatellite = false;
};

// helper functions
template <class T, class C>
void setUnion(std::set<T>& set, C const& container) {
  for (auto const& it : container) {
    set.insert(it);
  }
}

}  // namespace graph
}  // namespace arangodb

#endif  // ARANGOD_GRAPH_GRAPH_H
