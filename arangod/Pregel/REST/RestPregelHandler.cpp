////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RestPregelHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"

#include <velocypack/Builder.h>
#include <velocypack/SharedSlice.h>

#include "Inspection/VPackWithErrorT.h"
#include "Logger/LogMacros.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/SpawnActor.h"
#include "Pregel/Utils.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::pregel;

RestPregelHandler::RestPregelHandler(ArangodServer& server,
                                     GeneralRequest* request,
                                     GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _pregel(server.getFeature<PregelFeature>()) {}

RestStatus RestPregelHandler::execute() {
  try {
    bool parseSuccess = true;
    VPackSlice body = parseVPackBody(parseSuccess);
    if (!parseSuccess || !body.isObject()) {
      // error message generated in parseVPackBody
      return RestStatus::DONE;
    }
    if (_request->requestType() != rest::RequestType::POST) {
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_NOT_IMPLEMENTED,
                    "illegal method for /_api/pregel");
      return RestStatus::DONE;
    }

    VPackBuilder response;
    std::vector<std::string> const& suffix = _request->suffixes();
    if (suffix.size() == 1 && suffix[0] == "actor") {
      auto msg = inspection::deserializeWithErrorT<pregel::NetworkMessage>(
          velocypack::SharedSlice({}, body));
      if (!msg.ok()) {
        generateError(
            rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
            fmt::format(
                "Received actor network message {} cannot be deserialized",
                body.toJson()));
        return RestStatus::DONE;
      }
      if (msg.get().receiver.id == actor::ActorID{0}) {
        _pregel._actorRuntime->spawn<SpawnActor>(
            _vocbase.name(), SpawnState{},
            velocypack::SharedSlice({}, msg.get().payload.slice()));
        generateResult(rest::ResponseCode::OK, VPackBuilder().slice());
        return RestStatus::DONE;
      } else {
        _pregel._actorRuntime->receive(
            msg.get().sender, msg.get().receiver,
            velocypack::SharedSlice({}, msg.get().payload.slice()));
        generateResult(rest::ResponseCode::OK, VPackBuilder().slice());
        return RestStatus::DONE;
      }
    } else if (suffix.size() != 2) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_NOT_IMPLEMENTED,
                    "you are missing a prefix");
    } else if (suffix[0] == Utils::conductorPrefix) {
      _pregel.handleConductorRequest(_vocbase, suffix[1], body, response);
      generateResult(rest::ResponseCode::OK, response.slice());
    } else if (suffix[0] == Utils::workerPrefix) {
      _pregel.handleWorkerRequest(_vocbase, suffix[1], body, response);

      generateResult(rest::ResponseCode::OK, response.slice());
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_NOT_IMPLEMENTED,
                    "the prefix is incorrect");
    }
  } catch (basics::Exception const& ex) {
    LOG_TOPIC("d1b56", ERR, arangodb::Logger::PREGEL)
        << "Exception in pregel REST handler: " << ex.what();
    generateError(GeneralResponse::responseCode(ex.code()), ex.code(),
                  ex.what());
  } catch (std::exception const& ex) {
    LOG_TOPIC("2f547", ERR, arangodb::Logger::PREGEL)
        << "Exception in pregel REST handler: " << ex.what();
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  ex.what());
  } catch (...) {
    LOG_TOPIC("e2ef6", ERR, Logger::PREGEL)
        << "Exception in pregel REST handler";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "error in pregel handler");
  }

  return RestStatus::DONE;
}
