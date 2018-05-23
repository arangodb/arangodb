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

class RestGraphHandler : public arangodb::RestVocbaseBaseHandler {
 public:
  RestGraphHandler(GeneralRequest* request, GeneralResponse* response,
                   graph::GraphCache* graphCache);

  ~RestGraphHandler() override = default;

  char const* name() const final { return "RestGraphHandler"; }

  bool isDirect() const override { return false; }

  size_t queue() const override { return JobQueue::BACKGROUND_QUEUE; }

  RestStatus execute() override;

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

  void generateVertex(VPackSlice vertex, VPackOptions const& options);

 private:
  graph::GraphCache& _graphCache;

  std::shared_ptr<graph::Graph const> getGraph(
      std::shared_ptr<transaction::StandaloneContext> ctx,
      const std::string& graphName);
};

}  // namespace arangodb

#endif  // ARANGOD_REST_HANDLER_REST_GRAPH_HANDLER_H
