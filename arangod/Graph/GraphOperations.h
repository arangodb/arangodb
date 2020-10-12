////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#ifndef ARANGOD_GRAPH_GRAPHOPERATIONS_H
#define ARANGOD_GRAPH_GRAPHOPERATIONS_H

#include <velocypack/Buffer.h>
#include <velocypack/velocypack-aliases.h>
#include <chrono>
#include <utility>

#include "Aql/Query.h"
#include "Aql/VariableGenerator.h"
#include "Auth/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Cluster/ClusterInfo.h"
#include "Basics/ResultT.h"
#include "Graph/Graph.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationResult.h"

namespace arangodb {
namespace graph {

// TODO rename to GraphMethods

class GraphOperations {
 private:
  Graph& _graph;
  TRI_vocbase_t& _vocbase;
  std::shared_ptr<transaction::Context> _ctx;

  Graph const& graph() const { return _graph; };
  std::shared_ptr<transaction::Context> ctx();

 public:
  GraphOperations() = delete;
  GraphOperations(Graph& graph_, TRI_vocbase_t& vocbase,
                  std::shared_ptr<transaction::Context> const& ctx = nullptr)
      : _graph(graph_), _vocbase(vocbase), _ctx(ctx) {}

  // TODO I added the complex result type for the get* methods to exactly
  // reproduce (in the RestGraphHandler) the behavior of the similar methods
  // in the RestDocumentHandler. A simpler type, e.g. ResultT<OperationResult>,
  // would be preferable.

  /// @brief Get a single vertex document from collection, optionally check rev
  /// The return value is as follows:
  /// If trx.begin fails, the outer ResultT will contain this error Result.
  /// Otherwise, the results of both (trx.document(), trx.finish()) are
  /// returned as a pair.
  /// This is because in case of a precondition error during trx.document(),
  /// the OperationResult may still be needed.
  OperationResult getVertex(std::string const& collectionName,
                            std::string const& key, std::optional<RevisionId> rev);

  /// @brief Get a single edge document from definitionName.
  /// Similar to getVertex().
  OperationResult getEdge(const std::string& definitionName,
                          const std::string& key, std::optional<RevisionId> rev);

  /// @brief Remove a single edge document from definitionName.
  OperationResult removeEdge(const std::string& definitionName,
                             const std::string& key, std::optional<RevisionId> rev,
                             bool waitForSync, bool returnOld);

  /// @brief Remove a vertex and all incident edges in the graph
  OperationResult removeVertex(const std::string& collectionName,
                               const std::string& key, std::optional<RevisionId> rev,
                               bool waitForSync, bool returnOld);

  /// @brief Remove an edge or vertex and all incident edges in the graph
  OperationResult removeEdgeOrVertex(const std::string& collectionName,
                                     const std::string& key, std::optional<RevisionId> rev,
                                     bool waitForSync, bool returnOld);

  OperationResult updateEdge(const std::string& definitionName,
                             const std::string& key, VPackSlice document,
                             std::optional<RevisionId> rev, bool waitForSync,
                             bool returnOld, bool returnNew, bool keepNull);

  OperationResult replaceEdge(const std::string& definitionName,
                              const std::string& key, VPackSlice document,
                              std::optional<RevisionId> rev, bool waitForSync,
                              bool returnOld, bool returnNew, bool keepNull);

  OperationResult createEdge(const std::string& definitionName, VPackSlice document,
                             bool waitForSync, bool returnNew);

  // @brief This function is a helper function which is setting up a transaction
  // and calls validateEdgeVertices and validateEdgeContent methods.
  std::pair<OperationResult, std::unique_ptr<transaction::Methods>> validateEdge(
      const std::string& definitionName, const VPackSlice& document,
      bool waitForSync, bool isUpdate);

  // @brief This function is checking whether the given _from and _to vertex documents are available or not
  OperationResult validateEdgeVertices(const std::string& fromCollectionName,
                                       const std::string& fromCollectionKey,
                                       const std::string& toCollectionName,
                                       const std::string& toCollectionKey,
                                       transaction::Methods& trx);

  // @brief This function is checking whether the given document defines _from and _to attributes or not
  // and checks if they are correct or invalid if they are available.
  std::pair<OperationResult, bool> validateEdgeContent(
      const VPackSlice& document, std::string& fromCollectionName, std::string& fromCollectionKey,
      std::string& toCollectionName, std::string& toCollectionKey, bool isUpdate);

  OperationResult updateVertex(const std::string& collectionName,
                               const std::string& key, VPackSlice document,
                               std::optional<RevisionId> rev, bool waitForSync,
                               bool returnOld, bool returnNew, bool keepNull);

  OperationResult replaceVertex(const std::string& collectionName,
                                const std::string& key, VPackSlice document,
                                std::optional<RevisionId> rev, bool waitForSync,
                                bool returnOld, bool returnNew, bool keepNull);

  OperationResult createVertex(const std::string& collectionName, VPackSlice document,
                               bool waitForSync, bool returnNew);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief add an orphan to collection to an existing graph
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult addOrphanCollection(VPackSlice document, bool waitForSync,
                                      bool createCollection);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove an orphan collection from an existing graph
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult eraseOrphanCollection(bool waitForSync, std::string const& collectionName,
                                        bool dropCollection);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create a new edge definition in an existing graph
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult addEdgeDefinition(VPackSlice edgeDefinition, bool waitForSync);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove an edge definition from an existing graph
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult eraseEdgeDefinition(bool waitForSync, std::string const& edgeDefinitionName,
                                      bool dropCollection);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create edge definition in an existing graph
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult editEdgeDefinition(VPackSlice edgeDefinitionSlice, bool waitForSync,
                                     std::string const& edgeDefinitionName);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief change the edge definition for a specified graph
  /// if the graph doesn't already contain a definition for the same edge
  /// collection, this does nothing and returns success.
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult changeEdgeDefinitionForGraph(Graph& graph, EdgeDefinition const& edgeDefinition,
                                               bool waitForSync,
                                               transaction::Methods& trx);

  void checkForUsedEdgeCollections(const Graph& graph, const std::string& collectionName,
                                   std::unordered_set<std::string>& possibleEdgeCollections);

 private:
  using VPackBufferPtr = std::shared_ptr<velocypack::Buffer<uint8_t>>;

  OperationResult getDocument(std::string const& collectionName,
                              const std::string& key, std::optional<RevisionId> rev);

  /// @brief creates a vpack { _key: key } or { _key: key, _rev: rev }
  /// (depending on whether rev is set)
  VPackBufferPtr _getSearchSlice(const std::string& key, std::optional<RevisionId>& rev) const;

  OperationResult modifyDocument(const std::string& collectionName,
                                 const std::string& key, VPackSlice document,
                                 bool isPatch, std::optional<RevisionId> rev,
                                 bool waitForSync, bool returnOld, bool returnNew,
                                 bool keepNull, transaction::Methods& trx);

  OperationResult createDocument(transaction::Methods* trx, const std::string& collectionName,
                                 VPackSlice document, bool waitForSync, bool returnNew);

  Result checkEdgeCollectionAvailability(std::string const& edgeCollectionName);
  Result checkVertexCollectionAvailability(std::string const& vertexCollectionName);

  bool hasROPermissionsFor(std::string const& collection) const;
  bool hasRWPermissionsFor(std::string const& collection) const;
  bool hasPermissionsFor(std::string const& collection, auth::Level level) const;

  Result checkEdgeDefinitionPermissions(EdgeDefinition const& edgeDefinition) const;

  bool collectionExists(std::string const& collection) const;

#ifdef USE_ENTERPRISE
  bool isUsedAsInitialCollection(std::string const& collectionName);
#endif
};
}  // namespace graph
}  // namespace arangodb

#endif  // ARANGOD_GRAPH_GRAPHOPERATIONS_H
