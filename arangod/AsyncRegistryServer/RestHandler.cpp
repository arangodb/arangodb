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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#include "RestHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Async/Registry/registry_variable.h"
#include "Inspection/VPack.h"
#include "Rest/CommonDefines.h"

using namespace arangodb::async_registry;

RestHandler::RestHandler(ArangodServer& server, GeneralRequest* request,
                         GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _feature(server.getFeature<Feature>()) {}

auto RestHandler::execute() -> RestStatus {
  if (_request->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  VPackBuilder builder;
  builder.openArray();
  registry.for_promise(
      [&](Promise* promise) { velocypack::serialize(builder, *promise); });
  builder.close();

  generateResult(rest::ResponseCode::OK, builder.slice());
  return RestStatus::DONE;
}
