////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "InternalRestTraverserHandler.h"

#include "Basics/ScopeGuard.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Cluster/TraverserEngine.h"
#include "Cluster/TraverserEngineRegistry.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "Transaction/StandaloneContext.h"

using namespace arangodb;
using namespace arangodb::traverser;
using namespace arangodb::rest;

InternalRestTraverserHandler::InternalRestTraverserHandler(GeneralRequest* request,
                                                           GeneralResponse* response,
                                                           TraverserEngineRegistry* engineRegistry)
    : RestVocbaseBaseHandler(request, response), _registry(engineRegistry) {
  TRI_ASSERT(_registry != nullptr);
}

RestStatus InternalRestTraverserHandler::execute() {
  if (!ServerState::instance()->isDBServer()) {
    generateForbidden();
    return RestStatus::DONE;
  }

  // extract the sub-request type
  auto const type = _request->requestType();

  // execute one of the CRUD methods
  try {
    switch (type) {
      case RequestType::POST:
        createEngine();
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
    generateError(ResponseCode(ex.code()), ex.code(), ex.what());
  } catch (std::exception const& ex) {
    generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL);
  }

  // this handler is done
  return RestStatus::DONE;
}

void InternalRestTraverserHandler::createEngine() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if (!suffixes.empty()) {
    // POST does not allow path parameters
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected POST " + INTERNAL_TRAVERSER_PATH);
    return;
  }

  bool parseSuccess = true;
  VPackSlice body = this->parseVPackBody(parseSuccess);

  if (!parseSuccess) {
    generateError(
        ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "Expected an object with traverser information as body parameter");
    return;
  }

  auto ctx = transaction::StandaloneContext::Create(_vocbase);
  auto id = _registry->createNew(_vocbase, std::move(ctx), body, 600.0, true);

  TRI_ASSERT(id != 0);
  VPackBuilder resultBuilder;
  resultBuilder.add(VPackValue(id));

  // and generate a response
  generateResult(ResponseCode::OK, resultBuilder.slice());
}

void InternalRestTraverserHandler::queryEngine() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  size_t count = suffixes.size();
  if (count < 2 || count > 3) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected PUT " + INTERNAL_TRAVERSER_PATH +
                      "/[vertex|edge]/<TraverserEngineId>");
    return;
  }

  std::string const& option = suffixes[0];
  auto engineId =
      static_cast<TraverserEngineID>(basics::StringUtils::uint64(suffixes[1]));
  if (engineId == 0) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected TraveserEngineId to be an integer number");
    return;
  }
  BaseEngine* engine = _registry->get(engineId);
  if (engine == nullptr) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid TraverserEngineId");
    return;
  }

  auto& registry = _registry;  // For the guard
  auto cleanup = [registry, &engineId]() { registry->returnEngine(engineId); };
  TRI_DEFER(cleanup());

  if (option == "lock") {
    if (count != 3) {
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "expected PUT " + INTERNAL_TRAVERSER_PATH +
                        "/lock/<TraverserEngineId>/<shardId>");
      return;
    }
    if (!engine->lockCollection(suffixes[2])) {
      generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR,
                    "lock lead to an exception");
      return;
    }
    generateResult(ResponseCode::OK, arangodb::velocypack::Slice::trueSlice());
    return;
  }

  if (count != 2) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected PUT " + INTERNAL_TRAVERSER_PATH +
                      "/[vertex|edge]/<TraverserEngineId>");
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

  VPackBuilder result;
  if (option == "edge") {
    VPackSlice keysSlice = body.get("keys");

    if (!keysSlice.isString() && !keysSlice.isArray()) {
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "expecting 'keys' to be a string or an array value.");
      return;
    }

    switch (engine->getType()) {
      case BaseEngine::EngineType::TRAVERSER: {
        VPackSlice depthSlice = body.get("depth");
        if (!depthSlice.isInteger()) {
          generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                        "expecting 'depth' to be an integer value");
          return;
        }
        // Save Cast BaseTraverserEngines are all of type TRAVERSER
        auto eng = static_cast<BaseTraverserEngine*>(engine);
        TRI_ASSERT(eng != nullptr);
        eng->getEdges(keysSlice, depthSlice.getNumericValue<size_t>(), result);
        break;
      }
      case BaseEngine::EngineType::SHORTESTPATH: {
        VPackSlice bwSlice = body.get("backward");
        if (!bwSlice.isBool()) {
          generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                        "expecting 'backward' to be a boolean value");
          return;
        }
        // Save Cast ShortestPathEngines are all of type SHORTESTPATH
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
    if (depthSlice.isNone() || engine->getType() != BaseEngine::EngineType::TRAVERSER) {
      engine->getVertexData(keysSlice, result);
    } else {
      if (!depthSlice.isInteger()) {
        generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                      "expecting 'depth' to be an integer value");
        return;
      }
      // Save Cast BaseTraverserEngines are all of type TRAVERSER
      auto eng = static_cast<BaseTraverserEngine*>(engine);
      TRI_ASSERT(eng != nullptr);
      eng->getVertexData(keysSlice, depthSlice.getNumericValue<size_t>(), result);
    }
  } else if (option == "smartSearch") {
    if (engine->getType() != BaseEngine::EngineType::TRAVERSER) {
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "this engine does not support the requested operation.");
      return;
    }
    // Save Cast BaseTraverserEngines are all of type TRAVERSER
    auto eng = static_cast<BaseTraverserEngine*>(engine);
    TRI_ASSERT(eng != nullptr);
    eng->smartSearch(body, result);
  } else if (option == "smartSearchBFS") {
    if (engine->getType() != BaseEngine::EngineType::TRAVERSER) {
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "this engine does not support the requested operation.");
      return;
    }
    // Save Cast BaseTraverserEngines are all of type TRAVERSER
    auto eng = static_cast<BaseTraverserEngine*>(engine);
    TRI_ASSERT(eng != nullptr);
    eng->smartSearchBFS(body, result);
  } else {
    // PATH Info wrong other error
    generateError(ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND, "");
    return;
  }
  generateResult(ResponseCode::OK, result.slice(), engine->context());
}

void InternalRestTraverserHandler::destroyEngine() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if (suffixes.size() != 1) {
    // DELETE requires the id as path parameter
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected DELETE " + INTERNAL_TRAVERSER_PATH +
                      "/<TraverserEngineId>");
    return;
  }

  TraverserEngineID id = basics::StringUtils::uint64(suffixes[0]);
  _registry->destroy(id);
  generateResult(ResponseCode::OK, arangodb::velocypack::Slice::trueSlice());
}
