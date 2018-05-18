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

#include <boost/optional.hpp>

#include "Graph/Graph.h"
#include "RestGraphHandler.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/Graphs.h"

#define S1(x) #x
#define S2(x) S1(x)
#define LOGPREFIX(func)                                                   \
  "[" << (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__) \
      << ":" S2(__LINE__) << "@" << func << "] "

using namespace arangodb;
using namespace arangodb::graph;

RestGraphHandler::RestGraphHandler(GeneralRequest* request,
                                   GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

RestStatus RestGraphHandler::execute() {
  LOG_TOPIC(INFO, Logger::GRAPHS)
      << LOGPREFIX(__func__) << request()->requestType() << " "
      << request()->requestPath() << " " << request()->suffixes();

  boost::optional<RestStatus> maybeResult = executeGharial();

  if (maybeResult) {
    LOG_TOPIC(INFO, Logger::GRAPHS) << LOGPREFIX(__func__)
                                    << "Used C++ handler";
    return maybeResult.get();
  }

  LOG_TOPIC(INFO, Logger::GRAPHS) << LOGPREFIX(__func__)
                                  << "Using fallback JS handler";

  RestStatus restStatus = RestStatus::FAIL;

  {
    // prepend in reverse order
    // TODO when the fallback routes are removed, the prependSuffix method
    // in GeneralRequest can be removed again.
    _request->prependSuffix("gharial");
    _request->prependSuffix("_api");
    _request->setRequestPath("/");

    // Fallback for routes that aren't implemented yet. TODO Remove later.
    RestActionHandler restActionHandler(_request.release(),
                                        _response.release());
    restStatus = restActionHandler.execute();
    _request = restActionHandler.stealRequest();
    _response = restActionHandler.stealResponse();
  }

  return restStatus;
}

RestGraphHandler::~RestGraphHandler() {}

// Note: boost::none indicates "not (yet) implemented".
// Error-handling for nonexistent routes is, for now, taken from the fallback.
// TODO as soon as this implements everything, just return a RestStatus.
// TODO get rid of RestStatus; it has no useful state.
boost::optional<RestStatus> RestGraphHandler::executeGharial() {
  auto suffix = request()->suffixes().begin();
  auto end = request()->suffixes().end();

  auto getNextSuffix = [&suffix]() { return *suffix++; };

  auto noMoreSuffixes = [&suffix, &end]() { return suffix == end; };

  if (noMoreSuffixes()) {
    // /_api/gharial
    return graphsAction();
  }

  std::string const& graphName = getNextSuffix();

  auto ctx = transaction::StandaloneContext::Create(&_vocbase);

  // TODO cache graph
  std::unique_ptr<const Graph> graph;

  try {
    graph.reset(lookupGraphByName(ctx, graphName));
  }
  catch (arangodb::basics::Exception &exception) {
    if (exception.code() == TRI_ERROR_GRAPH_NOT_FOUND) {
      // reset error message
      THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NOT_FOUND);
    }
  };

  if (graph == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NOT_FOUND);
  }

  if (noMoreSuffixes()) {
    // /_api/gharial/{graph-name}
    return graphAction(std::move(graph));
  }

  std::string const& collType = getNextSuffix();

  const char* vertex = "vertex";
  const char* edge = "edge";
  if (collType != vertex && collType != edge) {
    return boost::none;
  }

  if (noMoreSuffixes()) {
    if (collType == vertex) {
      // /_api/gharial/{graph-name}/vertex
      return vertexSetsAction(std::move(graph));
    } else if (collType == edge) {
      // /_api/gharial/{graph-name}/edge
      return edgeSetsAction(std::move(graph));
    }
  }

  std::string const& setName = getNextSuffix();

  if (noMoreSuffixes()) {
    if (collType == vertex) {
      // /_api/gharial/{graph-name}/vertex/{collection-name}
      return vertexSetAction(std::move(graph), setName);
    } else if (collType == edge) {
      // /_api/gharial/{graph-name}/edge/{definition-name}
      return edgeSetAction(std::move(graph), setName);
    }
  }

  std::string const& elementKey = getNextSuffix();

  if (noMoreSuffixes()) {
    if (collType == vertex) {
      // /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
      return vertexAction(std::move(graph), setName, elementKey);
    } else if (collType == edge) {
      // /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
      return edgeAction(std::move(graph), setName, elementKey);
    }
  }

  // TODO This should be a 404
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::graphsAction() {
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::graphAction(
    const std::unique_ptr<const Graph> graph) {
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::vertexSetsAction(
    const std::unique_ptr<const Graph> graph) {
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::edgeSetsAction(
    const std::unique_ptr<const Graph> graph) {
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::edgeSetAction(
    const std::unique_ptr<const Graph> graph,
    const std::string& edgeDefinitionName) {
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::vertexSetAction(
    const std::unique_ptr<const Graph> graph,
    const std::string& vertexCollectionName) {
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::vertexAction(
    const std::unique_ptr<const Graph> graph,
    const std::string& vertexCollectionName, const std::string& vertexKey) {
  LOG_TOPIC(WARN, Logger::GRAPHS)
      << LOGPREFIX(__func__) << "graphName = " << graph->name() << ", "
      << "vertexCollectionName = " << vertexCollectionName << ", "
      << "vertexKey = " << vertexKey;

  switch (request()->requestType()) {
    case RequestType::DELETE_REQ:
      break;
    case RequestType::GET:
      break;
    case RequestType::PATCH:
      break;
    case RequestType::PUT:
      break;
    default:;
  }
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::edgeAction(
    const std::unique_ptr<const Graph> graph,
    const std::string& edgeDefinitionName, const std::string& edgeKey) {
  LOG_TOPIC(WARN, Logger::GRAPHS)
      << LOGPREFIX(__func__) << "graphName = " << graph->name() << ", "
      << "edgeDefinitionName = " << edgeDefinitionName << ", "
      << "edgeKey = " << edgeKey;
  return boost::none;
}
