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

using namespace arangodb;
using namespace arangodb::rest;

InternalRestTraverserHandler::InternalRestTraverserHandler(
    GeneralRequest* request, GeneralResponse* response,
    traverser::TraverserEngineRegistry* engineRegistry)
    : RestVocbaseBaseHandler(request, response), _registry(engineRegistry) {
      TRI_ASSERT(_registry != nullptr);
    }

RestHandler::status InternalRestTraverserHandler::execute() {
  if (!ServerState::instance()->isDBServer()) {
    generateForbidden();
    return status::DONE;
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
    generateError(ResponseCode(ex.code()), ex.code(),
                  ex.what());
  } catch (std::exception const& ex) {
    generateError(ResponseCode::SERVER_ERROR,
                  TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    generateError(ResponseCode::SERVER_ERROR,
                  TRI_ERROR_INTERNAL);
  }

  // this handler is done
  return status::DONE;
}

void InternalRestTraverserHandler::createEngine() {
  std::vector<std::string> const& suffix = _request->suffix();
  if (!suffix.empty()) {
    // POST does not allow path parameters
    generateError(ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected POST " + INTERNAL_TRAVERSER_PATH);
    return;
  }

  bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&VPackOptions::Defaults, parseSuccess);

  if (!parseSuccess) {
    generateError(
        ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "Expected an object with traverser information as body parameter");
    return;
  }
  traverser::TraverserEngineID id = _registry->createNew(_vocbase, parsedBody->slice());
  TRI_ASSERT(id != 0);
  VPackBuilder resultBuilder;
  resultBuilder.add(VPackValue(id));

  // and generate a response
  generateResult(ResponseCode::OK, resultBuilder.slice());
}

void InternalRestTraverserHandler::queryEngine() {
  std::vector<std::string> const& suffix = _request->suffix();
  size_t count = suffix.size();
  if (count < 2 || count > 3) {
    generateError(
        ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expected PUT " + INTERNAL_TRAVERSER_PATH + "/[vertex|edge]/<TraverserEngineId>");
    return;
  }

  std::string const& option = suffix[0];
  auto engineId = static_cast<traverser::TraverserEngineID>(basics::StringUtils::uint64(suffix[1]));
  if (engineId == 0) {
    generateError(
        ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expected TraveserEngineId to be an integer number");
    return;
  
  }
  traverser::BaseTraverserEngine* engine = _registry->get(engineId);
  if (engine == nullptr) {
    generateError(
        ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "invalid TraverserEngineId");
    return;
  }

  auto& registry = _registry; // For the guard
  arangodb::basics::ScopeGuard guard{
      []() -> void {},
      [registry, &engineId]() -> void { registry->returnEngine(engineId); }};

  if (option == "lock") {
    if (count != 3) {
      generateError(
          ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
          "expected PUT " + INTERNAL_TRAVERSER_PATH + "/lock/<TraverserEngineId>/<shardId>");
      return;
    }
    if (!engine->lockCollection(suffix[2])) {
      generateError(ResponseCode::SERVER_ERROR,
                    TRI_ERROR_HTTP_SERVER_ERROR, "lock lead to an exception");
      return;
    }
    generateResult(ResponseCode::OK,
                   arangodb::basics::VelocyPackHelper::TrueValue());
    return;
  }

  if (count != 2) {
    generateError(
        ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expected PUT " + INTERNAL_TRAVERSER_PATH + "/[vertex|edge]/<TraverserEngineId>");
    return;
  }

  bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&VPackOptions::Defaults, parseSuccess);

  if (!parseSuccess) {
    generateError(
        ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expecting a valid object containing the keys 'depth' and 'keys'");
    return;
  }

  VPackSlice body = parsedBody->slice();
  VPackBuilder result;
  if (option == "edge") {
    VPackSlice depthSlice = body.get("depth");

    VPackSlice keysSlice = body.get("keys");

    if (!keysSlice.isString() && !keysSlice.isArray()) {
      generateError(
          ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
          "expecting 'keys' to be a string or an array value.");
      return;
    }

    if (!depthSlice.isInteger()) {
      generateError(
          ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
          "expecting 'depth' to be an integer value");
      return;
    }

    engine->getEdges(keysSlice, depthSlice.getNumericValue<size_t>(), result);
  } else if (option == "vertex") {
    VPackSlice depthSlice = body.get("depth");

    VPackSlice keysSlice = body.get("keys");

    if (!keysSlice.isString() && !keysSlice.isArray()) {
      generateError(
          ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
          "expecting 'keys' to be a string or an array value.");
      return;
    }

    if (depthSlice.isNone()) {
      engine->getVertexData(keysSlice, result);
    } else {
      if (!depthSlice.isInteger()) {
        generateError(
            ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
            "expecting 'depth' to be an integer value");
        return;
      }
      engine->getVertexData(keysSlice, depthSlice.getNumericValue<size_t>(), result);
    }
  } else if (option == "smartSearch") {
    engine->smartSearch(body, result);
  } else if (option == "smartSearchBFS") {
    engine->smartSearchBFS(body, result);
  } else {
    // PATH Info wrong other error
    generateError(
        ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
        "");
    return;
  }
  generateResult(ResponseCode::OK, result.slice(), engine->context());
}

void InternalRestTraverserHandler::destroyEngine() {
  std::vector<std::string> const& suffix = _request->suffix();
  if (suffix.size() != 1) {
    // DELETE requires the id as path parameter
    generateError(
        ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expected DELETE " + INTERNAL_TRAVERSER_PATH + "/<TraverserEngineId>");
    return;
  }

  traverser::TraverserEngineID id = basics::StringUtils::uint64(suffix[0]);
  _registry->destroy(id);
  generateResult(ResponseCode::OK,
                 arangodb::basics::VelocyPackHelper::TrueValue());
}
