////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Logger/LogMacros.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Utils.h"
#include "fmt/format.h"

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

    LOG_DEVEL << "received pregel stuff: " << body.toJson();

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

    std::vector<std::string> const& suffix = _request->suffixes();
    if (suffix.size() != 0) {
      generateError(
          rest::ResponseCode::BAD, TRI_ERROR_NOT_IMPLEMENTED,
          fmt::format("Worker path not found: {}", fmt::join(suffix, "/")));
      return RestStatus::DONE;
    }

    auto message = inspection::deserializeWithErrorT<ModernMessage>(
        velocypack::SharedSlice(velocypack::SharedSlice{}, body));
    if (!message.ok()) {
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                    message.error().error());
      return RestStatus::DONE;
    }

    auto result = _pregel.process(message.get(), _vocbase);
    if (result.fail()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(result.errorNumber(),
                                     result.errorMessage());
    }
    auto out = inspection::serializeWithErrorT(result.get());
    if (!out.ok()) {
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                    out.error().error());
      return RestStatus::DONE;
    }
    generateResult(rest::ResponseCode::OK, out.get().slice());

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
