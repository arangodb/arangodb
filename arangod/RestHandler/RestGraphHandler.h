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

 public:
  RestGraphHandler(GeneralRequest* request, GeneralResponse* response,
                   graph::GraphCache* graphCache);

  ~RestGraphHandler() override = default;

  char const* name() const final { return "RestGraphHandler"; }

  bool isDirect() const override { return false; }

  size_t queue() const override { return JobQueue::BACKGROUND_QUEUE; }

  RestStatus execute() override;

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
  Result vertexActionRead(std::shared_ptr<const graph::Graph> graph,
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

  // GET /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  Result edgeActionRead(std::shared_ptr<const graph::Graph> graph,
                        const std::string& definitionName,
                        const std::string& key);

  // DELETE /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  Result edgeActionRemove(std::shared_ptr<const graph::Graph> graph,
                          const std::string& definitionName,
                          const std::string& key);

  std::shared_ptr<graph::Graph const> getGraph(
      std::shared_ptr<transaction::StandaloneContext> ctx,
      const std::string& graphName);

  void generateVertexRead(VPackSlice vertex, VPackOptions const& options);

  void generateEdgeRead(VPackSlice edge, const VPackOptions& options);

  void generateRemoved(bool removed, bool wasSynchronous, VPackSlice old,
                       VPackOptions const& options);

  // TODO maybe cleanup the generate* zoo a little?
  void generateResultWithField(std::string const& key, VPackSlice value,
                               VPackOptions const& options);

  void generateResultMergedWithObject(VPackSlice obj,
                                      VPackOptions const& options);

  void addEtagHeader(velocypack::Slice slice);

  Result vertexModify(std::shared_ptr<const graph::Graph> graph,
                      const std::string& collectionName, const std::string& key,
                      bool isPatch);

  void generateVertexModified(bool wasSynchronous, VPackSlice resultSlice,
                              const velocypack::Options& options);

  void generateModified(TRI_col_type_e colType, bool wasSynchronous,
                        VPackSlice resultSlice,
                        const velocypack::Options& options);

  Result edgeActionUpdate(std::shared_ptr<const graph::Graph> graph,
                          const std::string& collectionName,
                          const std::string& key);

  Result edgeActionReplace(std::shared_ptr<const graph::Graph> graph,
                           const std::string& collectionName,
                           const std::string& key);

  Result edgeModify(std::shared_ptr<const graph::Graph> graph,
                    const std::string& collectionName, const std::string& key,
                    bool isPatch);

  Result documentModify(
    std::shared_ptr<const graph::Graph> graph, const std::string &collectionName,
    const std::string &key, bool isPatch, TRI_col_type_e colType
  );

  void generateEdgeModified(
    bool wasSynchronous, VPackSlice resultSlice,
    const velocypack::Options &options
  );
};

}  // namespace arangodb

#endif  // ARANGOD_REST_HANDLER_REST_GRAPH_HANDLER_H
