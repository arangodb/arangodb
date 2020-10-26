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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GRAPH_GRAPHMANAGER_H
#define ARANGOD_GRAPH_GRAPHMANAGER_H

#include <velocypack/Buffer.h>
#include <chrono>
#include <utility>

#include "Aql/Query.h"
#include "Aql/VariableGenerator.h"
#include "Basics/ReadWriteLock.h"
#include "Cluster/ClusterInfo.h"
#include "Basics/ResultT.h"
#include "Graph/Graph.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationResult.h"

namespace arangodb {
namespace graph {

class GraphManager {
 private:
  TRI_vocbase_t& _vocbase;
  
  std::shared_ptr<transaction::Context> ctx() const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find or create vertex collection by name
  ////////////////////////////////////////////////////////////////////////////////
  Result findOrCreateVertexCollectionByName(const std::string& name,
                                            bool waitForSync, VPackSlice options);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find or create collection by name and type
  ////////////////////////////////////////////////////////////////////////////////
  Result createCollection(std::string const& name, TRI_col_type_e colType,
                          bool waitForSync, VPackSlice options);

 public:
  explicit GraphManager(TRI_vocbase_t& vocbase) : _vocbase(vocbase) {}

  Result readGraphs(velocypack::Builder& builder) const;

  Result readGraphKeys(velocypack::Builder& builder) const;

  Result readGraphByQuery(velocypack::Builder& builder, std::string const& queryStr) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find and return a collections if available
  ////////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<LogicalCollection> getCollectionByName(const TRI_vocbase_t& vocbase,
                                                                std::string const& name);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief checks wheter a graph exists or not
  ////////////////////////////////////////////////////////////////////////////////
  bool graphExists(std::string const& graphName) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief lookup a graph by name
  ////////////////////////////////////////////////////////////////////////////////
  ResultT<std::unique_ptr<Graph>> lookupGraphByName(std::string const& name) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create a graph
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult createGraph(VPackSlice document, bool waitForSync) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find or create collections by EdgeDefinitions
  ////////////////////////////////////////////////////////////////////////////////
  Result findOrCreateCollectionsByEdgeDefinitions(Graph const& graph,
                                                  std::map<std::string, EdgeDefinition> const& edgeDefinitions,
                                                  bool waitForSync, VPackSlice options);

  Result findOrCreateCollectionsByEdgeDefinition(Graph const& graph,
                                                 EdgeDefinition const& edgeDefinition,
                                                 bool waitForSync, VPackSlice options);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create a vertex collection
  ////////////////////////////////////////////////////////////////////////////////
  Result createVertexCollection(std::string const& name, bool waitForSync, VPackSlice options);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create an edge collection
  ////////////////////////////////////////////////////////////////////////////////
  Result createEdgeCollection(std::string const& name, bool waitForSync, VPackSlice options);

  /// @brief rename a collection used in an edge definition
  bool renameGraphCollection(std::string const& oldName, std::string const& newName);

  /// @brief check if the edge definitions conflicts with one in an existing
  /// graph
  Result checkForEdgeDefinitionConflicts(std::map<std::string, arangodb::graph::EdgeDefinition> const& edgeDefinitions,
                                         std::string const& graphName) const;

  /// @brief check if the edge definition conflicts with one in an existing
  /// graph
  Result checkForEdgeDefinitionConflicts(arangodb::graph::EdgeDefinition const& edgeDefinition,
                                         std::string const& graphName) const;

  /// @brief Remove a graph and optional all connected collections
  OperationResult removeGraph(Graph const& graph, bool waitForSync, bool dropCollections);

  Result pushCollectionIfMayBeDropped(const std::string& colName, const std::string& graphName,
                                      std::unordered_set<std::string>& toBeRemoved);

  bool collectionExists(std::string const& collection) const;

  /**
   * @brief Helper function to make sure all collections required
   *        for this graph are created or exist.
   *        Will fail if the collections cannot be created, or
   *        if they have a non-compatible sharding in SmartGraph-case.
   *
   * @param graph The graph information used to make sure all collections exist
   *
   * @return Either OK or an error.
   */
  Result ensureCollections(Graph* graph, bool waitForSync) const;

  /// @brief check if only SatelliteCollections are used
  bool onlySatellitesUsed(Graph const* graph) const;

  /**
   * @brief Store the given graph
   *
   * @param graph The graph to store
   * @param waitForSync Wait for Collection to sync
   * @param isUpdate If it is an update on existing graph or a new one.
   *
   * @return The result of the insrt transaction or Error.
   */
  OperationResult storeGraph(Graph const& graph, bool waitForSync, bool isUpdate) const;

  /**
   * @brief Apply callback on all graphs. The callback
   * may take over ownership of the unique_ptr otherwise
   * it will be deleted after the callback.
   * If the callback returns an error the iteration will
   * stop and return this error itself.
   *
   * @param callback The callback to be applied on all graphs
   *
   * @return The first failed callback, a general loading error or
   * TRI_ERROR_NO_ERROR.
   */
  Result applyOnAllGraphs(std::function<Result(std::unique_ptr<Graph>)> const& callback) const;

 private:
#ifdef USE_ENTERPRISE
  Result ensureEnterpriseCollectionSharding(Graph const* graph, bool waitForSync,
                                            std::unordered_set<std::string>& documentCollections) const;
  Result ensureSmartCollectionSharding(Graph const* graph, bool waitForSync,
                                       std::unordered_set<std::string>& documentCollections) const;
  Result ensureSatelliteCollectionSharding(Graph const* graph, bool waitForSync,
                                           std::unordered_set<std::string>& documentCollections) const;
#endif

  /**
   * @brief Create a new in memory graph object from the given input.
   *        This graph object does not create collections and does
   *        not check them. It cannot be used to access any kind
   *        of data. In order to get a "usable" Graph object use
   *        lookup by name.
   *
   * @param input The slice containing the graph data.
   *
   * @return A temporary Graph object
   */
  ResultT<std::unique_ptr<Graph>> buildGraphFromInput(std::string const& graphName,
                                                      arangodb::velocypack::Slice input) const;

  Result checkCreateGraphPermissions(Graph const* graph) const;

  Result checkDropGraphPermissions(Graph const& graph,
                                   std::unordered_set<std::string> const& followersToBeRemoved,
                                   std::unordered_set<std::string> const& leadersToBeRemoved);
};
}  // namespace graph
}  // namespace arangodb

#endif  // ARANGOD_GRAPH_GRAPHMANAGER_H
