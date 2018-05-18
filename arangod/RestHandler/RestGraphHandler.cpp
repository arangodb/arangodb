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

#include "Basics/VelocyPackHelper.h"
#include "Graph/Graph.h"
#include "RestGraphHandler.h"
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
                                   GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

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
  } catch (arangodb::basics::Exception& exception) {
    // reset some error messages to match the tests.
    // TODO it's possibly sane to change the tests to check for error codes
    // only instead
    switch (exception.code()) {
      case TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND:
        THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NOT_FOUND);
      default:
        throw exception;
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
    case RequestType::GET:
      vertexActionRead(vertexCollectionName, vertexKey);
      return RestStatus::DONE;
    case RequestType::DELETE_REQ:
    case RequestType::PATCH:
    case RequestType::PUT:
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

Result RestGraphHandler::vertexActionRead(const std::string& collectionName,
                                          const std::string& key) {
  LOG_TOPIC(WARN, Logger::GRAPHS)
      << LOGPREFIX(__func__) << "collectionName = " << collectionName << ", "
      << "key = " << key;

  // check for an etag
  bool isValidRevision;
  TRI_voc_rid_t ifNoneRid = extractRevision("if-none-match", isValidRevision);
  if (!isValidRevision) {
    ifNoneRid =
        UINT64_MAX;  // an impossible rev, so precondition failed will happen
  }

  OperationOptions options;
  options.ignoreRevs = true;

  TRI_voc_rid_t ifRid = extractRevision("if-match", isValidRevision);
  if (!isValidRevision) {
    ifRid =
        UINT64_MAX;  // an impossible rev, so precondition failed will happen
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add(StaticStrings::KeyString, VPackValue(key));
    if (ifRid != 0) {
      options.ignoreRevs = false;
      builder.add(StaticStrings::RevString, VPackValue(TRI_RidToString(ifRid)));
    }
  }
  VPackSlice search = builder.slice();

  // find and load collection given by name or identifier
  auto ctx(transaction::StandaloneContext::Create(&_vocbase));
  SingleCollectionTransaction trx(ctx, collectionName, AccessMode::Type::READ);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  // ...........................................................................
  // inside read transaction
  // ...........................................................................

  Result res = trx.begin();

  if (!res.ok()) {
    generateTransactionError(collectionName, res, "");
    return res;
  }

  OperationResult result = trx.document(collectionName, search, options);

  res = trx.finish(result.result);

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

  if (ifNoneRid != 0) {
    TRI_voc_rid_t const rid = TRI_ExtractRevisionId(result.slice());
    if (ifNoneRid == rid) {
      generateNotModified(rid);
      return Result();
    }
  }

  // use default options
  generateVertex(result.slice(), *ctx->getVPackOptionsForDump());
  return Result();
}

/// @brief generate response { error, code, vertex }
void RestGraphHandler::generateVertex(VPackSlice vertex,
                                      VPackOptions const& options) {
  ResponseCode code = rest::ResponseCode::OK;
  resetResponse(code);

  vertex = vertex.resolveExternal();
  std::string rev;
  if (vertex.isObject()) {
    rev = basics::VelocyPackHelper::getStringValue(
        vertex, StaticStrings::RevString, "");
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
    tmp.add("vertex", vertex);
    tmp.close();

    writeResult(std::move(buffer), options);
  } catch (...) {
    // Building the error response failed
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }
}
