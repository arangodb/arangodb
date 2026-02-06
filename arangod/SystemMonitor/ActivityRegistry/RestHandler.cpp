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
#include <optional>
#include <variant>

#include "ApplicationFeatures/ApplicationServer.h"
#include "ActivityRegistry/activity_registry_variable.h"
#include "Inspection/VPack.h"

using namespace arangodb;
using namespace arangodb::activity_registry;
using namespace arangodb::containers;

RestHandler::RestHandler(ArangodServer& server, GeneralRequest* request,
                         GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _feature(server.getFeature<Feature>()) {}

auto RestHandler::executeAsync() -> futures::Future<futures::Unit> {
  if (!ExecContext::current().isAdminUser()) {
    generateError(
        rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
        "you need admin user rights for activity-registry operations");
    co_return;
  }

  if (_request->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    co_return;
  }

  auto isForwarded = co_await tryForwarding();
  if (isForwarded) {
    co_return;
  }

  auto lock_guard = co_await _feature.asyncLock();

  VPackBuilder builder;
  builder.openArray();
  registry.for_node([&](ActivityInRegistrySnapshot activity) {
    if (activity.state != State::Deleted) {
      velocypack::serialize(builder, activity);
    }
  });
  builder.close();
  generateResult(rest::ResponseCode::OK, builder.slice());
  co_return;
}
