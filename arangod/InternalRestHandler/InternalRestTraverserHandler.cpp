////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "InternalRestTraverserHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryRegistry.h"
#include "Basics/ScopeGuard.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Cluster/TraverserEngine.h"
#include "Logger/LogMacros.h"
#include "Rest/GeneralResponse.h"
#include "Transaction/StandaloneContext.h"

#include <Logger/LogMacros.h>
#include <chrono>
#include <cstdint>
#include <thread>
#include <variant>

using namespace arangodb;
using namespace arangodb::traverser;
using namespace arangodb::rest;

#define LOG_TRAVERSAL LOG_DEVEL_IF(false)

namespace {

struct StartEdgeQuery {
  std::vector<std::string> vertexKeys;
  size_t depth;
  VPackSlice variables;
  std::optional<uint64_t> batchSize = std::nullopt;
};

struct Continue {
  size_t cursorId;
};

struct EdgeQuery {
  std::variant<Continue, StartEdgeQuery> query;
  size_t batchId = 0;
};
ResultT<EdgeQuery> parseQuery(VPackSlice body) {
  auto maybeCursorId = body.get("cursorId");
  if (not maybeCursorId.isNone()) {
    if (not maybeCursorId.isInteger()) {
      return ResultT<EdgeQuery>::error(TRI_ERROR_HTTP_BAD_PARAMETER,
                                       "expecting cursor id as integer");
    }
    auto cursorId = maybeCursorId.getInt();
    if (cursorId < 0) {
      return ResultT<EdgeQuery>::error(TRI_ERROR_HTTP_BAD_PARAMETER,
                                       "expecting cursor id >= 0");
    }
    auto maybeBatchId = body.get("batchId");
    if (maybeBatchId.isNone() || not maybeBatchId.isInteger()) {
      return ResultT<EdgeQuery>::error(TRI_ERROR_HTTP_BAD_PARAMETER,
                                       "expecting integer batch id");
    }
    auto batchId = maybeBatchId.getInt();
    if (batchId < 0) {
      return ResultT<EdgeQuery>::error(TRI_ERROR_HTTP_BAD_PARAMETER,
                                       "expecting batch id >= 0");
    }

    return EdgeQuery{
        .query = Continue{.cursorId = static_cast<size_t>(cursorId)},
        .batchId = static_cast<size_t>(batchId)};
  }

  // keys
  VPackSlice keysSlice = body.get("keys");
  if (!keysSlice.isString() && !keysSlice.isArray()) {
    return ResultT<EdgeQuery>::error(
        TRI_ERROR_HTTP_BAD_PARAMETER,
        "expecting 'keys' to be a string or an array value.");
  }
  std::vector<std::string> vertices;
  if (keysSlice.isArray()) {
    for (VPackSlice vertex : VPackArrayIterator(keysSlice)) {
      TRI_ASSERT(vertex.isString());
      vertices.emplace_back(vertex.copyString());
    }
  } else if (keysSlice.isString()) {
    vertices.emplace_back(keysSlice.copyString());
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  // depth
  VPackSlice depthSlice = body.get("depth");
  if (!depthSlice.isInteger()) {
    return ResultT<EdgeQuery>::error(
        TRI_ERROR_HTTP_BAD_PARAMETER,
        "expecting 'depth' to be an integer value");
  }
  auto depth = depthSlice.getNumericValue<size_t>();

  // variables
  VPackSlice variables = body.get("variables");

  // batch size
  auto maybeBatchSize = body.get("batchSize");
  if (maybeBatchSize.isNone()) {
    return EdgeQuery{StartEdgeQuery{
        .vertexKeys = vertices, .depth = depth, .variables = variables}};
  }
  if (not maybeBatchSize.isInteger()) {
    return ResultT<EdgeQuery>::error(
        TRI_ERROR_HTTP_BAD_PARAMETER,
        "expecting 'batchSize' to be an integer value");
  }
  auto batchSizeInt = maybeBatchSize.getInt();
  if (batchSizeInt <= 0) {
    return ResultT<EdgeQuery>::error(TRI_ERROR_HTTP_BAD_PARAMETER,
                                     "expecting 'batchSize' to be positive");
  }
  auto batchSize = static_cast<uint64_t>(batchSizeInt);

  return EdgeQuery{StartEdgeQuery{.vertexKeys = vertices,
                                  .depth = depth,
                                  .variables = variables,
                                  .batchSize = batchSize}};
}

}  // namespace

auto InternalRestTraverserHandler::get_engine(uint64_t engineId)
    -> ResultT<traverser::BaseEngine*> {
  std::chrono::time_point<std::chrono::steady_clock> start =
      std::chrono::steady_clock::now();
  while (true) {
    try {
      auto engine = _registry->openGraphEngine(engineId);
      if (engine != nullptr) {
        return {engine};
      }
      return ResultT<traverser::BaseEngine*>::error(
          TRI_ERROR_HTTP_BAD_PARAMETER,
          "invalid TraverserEngine id - potentially the AQL query "
          "was already aborted or timed out");
    } catch (basics::Exception const& ex) {
      // it is possible that the engine is already in use
      if (ex.code() != TRI_ERROR_LOCKED) {
        throw;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (server().isStopping()) {
      return {TRI_ERROR_SHUTTING_DOWN};
    }

    if (start + std::chrono::seconds(60) < std::chrono::steady_clock::now()) {
      // timeout
      return {TRI_ERROR_LOCK_TIMEOUT};
    }
  }
}

InternalRestTraverserHandler::InternalRestTraverserHandler(
    ArangodServer& server, GeneralRequest* request, GeneralResponse* response,
    aql::QueryRegistry* engineRegistry)
    : RestVocbaseBaseHandler(server, request, response),
      _registry(engineRegistry) {
  TRI_ASSERT(_registry != nullptr);
}

auto InternalRestTraverserHandler::executeAsync()
    -> futures::Future<futures::Unit> {
  if (!ServerState::instance()->isDBServer()) {
    generateForbidden();
    co_return;
  }

  // extract the sub-request type
  auto const type = _request->requestType();

  // execute one of the CRUD methods
  try {
    switch (type) {
      case RequestType::POST:
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_NOT_IMPLEMENTED,
            "API traversal engine creation no longer supported");
        break;
      case RequestType::PUT:
        queryEngine();
        break;
      case RequestType::DELETE_REQ:
        destroyEngine();
        break;

      default:
        generateNotImplemented("ILLEGAL " + EDGES_PATH);
        break;
    }
  } catch (arangodb::basics::Exception const& ex) {
    generateError(GeneralResponse::responseCode(ex.code()), ex.code(),
                  ex.what());
  } catch (std::exception const& ex) {
    generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL);
  }

  co_return;
}

void InternalRestTraverserHandler::queryEngine() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  size_t count = suffixes.size();
  if (count != 2) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected PUT " + INTERNAL_TRAVERSER_PATH +
                      "/[vertex|edge]/<TraverserEngineId>");
    return;
  }

  std::string const& option = suffixes[0];
  auto engineId =
      static_cast<aql::EngineId>(basics::StringUtils::uint64(suffixes[1]));
  if (engineId == 0) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected TraveserEngineId to be an integer number");
    return;
  }

  bool parseSuccess = true;
  VPackSlice body = this->parseVPackBody(parseSuccess);

  if (!parseSuccess) {
    generateError(
        ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expecting a valid object containing the keys 'depth' and 'keys'");
    return;
  }

  auto maybeEngine = get_engine(engineId);
  if (not maybeEngine.ok()) {
    if (maybeEngine.errorNumber() == TRI_ERROR_LOCK_TIMEOUT) {
      generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_LOCK_TIMEOUT);
      return;
    }
    generateError(ResponseCode::BAD, maybeEngine.errorNumber(),
                  maybeEngine.errorMessage());
    return;
  }
  auto engine = maybeEngine.get();

  TRI_ASSERT(engine != nullptr);

  auto& registry = _registry;  // For the guard
  auto cleanup = scopeGuard([registry, &engineId]() noexcept {
    try {
      registry->closeEngine(engineId);
    } catch (std::exception const& ex) {
      LOG_TOPIC("dfc7a", ERR, Logger::AQL)
          << "Failed to close engine: " << ex.what();
    }
  });

  if (option == "lock") {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_NOT_IMPLEMENTED,
        "API for traversal engine locking no longer supported");
  }

  VPackBuilder result;
  if (option == "edge") {
    switch (engine->getType()) {
      case BaseEngine::EngineType::TRAVERSER: {
        // Safe cast BaseTraverserEngines are all of type TRAVERSER
        auto eng = static_cast<BaseTraverserEngine*>(engine);
        TRI_ASSERT(eng != nullptr);

        auto query = parseQuery(body);
        if (not query.ok()) {
          generateError(ResponseCode::BAD, query.errorNumber(),
                        query.errorMessage());
          return;
        }

        auto startQuery = query.get().query;
        if (std::holds_alternative<StartEdgeQuery>(startQuery)) {
          auto q = std::get<StartEdgeQuery>(startQuery);
          if (q.batchSize.has_value()) {
            eng->rearm(q.depth, q.batchSize.value(), std::move(q.vertexKeys),
                       q.variables);
          } else {
            // old behaviour
            eng->injectVariables(q.variables);
            eng->allEdges(q.vertexKeys, q.depth, result);
            generateResult(ResponseCode::OK, result.slice(), engine->context());
            return;
          }
        } else if (std::holds_alternative<Continue>(startQuery)) {
          auto q = std::get<Continue>(startQuery);
          if (not eng->_cursor.has_value() ||
              q.cursorId != eng->_cursor->_cursorId) {
            generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                          "cursor id " + std::to_string(q.cursorId) +
                              " does not exist in traverser engine " +
                              std::to_string(engineId));
            return;
          }
        }

        result.openObject();
        auto res = eng->nextEdgeBatch(query.get().batchId, result);
        if (res.fail()) {
          generateError(ResponseCode::BAD, res.errorNumber(),
                        res.errorMessage());
          return;
        }
        eng->addAndClearStatistics(result);
        result.close();
        LOG_TRAVERSAL << "--- " << inspection::json(body) << " | "
                      << inspection::json(result);

        generateResult(ResponseCode::OK, result.slice(), engine->context());
        return;
      }
      case BaseEngine::EngineType::SHORTESTPATH: {
        VPackSlice keysSlice = body.get("keys");
        if (!keysSlice.isString() && !keysSlice.isArray()) {
          generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                        "expecting 'keys' to be a string or an array value.");
          return;
        }
        VPackSlice bwSlice = body.get("backward");
        if (!bwSlice.isBool()) {
          generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                        "expecting 'backward' to be a boolean value");
          return;
        }
        // Safe cast ShortestPathEngines are all of type SHORTESTPATH
        auto eng = static_cast<ShortestPathEngine*>(engine);
        TRI_ASSERT(eng != nullptr);
        eng->getEdges(keysSlice, bwSlice.getBoolean(), result);
        break;
      }
      default:
        generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                      "this engine does not support the requested operation.");
        return;
    }
  } else if (option == "vertex") {
    VPackSlice keysSlice = body.get("keys");

    if (!keysSlice.isString() && !keysSlice.isArray()) {
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "expecting 'keys' to be a string or an array value.");
      return;
    }

    VPackSlice depthSlice = body.get("depth");
    if (!depthSlice.isNone() && !depthSlice.isInteger()) {
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "expecting 'depth' to be an integer value");
      return;
    }
    engine->getVertexData(keysSlice, result, !depthSlice.isNone());
  } else if (option == "smartSearch" || option == "smartSearchBFS" ||
             option == "smartSearchWeighted" ||
             option == "smartSearchUnified") {
    if (engine->getType() != BaseEngine::EngineType::TRAVERSER) {
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "this engine does not support the requested operation.");
      return;
    }
    // Safe cast BaseTraverserEngines are all of type TRAVERSER
    auto eng = static_cast<BaseTraverserEngine*>(engine);
    TRI_ASSERT(eng != nullptr);

    try {
      if (option == "smartSearchUnified") {
        eng->smartSearchUnified(body, result);
      } else {
        // TODO: Take deprecation path!
        eng->smartSearch(body, result);
      }
    } catch (arangodb::basics::Exception const& ex) {
      generateError(ResponseCode::BAD, ex.code(), ex.what());
      return;
    }
    LOG_TRAVERSAL << "--- " << inspection::json(body) << " | "
                  << inspection::json(result);
  } else {
    // PATH Info wrong other error
    generateError(ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND, "");
    return;
  }
  generateResult(ResponseCode::OK, result.slice(), engine->context());
}

// simon: this API will no longer be used in 3.7 to regularly
// shut down an AQL query, but it can be used during query setup if the
// setup fails.
void InternalRestTraverserHandler::destroyEngine() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if (suffixes.size() != 1) {
    // DELETE requires the id as path parameter
    generateError(
        ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expected DELETE " + INTERNAL_TRAVERSER_PATH + "/<TraverserEngineId>");
    return;
  }

  auto engineId =
      static_cast<aql::EngineId>(basics::StringUtils::uint64(suffixes[0]));
  bool found = _registry->destroyEngine(engineId, TRI_ERROR_NO_ERROR);
  generateResult(ResponseCode::OK, VPackSlice::booleanSlice(found));
}
