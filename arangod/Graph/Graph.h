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

#include "Aql/VariableGenerator.h"
#include "Aql/Query.h"
#include "Basics/ReadWriteLock.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ResultT.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/Methods.h"
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

  /// @brief validate the structure of edgeDefinition, i.e.
  /// that it contains the correct attributes, and that they contain the correct
  /// types of values.
  static Result ValidateEdgeDefinition(const velocypack::Slice& edgeDefinition);
  static Result ValidateOrphanCollection(const velocypack::Slice& orphanDefinition);
  static std::shared_ptr<velocypack::Buffer<uint8_t>> SortEdgeDefinition(const velocypack::Slice& edgeDefinition);

 public:
  /// @brief get the cids of all vertexCollections
  std::unordered_set<std::string> const& vertexCollections() const;

  /// @brief get the cids of all orphanCollections
  std::unordered_set<std::string> const& orphanCollections() const;

  /// @brief get the cids of all edgeCollections
  std::unordered_set<std::string> const& edgeCollections() const;

  /// @brief get the cids of all edgeCollections
  std::unordered_map<std::string, EdgeDefinition> const& edgeDefinitions() const;

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

  virtual void enhanceEngineInfo(velocypack::Builder&) const;

  std::ostream& operator<<(std::ostream& ostream);

 private:
  /// @brief adds one edge definition. the edge definition must not have been
  /// added before.
  Result addEdgeDefinition(velocypack::Slice const& edgeDefinition);

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

class GraphOperations {
 private:
  Graph const& _graph;
  std::shared_ptr<transaction::Context> _ctx;

  Graph const& graph() const { return _graph; };
  std::shared_ptr<transaction::Context>& ctx() { return _ctx; };

 public:
  GraphOperations() = delete;
  GraphOperations(Graph const& graph_,
                  std::shared_ptr<transaction::Context> ctx_)
      : _graph(graph_), _ctx(std::move(ctx_)) {}

  // TODO I added the complex result type for the get* methods to exactly
  // reproduce (in the RestGraphHandler) the behaviour of the similar methods
  // in the RestDocumentHandler. A simpler type, e.g. ResultT<OperationResult>,
  // would be preferable.

  /// @brief Get a single vertex document from collection, optionally check rev
  /// The return value is as follows:
  /// If trx.begin fails, the outer ResultT will contain this error Result.
  /// Otherwise, the results of both (trx.document(), trx.finish()) are
  /// returned as a pair.
  /// This is because in case of a precondition error during trx.document(),
  /// the OperationResult may still be needed.
  ResultT<std::pair<OperationResult, Result>> getVertex(
      std::string const& collectionName, std::string const& key,
      boost::optional<TRI_voc_rid_t> rev);

  /// @brief Get a single edge document from definitionName.
  /// Similar to getVertex().
  ResultT<std::pair<OperationResult, Result>> getEdge(
      const std::string& definitionName, const std::string& key,
      boost::optional<TRI_voc_rid_t> rev);

  /// @brief Remove a single edge document from definitionName.
  ResultT<std::pair<OperationResult, Result>> removeEdge(
      const std::string& definitionName, const std::string& key,
      boost::optional<TRI_voc_rid_t> rev, bool waitForSync, bool returnOld);

  /// @brief Remove a vertex and all incident edges in the graph
  ResultT<std::pair<OperationResult, Result>> removeVertex(
      const std::string& collectionName, const std::string& key,
      boost::optional<TRI_voc_rid_t> rev, bool waitForSync, bool returnOld);

  /// @brief Remove a graph and optional all connected collections
  ResultT<std::pair<OperationResult, Result>> removeGraph(bool waitForSync,
                                                          bool dropCollections);

  ResultT<std::pair<OperationResult, Result>> updateEdge(
      const std::string& definitionName, const std::string& key,
      VPackSlice document, boost::optional<TRI_voc_rid_t> rev, bool waitForSync,
      bool returnOld, bool returnNew, bool keepNull);

  ResultT<std::pair<OperationResult, Result>> replaceEdge(
      const std::string& definitionName, const std::string& key,
      VPackSlice document, boost::optional<TRI_voc_rid_t> rev, bool waitForSync,
      bool returnOld, bool returnNew, bool keepNull);

  ResultT<std::pair<OperationResult, Result>> createEdge(
      const std::string& definitionName,
      VPackSlice document, bool waitForSync, bool returnNew);

  ResultT<std::pair<OperationResult, Result>> updateVertex(
      const std::string& collectionName, const std::string& key,
      VPackSlice document, boost::optional<TRI_voc_rid_t> rev, bool waitForSync,
      bool returnOld, bool returnNew, bool keepNull);

  ResultT<std::pair<OperationResult, Result>> replaceVertex(
      const std::string& collectionName, const std::string& key,
      VPackSlice document, boost::optional<TRI_voc_rid_t> rev, bool waitForSync,
      bool returnOld, bool returnNew, bool keepNull);

  ResultT<std::pair<OperationResult, Result>> createVertex(
      const std::string& collectionName,
      VPackSlice document, bool waitForSync, bool returnNew);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief add an orphan to collection to an existing graph
  ////////////////////////////////////////////////////////////////////////////////
  ResultT<std::pair<OperationResult, Result>> addOrphanCollection(
      VPackSlice document, bool waitForSync);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove an orphan collection from an existing graph
  ////////////////////////////////////////////////////////////////////////////////
  ResultT<std::pair<OperationResult, Result>> eraseOrphanCollection(
      bool waitForSync, std::string collectionName, bool dropCollection);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create a new edge definition in an existing graph
  ////////////////////////////////////////////////////////////////////////////////
  ResultT<std::pair<OperationResult, Result>> addEdgeDefinition(
      VPackSlice edgeDefinition, bool waitForSync);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove an edge definition from an existing graph
  ////////////////////////////////////////////////////////////////////////////////
  ResultT<std::pair<OperationResult, Result>> eraseEdgeDefinition(
      bool waitForSync, std::string edgeDefinitionName, bool dropCollection);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create edge definition in an existing graph
  ////////////////////////////////////////////////////////////////////////////////
  ResultT<std::pair<OperationResult, Result>> editEdgeDefinition(
      VPackSlice edgeDefinition, bool waitForSync, std::string edgeDefinitionName);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief change the edge definition for a specified graph
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult changeEdgeDefinitionsForGraph(
      VPackSlice graph, VPackSlice edgeDefinition,
      std::unordered_set<std::string> newCollections,
      std::unordered_set<std::string> possibleOrphans, bool waitForSync,
      std::string thisGraphName, transaction::Methods& trx);

  void checkIfCollectionMayBeDropped(std::string colName, std::string graphName,
      std::vector<std::string>& toBeRemoved);

  // TODO should these operations always fetch a new definition from the
  // database?
  void readGraph(VPackBuilder& builder);
  void readEdges(VPackBuilder& builder);
  void readVertices(VPackBuilder& builder);

 private:
  using VPackBufferPtr = std::shared_ptr<velocypack::Buffer<uint8_t>>;

  ResultT<std::pair<OperationResult, Result>> getDocument(
      std::string const& collectionName, const std::string& key,
      boost::optional<TRI_voc_rid_t> rev);

  VPackBufferPtr _getSearchSlice(const std::string& key,
                                 boost::optional<TRI_voc_rid_t>& rev) const;

  ResultT<std::pair<OperationResult, Result>> modifyDocument(
      const std::string& collectionName, const std::string& key,
      VPackSlice document, bool isPatch, boost::optional<TRI_voc_rid_t> rev,
      bool waitForSync, bool returnOld, bool returnNew, bool keepNull);

  ResultT<std::pair<OperationResult, Result>> createDocument(
      transaction::Methods* trx, const std::string& collectionName,
      VPackSlice document, bool waitForSync, bool returnNew);

    void checkEdgeCollectionAvailability(std::string edgeDefinitionName);
    void checkVertexCollectionAvailability(std::string VertexDefinitionName);
};

class GraphManager {
  private:
   std::shared_ptr<transaction::Context> _ctx;
   std::shared_ptr<transaction::Context>& ctx() { return _ctx; };

   ////////////////////////////////////////////////////////////////////////////////
   /// @brief check for duplicate definitions within edge definitions
   ////////////////////////////////////////////////////////////////////////////////
   void checkDuplicateEdgeDefinitions(VPackSlice edgeDefinition);

   ////////////////////////////////////////////////////////////////////////////////
   /// @brief find or create vertex collection by name
   ////////////////////////////////////////////////////////////////////////////////
   void findOrCreateVertexCollectionByName(std::string&& name);
   
   ////////////////////////////////////////////////////////////////////////////////
   /// @brief find or create collection by name and type
   ////////////////////////////////////////////////////////////////////////////////
   void createCollection(std::string const& name, TRI_col_type_e colType);

   ////////////////////////////////////////////////////////////////////////////////
   /// @brief create an edge collection
   ////////////////////////////////////////////////////////////////////////////////
   void createEdgeCollection(std::string const& name);
   
   ////////////////////////////////////////////////////////////////////////////////
   /// @brief get all vertex collections
   ////////////////////////////////////////////////////////////////////////////////
   void getVertexCollections(std::vector<std::string>& collections, VPackSlice document);

   ////////////////////////////////////////////////////////////////////////////////
   /// @brief get all edge collections
   ////////////////////////////////////////////////////////////////////////////////
   void getEdgeCollections(std::vector<std::string>& collections, VPackSlice edgeDefinitions);

  public:
   GraphManager() = delete;
   GraphManager(std::shared_ptr<transaction::Context> ctx_)
        : _ctx(std::move(ctx_)) {}
   void readGraphs(velocypack::Builder& builder, arangodb::aql::QueryPart QueryPart);

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief find and return a collections if available
    ////////////////////////////////////////////////////////////////////////////////
    std::shared_ptr<LogicalCollection> getCollectionByName(
        TRI_vocbase_t& vocbase,
        std::string const& name);


   ////////////////////////////////////////////////////////////////////////////////
   /// @brief create a graph
   ////////////////////////////////////////////////////////////////////////////////
   ResultT<std::pair<OperationResult, Result>> createGraph(VPackSlice document,
       bool waitForSync);

   ////////////////////////////////////////////////////////////////////////////////
   /// @brief find or create collections by EdgeDefinitions
   ////////////////////////////////////////////////////////////////////////////////
   void findOrCreateCollectionsByEdgeDefinitions(VPackSlice edgeDefinition);

   ////////////////////////////////////////////////////////////////////////////////
   /// @brief create a vertex collection
   ////////////////////////////////////////////////////////////////////////////////
   void createVertexCollection(std::string const& name);

   ////////////////////////////////////////////////////////////////////////////////
   /// @brief remove a vertex collection
   ////////////////////////////////////////////////////////////////////////////////
   void removeVertexCollection(
     std::string const& collectionName,
     bool dropCollection
   );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief check if the edge definition conflicts with one in an existing graph
  ////////////////////////////////////////////////////////////////////////////////
   Result checkForEdgeDefinitionConflicts(std::string const& edgeDefinitionName,
                                          VPackSlice edgeDefinition);
};


class GraphCache {
 public:
  // save now() along with the graph
  using EntryType = std::pair<std::chrono::steady_clock::time_point,
                              std::shared_ptr<const Graph>>;
  using CacheType = std::unordered_map<std::string, EntryType>;

  // TODO HEIKO: DB is missing
  const std::shared_ptr<const Graph> getGraph(
      std::shared_ptr<transaction::Context> ctx, std::string const& name,
      std::chrono::seconds maxAge = std::chrono::seconds(60));

 private:
  basics::ReadWriteLock _lock;
  CacheType _cache;
};
}
}

#endif  // ARANGOD_GRAPH_GRAPH_H
