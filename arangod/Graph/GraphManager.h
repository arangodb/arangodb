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
#include "Cluster/ResultT.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationResult.h"
#include "Graph/Graph.h"

namespace arangodb {
namespace graph {

class GraphManager {
 private:
  std::shared_ptr<transaction::Context> _ctx;

  std::shared_ptr<transaction::Context>& ctx() { return _ctx; };

  std::shared_ptr<transaction::Context> const& ctx() const { return _ctx; };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find or create vertex collection by name
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult findOrCreateVertexCollectionByName(const std::string& name,
                                          bool waitForSync, VPackSlice options);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find or create collection by name and type
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult createCollection(std::string const& name, TRI_col_type_e colType,
                          bool waitForSync, VPackSlice options);

 public:
  GraphManager() = delete;

  explicit GraphManager(std::shared_ptr<transaction::Context> ctx_)
      : _ctx(std::move(ctx_)) {}

  OperationResult readGraphs(velocypack::Builder& builder,
                             arangodb::aql::QueryPart queryPart) const;

  OperationResult readGraphKeys(velocypack::Builder& builder,
                                arangodb::aql::QueryPart queryPart) const;

  OperationResult readGraphByQuery(velocypack::Builder& builder,
                                   arangodb::aql::QueryPart queryPart,
                                   std::string queryStr) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find and return a collections if available
  ////////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<LogicalCollection> getCollectionByName(
      const TRI_vocbase_t& vocbase, std::string const& name);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief checks wheter a graph exists or not
  ////////////////////////////////////////////////////////////////////////////////
  bool graphExists(std::string graphName) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create a graph
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult createGraph(VPackSlice document, bool waitForSync);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find or create collections by EdgeDefinitions
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult findOrCreateCollectionsByEdgeDefinitions(
      VPackSlice edgeDefinitions, bool waitForSync, VPackSlice options);
  OperationResult findOrCreateCollectionsByEdgeDefinitions(
    std::map<std::string, EdgeDefinition> const& edgeDefinitions, bool waitForSync,
      VPackSlice options);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create a vertex collection
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult createVertexCollection(std::string const& name,
                                         bool waitForSync, VPackSlice options);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create an edge collection
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult createEdgeCollection(std::string const& name,
                                       bool waitForSync, VPackSlice options);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief assert that the edge definition is valid, that there are no
  /// duplicate edge definitions in the argument, and that there are no
  /// mismatching edge definitions in existing graphs.
  ////////////////////////////////////////////////////////////////////////////////
  Result assertFeasibleEdgeDefinitions(VPackSlice edgeDefinitionsSlice) const;

  /// @brief rename a collection used in an edge definition
  bool renameGraphCollection(std::string oldName, std::string newName);

  /// @brief check if the edge definitions conflicts with one in an existing
  /// graph
  Result checkForEdgeDefinitionConflicts(
    std::unordered_map<std::string, arangodb::graph::EdgeDefinition> const&
    edgeDefinitions) const;

  /// @brief check if the edge definition conflicts with one in an existing
  /// graph
  Result checkForEdgeDefinitionConflicts(
    arangodb::graph::EdgeDefinition const& edgeDefinition) const;

  /// @brief Remove a graph and optional all connected collections
  OperationResult removeGraph(Graph const& graph, bool waitForSync,
                              bool dropCollections);

  OperationResult pushCollectionIfMayBeDropped(
      const std::string& colName, const std::string& graphName,
      std::unordered_set<std::string>& toBeRemoved);

  bool collectionExists(std::string const& collection) const;

 private:

  Result checkCreateGraphPermissions(
      std::string const& graphName,
      std::map<std::string, EdgeDefinition> const& edgeDefinitions,
      std::set<std::string> const& orphanCollections) const;

  Result checkDropGraphPermissions(
      Graph const& graph,
      std::unordered_set<std::string> const& followersToBeRemoved,
      std::unordered_set<std::string> const& leadersToBeRemoved);
};
}  // namespace graph
}  // namespace arangodb

#endif  // ARANGOD_GRAPH_GRAPHMANAGER_H
