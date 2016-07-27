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

#include "Basics/VelocyPackHelper.h"
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
#if 0
  if (!ServerState::instance()->isDBServer()) {
    generateForbidden();
    return status::DONE;
  }
#endif

  // extract the sub-request type
  auto const type = _request->requestType();

  // execute one of the CRUD methods
  try {
    switch (type) {
      case GeneralRequest::RequestType::POST:
        createEngine();
        break;
      case GeneralRequest::RequestType::PUT:
        queryEngine();
        break;
      case GeneralRequest::RequestType::DELETE_REQ:
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
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_INTERNAL);
  }

  // this handler is done
  return status::DONE;
}

void InternalRestTraverserHandler::createEngine() {
  std::vector<std::string> const& suffix = _request->suffix();
  if (!suffix.empty()) {
    // POST does not allow path parameters
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected POST " + INTERNAL_TRAVERSER_PATH);
    return;
  }

  bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&VPackOptions::Defaults, parseSuccess);

  if (!parseSuccess) {
    generateError(
        GeneralResponse::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "Expected an object with traverser information as body parameter");
    return;
  }

  // TODO create correctly
  traverser::TraverserEngineID id = _registry->createNew(parsedBody->slice());
  VPackBuilder resultBuilder;
  resultBuilder.add(VPackValue(id));

  // and generate a response
  generateResult(GeneralResponse::ResponseCode::OK, resultBuilder.slice());
}

void InternalRestTraverserHandler::queryEngine() {
}

void InternalRestTraverserHandler::destroyEngine() {
  std::vector<std::string> const& suffix = _request->suffix();
  if (suffix.size() != 1) {
    // DELETE requires the id as path parameter
    generateError(
        GeneralResponse::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expected DELETE " + INTERNAL_TRAVERSER_PATH + "/<TraverserEngineId>");
    return;
  }

  traverser::TraverserEngineID id = basics::StringUtils::uint64(suffix[0]);
  auto engine = _registry->get(id);
  if (engine != nullptr) {
    _registry->returnEngine(id);
  }
  _registry->destroy(id);
  generateResult(GeneralResponse::ResponseCode::OK,
                 arangodb::basics::VelocyPackHelper::TrueValue());
}
