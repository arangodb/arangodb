////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <optional>

#include "Actions/RestActionHandler.h"
#include "Graph/GraphManager.h"
#include "RestHandler/RestBaseHandler.h"

namespace arangodb {

namespace graph {
class Graph;
}

class RestGraphHandler : public arangodb::RestVocbaseBaseHandler {
 private:
  enum class GraphProperty { VERTICES, EDGES };

  enum class EdgeDefinitionAction { CREATE, EDIT, REMOVE };

  enum class VertexDefinitionAction { CREATE, REMOVE };

 public:
  RestGraphHandler(ArangodServer& server, GeneralRequest* request,
                   GeneralResponse* response);

  ~RestGraphHandler() = default;

  char const* name() const override final { return "RestGraphHandler"; }

  RestStatus execute() override;

  RequestLane lane() const override;

 private:
  Result returnError(ErrorCode errorNumber);

  Result returnError(ErrorCode errorNumber, std::string_view message);

  futures::Future<arangodb::Result> executeGharial();

  // /_api/gharial
  arangodb::Result graphsAction();

  // /_api/gharial/{graph-name}
  arangodb::Result graphAction(graph::Graph& graph);

  // /_api/gharial/{graph-name}/vertex
  futures::Future<arangodb::Result> vertexSetsAction(graph::Graph& graph);

  // /_api/gharial/{graph-name}/edge
  futures::Future<arangodb::Result> edgeSetsAction(graph::Graph& graph);

  // /_api/gharial/{graph-name}/vertex/{collection-name}
  futures::Future<arangodb::Result> vertexSetAction(
      graph::Graph& graph, std::string const& vertexCollectionName);

  // /_api/gharial/{graph-name}/edge/{definition-name}
  futures::Future<arangodb::Result> edgeSetAction(
      graph::Graph& graph, std::string const& edgeDefinitionName);

  // /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  futures::Future<arangodb::Result> vertexAction(
      graph::Graph& graph, std::string const& vertexCollectionName,
      std::string const& vertexKey);

  // /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  futures::Future<arangodb::Result> edgeAction(
      graph::Graph& graph, std::string const& edgeDefinitionName,
      std::string const& edgeKey);

  // GET /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  [[nodiscard]] futures::Future<futures::Unit> vertexActionRead(
      graph::Graph& graph, std::string const& collectionName,
      std::string const& key);

  // DELETE /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  futures::Future<arangodb::Result> vertexActionRemove(
      graph::Graph& graph, std::string const& collectionName,
      std::string const& key);

  // PATCH /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  futures::Future<arangodb::Result> vertexActionUpdate(
      graph::Graph& graph, std::string const& collectionName,
      std::string const& key);

  // PUT /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  futures::Future<arangodb::Result> vertexActionReplace(
      graph::Graph& graph, std::string const& collectionName,
      std::string const& key);

  // POST /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  futures::Future<arangodb::Result> vertexActionCreate(
      graph::Graph& graph, std::string const& collectionName);

  // GET /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  [[nodiscard]] futures::Future<futures::Unit> edgeActionRead(
      graph::Graph& graph, std::string const& definitionName,
      std::string const& key);

  // DELETE /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  futures::Future<arangodb::Result> edgeActionRemove(
      graph::Graph& graph, std::string const& definitionName,
      std::string const& key);

  // POST /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  futures::Future<arangodb::Result> edgeActionCreate(
      graph::Graph& graph, std::string const& definitionName);

  // PATCH /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  futures::Future<arangodb::Result> edgeActionUpdate(
      graph::Graph& graph, std::string const& collectionName,
      std::string const& key);

  // PUT /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  futures::Future<arangodb::Result> edgeActionReplace(
      graph::Graph& graph, std::string const& collectionName,
      std::string const& key);

  std::unique_ptr<graph::Graph> getGraph(std::string const& graphName);

  void generateVertexRead(VPackSlice vertex, VPackOptions const& options);

  void generateEdgeRead(VPackSlice edge, const VPackOptions& options);

  void generateRemoved(bool removed, bool wasSynchronous, VPackSlice old,
                       VPackOptions const& options);

  void generateGraphRemoved(bool removed, bool wasSynchronous,
                            VPackOptions const& options);

  void generateCreatedGraphConfig(bool wasSynchronous, VPackSlice slice,
                                  VPackOptions const& options);

  void generateCreatedEdgeDefinition(bool wasSynchronous, VPackSlice slice,
                                     VPackOptions const& options);

  void generateGraphConfig(VPackSlice slice, VPackOptions const& options);

  // TODO maybe cleanup the generate* zoo a little?
  void generateResultWithField(std::string const& key, VPackSlice value,
                               VPackOptions const& options);

  void generateResultMergedWithObject(VPackSlice obj,
                                      VPackOptions const& options);

  void addEtagHeader(velocypack::Slice slice);

  futures::Future<arangodb::Result> vertexModify(
      graph::Graph& graph, std::string const& collectionName,
      std::string const& key, bool isPatch);

  futures::Future<arangodb::Result> vertexCreate(
      graph::Graph& graph, std::string const& collectionName);

  void generateVertexModified(bool wasSynchronous, VPackSlice resultSlice,
                              const velocypack::Options& options);

  void generateVertexCreated(bool wasSynchronous, VPackSlice resultSlice,
                             const velocypack::Options& options);

  void generateModified(TRI_col_type_e colType, bool wasSynchronous,
                        VPackSlice resultSlice,
                        const velocypack::Options& options);

  void generateCreated(TRI_col_type_e colType, bool wasSynchronous,
                       VPackSlice resultSlice,
                       const velocypack::Options& options);

  futures::Future<arangodb::Result> edgeModify(
      graph::Graph& graph, std::string const& collectionName,
      std::string const& key, bool isPatch);

  futures::Future<arangodb::Result> edgeCreate(
      graph::Graph& graph, std::string const& collectionName);

  futures::Future<arangodb::Result> documentModify(
      graph::Graph& graph, std::string const& collectionName,
      std::string const& key, bool isPatch, TRI_col_type_e colType);

  futures::Future<arangodb::Result> documentCreate(
      graph::Graph& graph, std::string const& collectionName,
      TRI_col_type_e colType);

  Result graphActionReadGraphConfig(graph::Graph const& graph);

  Result graphActionRemoveGraph(graph::Graph const& graph);

  Result graphActionCreateGraph();
  Result graphActionReadGraphs();

  Result graphActionReadConfig(graph::Graph const& graph,
                               TRI_col_type_e colType, GraphProperty property);

  void generateEdgeModified(bool wasSynchronous, VPackSlice resultSlice,
                            velocypack::Options const& options);

  void generateEdgeCreated(bool wasSynchronous, VPackSlice resultSlice,
                           velocypack::Options const& options);

  // edges
  // PATCH /_api/gharial/{graph-name}/edge/{definition-name}
  futures::Future<arangodb::Result> editEdgeDefinition(
      graph::Graph& graph, std::string const& edgeDefinitionName);

  // DELETE /_api/gharial/{graph-name}/edge/{definition-name}
  futures::Future<arangodb::Result> removeEdgeDefinition(
      graph::Graph& graph, std::string const& edgeDefinitionName);

  // POST /_api/gharial/{graph-name}/edge/
  futures::Future<arangodb::Result> createEdgeDefinition(graph::Graph& graph);

  // edgeDefinitionName may be omitted when action == CREATE
  futures::Future<arangodb::Result> modifyEdgeDefinition(
      graph::Graph& graph, EdgeDefinitionAction action,
      std::string edgeDefinitionName = {});

  futures::Future<arangodb::Result> modifyVertexDefinition(
      graph::Graph& graph, VertexDefinitionAction action,
      std::string vertexDefinitionName);

  std::optional<RevisionId> handleRevision() const;

 private:
  graph::GraphManager _graphManager;
};
}  // namespace arangodb
