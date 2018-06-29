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
#include "Graph.h"

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

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find and return a collections if available
  ////////////////////////////////////////////////////////////////////////////////
  std::shared_ptr<LogicalCollection> getCollectionByName(
      const TRI_vocbase_t& vocbase, std::string const& name) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create a graph
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult createGraph(VPackSlice document, bool waitForSync);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find or create collections by EdgeDefinitions
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult findOrCreateCollectionsByEdgeDefinitions(
      VPackSlice edgeDefinition, bool waitForSync, VPackSlice options);

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

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief check that the edge definition is valid and doesn't conflict with one
  /// in an existing graph
  ////////////////////////////////////////////////////////////////////////////////
  Result assertFeasibleEdgeDefinition(std::string const& edgeDefinitionName,
                                      VPackSlice edgeDefinitionSlice);

  /// @brief check if the edge definitions conflicts with one in an existing
  /// graph
  Result checkForEdgeDefinitionConflicts(
    std::unordered_map<std::string, arangodb::graph::EdgeDefinition> const&
    edgeDefinitions) const;

  /// @brief check if the edge definition conflicts with one in an existing
  /// graph
  Result checkForEdgeDefinitionConflicts(
    arangodb::graph::EdgeDefinition const& edgeDefinition) const;

};
}  // namespace graph
}  // namespace arangodb

#endif  // ARANGOD_GRAPH_GRAPHMANAGER_H
