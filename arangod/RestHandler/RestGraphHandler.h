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

#include <arangod/Actions/RestActionHandler.h>
#include <3rdParty/boost/1.62.0/boost/optional.hpp>
#include "RestHandler/RestBaseHandler.h"

namespace arangodb {

class RestGraphHandler : public arangodb::RestBaseHandler {
 public:
  RestGraphHandler(GeneralRequest* request, GeneralResponse* response);

  virtual ~RestGraphHandler();

  char const* name() const final { return "RestGraphHandler"; }

  bool isDirect() const override { return false; }

  size_t queue() const override { return JobQueue::BACKGROUND_QUEUE; }

  RestStatus execute() override;

  boost::optional<RestStatus> executeGharial();

  // /_api/gharial
  boost::optional<RestStatus> graphsAction();

  // /_api/gharial/{graph-name}
  boost::optional<RestStatus> graphAction(const std::string& graphName);

  // /_api/gharial/{graph-name}/vertex
  boost::optional<RestStatus> vertexSetsAction(const std::string& graphName);

  // /_api/gharial/{graph-name}/edge
  boost::optional<RestStatus> edgeSetsAction(const std::string& graphName);

  // /_api/gharial/{graph-name}/vertex/{collection-name}
  boost::optional<RestStatus> vertexSetAction(
      const std::string& graphName, const std::string& vertexCollectionName);

  // /_api/gharial/{graph-name}/edge/{definition-name}
  boost::optional<RestStatus> edgeSetAction(
      const std::string& graphName, const std::string& edgeDefinitionName);

  // /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
  boost::optional<RestStatus> vertexAction(
      const std::string& graphName, const std::string& vertexCollectionName,
      const std::string& vertexKey);

  // /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
  boost::optional<RestStatus> edgeAction(const std::string& graphName,
                                         const std::string& edgeDefinitionName,
                                         const std::string& edgeKey);
};

}  // namespace arangodb

#endif  // ARANGOD_REST_HANDLER_REST_GRAPH_HANDLER_H
