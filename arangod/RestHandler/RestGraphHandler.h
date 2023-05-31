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

  arangodb::Result executeGharial();

  // /_api/gharial
  arangodb::Result graphsAction();

  // /_api/gharial/{graph-name}
  arangodb::Result graphAction(graph::Graph& graph);

  // /_api/gharial/{graph-name}/vertex
  arangodb::Result vertexSetsAction(graph::Graph& graph);

  // /_api/gharial/{graph-name}/edge
  arangodb::Result edgeSetsAction(graph::Graph& graph);

  // /_api/gharial/{graph-name}/vertex/{collection-name}
  arangodb::Result vertexSetAction(graph::Graph& graph,
                                   std::string const& vertexCollectionName);

  // /_api/gharial/{graph-name}/edge/{definition-name}
  arangodb::Result edgeSetAction(graph::Graph& graph,
                                 std::string const& edgeDefinitionName);

  // /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  arangodb::Result vertexAction(graph::Graph& graph,
                                std::string const& vertexCollectionName,
                                std::string const& vertexKey);

  // /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  arangodb::Result edgeAction(graph::Graph& graph,
                              std::string const& edgeDefinitionName,
                              std::string const& edgeKey);

  // GET /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  void vertexActionRead(graph::Graph& graph, std::string const& collectionName,
                        std::string const& key);

  // DELETE /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  arangodb::Result vertexActionRemove(graph::Graph& graph,
                                      std::string const& collectionName,
                                      std::string const& key);

  // PATCH /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  arangodb::Result vertexActionUpdate(graph::Graph& graph,
                                      std::string const& collectionName,
                                      std::string const& key);

  // PUT /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  arangodb::Result vertexActionReplace(graph::Graph& graph,
                                       std::string const& collectionName,
                                       std::string const& key);

  // POST /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  arangodb::Result vertexActionCreate(graph::Graph& graph,
                                      std::string const& collectionName);

  // GET /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  void edgeActionRead(graph::Graph& graph, std::string const& definitionName,
                      std::string const& key);

  // DELETE /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  arangodb::Result edgeActionRemove(graph::Graph& graph,
                                    std::string const& definitionName,
                                    std::string const& key);

  // POST /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  arangodb::Result edgeActionCreate(graph::Graph& graph,
                                    std::string const& definitionName);

  // PATCH /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  arangodb::Result edgeActionUpdate(graph::Graph& graph,
                                    std::string const& collectionName,
                                    std::string const& key);

  // PUT /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  arangodb::Result edgeActionReplace(graph::Graph& graph,
                                     std::string const& collectionName,
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

  Result vertexModify(graph::Graph& graph, std::string const& collectionName,
                      std::string const& key, bool isPatch);

  Result vertexCreate(graph::Graph& graph, std::string const& collectionName);

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

  Result edgeModify(graph::Graph& graph, std::string const& collectionName,
                    std::string const& key, bool isPatch);

  Result edgeCreate(graph::Graph& graph, std::string const& collectionName);

  Result documentModify(graph::Graph& graph, std::string const& collectionName,
                        std::string const& key, bool isPatch,
                        TRI_col_type_e colType);

  Result documentCreate(graph::Graph& graph, std::string const& collectionName,
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
  Result editEdgeDefinition(graph::Graph& graph,
                            std::string const& edgeDefinitionName);

  // DELETE /_api/gharial/{graph-name}/edge/{definition-name}
  Result removeEdgeDefinition(graph::Graph& graph,
                              std::string const& edgeDefinitionName);

  // POST /_api/gharial/{graph-name}/edge/
  Result createEdgeDefinition(graph::Graph& graph);

  // edgeDefinitionName may be omitted when action == CREATE
  Result modifyEdgeDefinition(graph::Graph& graph, EdgeDefinitionAction action,
                              std::string edgeDefinitionName = {});

  Result modifyVertexDefinition(graph::Graph& graph,
                                VertexDefinitionAction action,
                                std::string vertexDefinitionName);

  std::optional<RevisionId> handleRevision() const;

 private:
  graph::GraphManager _graphManager;
};
}  // namespace arangodb
