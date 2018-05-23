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

#include "RestGraphHandler.h"

#include <boost/optional.hpp>
#include <utility>

#include "Basics/VelocyPackHelper.h"
#include "Graph/Graph.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/Graphs.h"

#define S1(x) #x
#define S2(x) S1(x)
#define LOGPREFIX(func)                                                   \
  "[" << (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__) \
      << ":" S2(__LINE__) << "@" << func << "] "

using namespace arangodb;
using namespace arangodb::graph;

RestGraphHandler::RestGraphHandler(GeneralRequest* request,
                                   GeneralResponse* response,
                                   GraphCache* graphCache_)
    : RestVocbaseBaseHandler(request, response), _graphCache(*graphCache_) {}

RestStatus RestGraphHandler::execute() {
  LOG_TOPIC(INFO, Logger::GRAPHS)
      << LOGPREFIX(__func__) << request()->requestType() << " "
      << request()->requestPath() << " " << request()->suffixes();

  boost::optional<RestStatus> maybeResult;
  try {
    maybeResult = executeGharial();
  } catch (arangodb::basics::Exception& exception) {
    // reset some error messages to match the tests.
    // TODO it's possibly sane to change the tests to check for error codes
    // only instead
    switch (exception.code()) {
      case TRI_ERROR_GRAPH_NOT_FOUND:
        THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NOT_FOUND);
      case TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND:
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
      default:
        throw exception;
    }
  };

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

  std::shared_ptr<Graph const> graph = getGraph(ctx, graphName);

  if (noMoreSuffixes()) {
    // /_api/gharial/{graph-name}
    return graphAction(graph);
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
      return vertexSetsAction(graph);
    } else if (collType == edge) {
      // /_api/gharial/{graph-name}/edge
      return edgeSetsAction(graph);
    }
  }

  std::string const& setName = getNextSuffix();

  // TODO Add tests for this, especially with existing collections & vertices
  // where the collection is only missing in the graph.
  // TODO The existing tests seem to be inconsistent about this:
  // e.g., deleting a non-existent vertex collection is expected to throw
  // TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST but reading a vertex of a
  // non-existent collection is expected to throw
  // ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.
  // This is commented out until the tests are changed.
  /*
    if (collType == vertex) {
      if (graph->vertexCollections().find(setName) ==
    graph->vertexCollections().end()) {
        generateError(TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST);
        return RestStatus::DONE;
      }
    } else if (collType == edge) {
      if (graph->edgeCollections().find(setName) ==
    graph->edgeCollections().end()) {
        generateError(TRI_ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST);
        return RestStatus::DONE;
      }
    }
  */

  if (noMoreSuffixes()) {
    if (collType == vertex) {
      // /_api/gharial/{graph-name}/vertex/{collection-name}
      return vertexSetAction(graph, setName);
    } else if (collType == edge) {
      // /_api/gharial/{graph-name}/edge/{definition-name}
      return edgeSetAction(graph, setName);
    }
  }

  std::string const& elementKey = getNextSuffix();

  if (noMoreSuffixes()) {
    if (collType == vertex) {
      // /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
      return vertexAction(graph, setName, elementKey);
    } else if (collType == edge) {
      // /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
      return edgeAction(graph, setName, elementKey);
    }
  }

  // TODO This should be a 404
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::graphsAction() {
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::graphAction(
    const std::shared_ptr<const Graph> graph) {
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::vertexSetsAction(
    const std::shared_ptr<const Graph> graph) {
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::edgeSetsAction(
    const std::shared_ptr<const Graph> graph) {
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::edgeSetAction(
    const std::shared_ptr<const Graph> graph,
    const std::string& edgeDefinitionName) {
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::vertexSetAction(
    const std::shared_ptr<const Graph> graph,
    const std::string& vertexCollectionName) {
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::vertexAction(
    const std::shared_ptr<const Graph> graph,
    const std::string& vertexCollectionName, const std::string& vertexKey) {
  LOG_TOPIC(WARN, Logger::GRAPHS)
      << LOGPREFIX(__func__) << "graphName = " << graph->name() << ", "
      << "vertexCollectionName = " << vertexCollectionName << ", "
      << "vertexKey = " << vertexKey;

  switch (request()->requestType()) {
    case RequestType::GET: {
      // TODO Maybe vertexActionRead can return void as it already handles
      // errors
      Result res = vertexActionRead(graph, vertexCollectionName, vertexKey);
      return RestStatus::DONE;
    }
    case RequestType::DELETE_REQ:
    case RequestType::PATCH:
    case RequestType::PUT:
    default:;
  }
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::edgeAction(
    const std::shared_ptr<const Graph> graph,
    const std::string& edgeDefinitionName, const std::string& edgeKey) {
  LOG_TOPIC(WARN, Logger::GRAPHS)
      << LOGPREFIX(__func__) << "graphName = " << graph->name() << ", "
      << "edgeDefinitionName = " << edgeDefinitionName << ", "
      << "edgeKey = " << edgeKey;

  switch (request()->requestType()) {
    case RequestType::GET:
      // TODO Maybe edgeActionRead can return void as it already handles
      // errors
      edgeActionRead(graph, edgeDefinitionName, edgeKey);
      return RestStatus::DONE;
    case RequestType::DELETE_REQ:
    case RequestType::PATCH:
    case RequestType::PUT:
    default:;
  }
  return boost::none;
}

Result RestGraphHandler::vertexActionRead(
    const std::shared_ptr<const Graph> graph, const std::string& collectionName,
    const std::string& key) {
  LOG_TOPIC(WARN, Logger::GRAPHS)
      << LOGPREFIX(__func__) << "collectionName = " << collectionName << ", "
      << "key = " << key;

  bool isValidRevision;
  TRI_voc_rid_t ifRid = extractRevision("if-match", isValidRevision);
  if (!isValidRevision) {
    ifRid =
        UINT64_MAX;  // an impossible rev, so precondition failed will happen
  }

  std::shared_ptr<transaction::StandaloneContext> ctx =
      transaction::StandaloneContext::Create(&_vocbase);
  GraphOperations gops{*graph, ctx};
  auto maybeRev = boost::make_optional(ifRid != 0, ifRid);
  auto resultT = gops.getVertex(collectionName, key, maybeRev);

  if (!resultT.ok()) {
    generateTransactionError(collectionName, resultT, "");
    return resultT.copy_result();
  }

  OperationResult& result = resultT.get().first;

  Result res = resultT.get().second;

  if (!result.ok()) {
    if (result.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
      generateDocumentNotFound(collectionName, key);
    } else if (ifRid != 0 && result.is(TRI_ERROR_ARANGO_CONFLICT)) {
      generatePreconditionFailed(result.slice());
    } else {
      generateTransactionError(collectionName, res, key);
    }
    return result.result;
  }

  if (!res.ok()) {
    generateTransactionError(collectionName, res, key);
    return res;
  }

  // use default options
  generateVertex(result.slice(), *ctx->getVPackOptionsForDump());
  return Result();
}

/// @brief generate response object: { error, code, vertex }
void RestGraphHandler::generateVertex(VPackSlice vertex,
                                      VPackOptions const& options) {
  generateResultWithField("vertex", vertex, options);
}

/// @brief generate response object: { error, code, edge }
void RestGraphHandler::generateEdge(VPackSlice edge,
                                    VPackOptions const& options) {
  generateResultWithField("edge", edge, options);
}

/// @brief generate response object: { error, code, key: value }
void RestGraphHandler::generateResultWithField(std::string const& key,
                                               VPackSlice value,
                                               VPackOptions const& options) {
  ResponseCode code = rest::ResponseCode::OK;
  resetResponse(code);

  value = value.resolveExternal();
  std::string rev;
  if (value.isObject()) {
    rev = basics::VelocyPackHelper::getStringValue(
        value, StaticStrings::RevString, "");
  }

  // set ETAG header
  if (!rev.empty()) {
    _response->setHeaderNC(StaticStrings::Etag, rev);
  }

  _response->setContentType(_request->contentTypeResponse());

  try {
    VPackBuffer<uint8_t> buffer;
    VPackBuilder tmp(buffer);
    tmp.add(VPackValue(VPackValueType::Object, true));
    tmp.add(StaticStrings::Error, VPackValue(false));
    tmp.add(StaticStrings::Code, VPackValue(static_cast<int>(code)));
    tmp.add(key, value);
    tmp.close();

    writeResult(std::move(buffer), options);
  } catch (...) {
    // Building the error response failed
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }
}

// TODO this is nearly exactly the same as vertexActionRead. reuse somehow?
Result RestGraphHandler::edgeActionRead(
    const std::shared_ptr<const graph::Graph> graph,
    const std::string& definitionName, const std::string& key) {
  LOG_TOPIC(WARN, Logger::GRAPHS)
      << LOGPREFIX(__func__) << "definitionName = " << definitionName << ", "
      << "key = " << key;

  bool isValidRevision;
  TRI_voc_rid_t ifRid = extractRevision("if-match", isValidRevision);
  if (!isValidRevision) {
    ifRid =
        UINT64_MAX;  // an impossible rev, so precondition failed will happen
  }

  std::shared_ptr<transaction::StandaloneContext> ctx =
      transaction::StandaloneContext::Create(&_vocbase);
  GraphOperations gops{*graph, ctx};
  auto maybeRev = boost::make_optional(ifRid != 0, ifRid);
  auto resultT = gops.getEdge(definitionName, key, maybeRev);

  if (!resultT.ok()) {
    generateTransactionError(definitionName, resultT, "");
    return resultT.copy_result();
  }

  OperationResult& result = resultT.get().first;

  Result res = resultT.get().second;

  if (!result.ok()) {
    if (result.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
      generateDocumentNotFound(definitionName, key);
    } else if (ifRid != 0 && result.is(TRI_ERROR_ARANGO_CONFLICT)) {
      generatePreconditionFailed(result.slice());
    } else {
      generateTransactionError(definitionName, res, key);
    }
    return result.result;
  }

  if (!res.ok()) {
    generateTransactionError(definitionName, res, key);
    return res;
  }

  // use default options
  generateEdge(result.slice(), *ctx->getVPackOptionsForDump());
  return Result();
}

std::shared_ptr<Graph const> RestGraphHandler::getGraph(
    std::shared_ptr<transaction::StandaloneContext> ctx,
    const std::string& graphName) {
  std::shared_ptr<Graph const> graph;

  graph = _graphCache.getGraph(std::move(ctx), graphName);

  // TODO remove exception, handle return value instead
  if (graph == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NOT_FOUND);
  }

  return graph;
}
