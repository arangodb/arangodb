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

#ifndef ARANGOD_REST_HANDLER_REST_GRAPH_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_GRAPH_HANDLER_H

#include <boost/optional.hpp>

#include "Actions/RestActionHandler.h"
#include "RestHandler/RestBaseHandler.h"

namespace arangodb {

namespace graph {
class Graph;
class GraphCache;
}

namespace transaction {
class StandaloneContext;
}

class RestGraphHandler : public arangodb::RestVocbaseBaseHandler {
 private:
  graph::GraphCache& _graphCache;
  enum class GraphProperty {
    VERTICES, EDGES
  };

  enum class EdgeDefinitionAction {
    CREATE, EDIT, REMOVE
  };

  enum class VertexDefinitionAction {
    CREATE, REMOVE
  };

 public:
  RestGraphHandler(GeneralRequest* request, GeneralResponse* response,
                   graph::GraphCache* graphCache);

  ~RestGraphHandler() override = default;

  char const* name() const final { return "RestGraphHandler"; }

  RestStatus execute() override;

  RequestLane lane() const override;

 private:
  boost::optional<RestStatus> executeGharial();

  // /_api/gharial
  boost::optional<RestStatus> graphsAction();

  // /_api/gharial/{graph-name}
  boost::optional<RestStatus> graphAction(
      std::shared_ptr<const graph::Graph> graph);

  // /_api/gharial/{graph-name}/vertex
  boost::optional<RestStatus> vertexSetsAction(
      std::shared_ptr<const graph::Graph> graph);

  // /_api/gharial/{graph-name}/edge
  boost::optional<RestStatus> edgeSetsAction(
      std::shared_ptr<const graph::Graph> graph);

  // /_api/gharial/{graph-name}/vertex/{collection-name}
  boost::optional<RestStatus> vertexSetAction(
      std::shared_ptr<const graph::Graph> graph,
      const std::string& vertexCollectionName);

  // /_api/gharial/{graph-name}/edge/{definition-name}
  boost::optional<RestStatus> edgeSetAction(
      std::shared_ptr<const graph::Graph> graph,
      const std::string& edgeDefinitionName);

  // /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  boost::optional<RestStatus> vertexAction(
      std::shared_ptr<const graph::Graph> graph,
      const std::string& vertexCollectionName, const std::string& vertexKey);

  // /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  boost::optional<RestStatus> edgeAction(
      std::shared_ptr<const graph::Graph> graph,
      const std::string& edgeDefinitionName, const std::string& edgeKey);

  // GET /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  void vertexActionRead(const std::shared_ptr<const graph::Graph> graph,
                        const std::string &collectionName,
                        const std::string &key);

  // DELETE /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  Result vertexActionRemove(std::shared_ptr<const graph::Graph> graph,
                            const std::string& collectionName,
                            const std::string& key);

  // PATCH /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  Result vertexActionUpdate(std::shared_ptr<const graph::Graph> graph,
                            const std::string& collectionName,
                            const std::string& key);

  // PUT /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  Result vertexActionReplace(std::shared_ptr<const graph::Graph> graph,
                             const std::string& collectionName,
                             const std::string& key);

  // POST /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  Result vertexActionCreate(std::shared_ptr<const graph::Graph> graph,
                             const std::string& collectionName);

  // GET /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  void edgeActionRead(const std::shared_ptr<const graph::Graph> graph,
                      const std::string &definitionName,
                      const std::string &key);

  // DELETE /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  Result edgeActionRemove(std::shared_ptr<const graph::Graph> graph,
                          const std::string& definitionName,
                          const std::string& key);
  
  // POST /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  Result edgeActionCreate(std::shared_ptr<const graph::Graph> graph,
                          const std::string& definitionName);

  // PATCH /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  Result edgeActionUpdate(std::shared_ptr<const graph::Graph> graph,
                          const std::string& collectionName,
                          const std::string& key);

  // PUT /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  Result edgeActionReplace(std::shared_ptr<const graph::Graph> graph,
                           const std::string& collectionName,
                           const std::string& key);

  std::shared_ptr<graph::Graph const> getGraph(
      std::shared_ptr<transaction::Context> ctx, const std::string& graphName);

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

  Result vertexModify(std::shared_ptr<const graph::Graph> graph,
                      const std::string& collectionName, const std::string& key,
                      bool isPatch);

  Result vertexCreate(std::shared_ptr<const graph::Graph> graph,
                      const std::string& collectionName);

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

  Result edgeModify(std::shared_ptr<const graph::Graph> graph,
                    const std::string& collectionName, const std::string& key,
                    bool isPatch);

  Result edgeCreate(std::shared_ptr<const graph::Graph> graph,
                    const std::string& collectionName);

  Result documentModify(std::shared_ptr<const graph::Graph> graph,
                        const std::string& collectionName,
                        const std::string& key, bool isPatch,
                        TRI_col_type_e colType);

  Result documentCreate(
    std::shared_ptr<const graph::Graph> graph, const std::string &collectionName,
    TRI_col_type_e colType
  );

  Result graphActionReadGraphConfig(
    std::shared_ptr<const graph::Graph> graph
  );

  Result graphActionRemoveGraph(
    std::shared_ptr<const graph::Graph> graph
  );

  Result graphActionCreateGraph();
  Result graphActionReadGraphs();

  Result graphActionReadConfig(
    std::shared_ptr<const graph::Graph> graph,
    TRI_col_type_e colType, GraphProperty property
  );

  void generateEdgeModified(
    bool wasSynchronous, VPackSlice resultSlice,
    velocypack::Options const& options
  );

  void generateEdgeCreated(
    bool wasSynchronous, VPackSlice resultSlice,
    velocypack::Options const& options
  );

  // edges
  // PATCH /_api/gharial/{graph-name}/edge/{definition-name}
  Result editEdgeDefinition(
    std::shared_ptr<const graph::Graph> graph,
    const std::string& edgeDefinitionName
  );

  // DELETE /_api/gharial/{graph-name}/edge/{definition-name}
  Result removeEdgeDefinition(
    std::shared_ptr<const graph::Graph> graph,
    const std::string& edgeDefinitionName
  );

  // POST /_api/gharial/{graph-name}/edge/
  Result createEdgeDefinition(
    std::shared_ptr<const graph::Graph> graph
  );

  // edgeDefinitionName may be omitted when action == CREATE
  Result modifyEdgeDefinition(
    std::shared_ptr<const graph::Graph> graph,
    EdgeDefinitionAction action,
    std::string edgeDefinitionName = {}
  );

  Result modifyVertexDefinition(
    std::shared_ptr<const graph::Graph> graph,
    VertexDefinitionAction action,
    std::string vertexDefinitionName
  );

  };
}  // namespace arangodb

#endif  // ARANGOD_REST_HANDLER_REST_GRAPH_HANDLER_H
