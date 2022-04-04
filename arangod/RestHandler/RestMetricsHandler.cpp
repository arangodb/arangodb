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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "RestMetricsHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Rest/Version.h"
#include "RestServer/ServerFeature.h"
#include "Metrics/MetricsFeature.h"

#include <frozen/string.h>
#include <frozen/unordered_map.h>
#include <velocypack/Builder.h>

namespace arangodb {
namespace {

constexpr frozen::unordered_map<frozen::string, metrics::CollectMode, 4> kModes{
    {"local", metrics::CollectMode::Local},
    {"trigger_global", metrics::CollectMode::TriggerGlobal},
    {"read_global", metrics::CollectMode::ReadGlobal},
    {"write_global", metrics::CollectMode::WriteGlobal},
};

network::Headers buildHeaders(
    std::unordered_map<std::string, std::string> const& originalHeaders) {
  auto auth = AuthenticationFeature::instance();

  network::Headers headers;
  if (auth != nullptr && auth->isActive()) {
    headers.try_emplace(StaticStrings::Authorization,
                        "bearer " + auth->tokenCache().jwtToken());
  }
  for (auto& header : originalHeaders) {
    headers.try_emplace(header.first, header.second);
  }
  return headers;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

RestMetricsHandler::RestMetricsHandler(ArangodServer& server,
                                       GeneralRequest* request,
                                       GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestMetricsHandler::execute() {
  auto& security = server().getFeature<ServerSecurityFeature>();

  if (!security.canAccessHardenedApi()) {
    // don't leak information about server internals here
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
    return RestStatus::DONE;
  }

  if (_request->requestType() != RequestType::GET) {
    // TODO(MBkkt) Now our API return 405 errorCode for 400 HTTP response code
    //             I think we should fix it, but its breaking change
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  bool foundServerId;
  bool foundType;
  bool foundMode;
  auto const& serverId = _request->value("serverId", foundServerId);
  std::string_view type = _request->value("type", foundType);
  std::string_view mode = _request->value("mode", foundMode);

  foundServerId = foundServerId && ServerState::instance()->isCoordinator() &&
                  serverId != ServerState::instance()->getId();
  // TODO(MBkkt) I think in the future we should return an error
  //             if the ServerId is not a Coordinator or it's our ServerId.
  //             But now it will be breaking changes.

  std::string error;
  size_t notFound = 0;
  if (foundServerId) {
    auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
    if (!ci.serverExists(serverId)) {
      error += "Unknown value of serverId parameter.\n";
      notFound = error.size();
    }
    if (foundType) {
      error += "Can't use type parameter with serverId parameter.\n";
    }
    if (foundMode) {
      error += "Can't use mode parameter with serverId parameter.\n";
    }

    if (error.empty()) {
      auto* pool = server().getFeature<NetworkFeature>().pool();
      if (pool == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }

      network::RequestOptions options;
      options.timeout = network::Timeout(30.0);
      options.database = _request->databaseName();
      options.parameters = _request->parameters();

      auto f = network::sendRequest(
          pool, "server:" + serverId, fuerte::RestVerb::Get,
          _request->requestPath(), VPackBuffer<uint8_t>{}, options,
          buildHeaders(_request->headers()));
      return waitForFuture(std::move(f).thenValue(
          [self = std::dynamic_pointer_cast<RestMetricsHandler>(
               shared_from_this())](network::Response const& r) {
            if (r.fail() || !r.hasResponse()) {
              TRI_ASSERT(r.fail());
              self->generateError(r.combinedResult());
            } else {
              // the response will not contain any velocypack.
              // we need to forward the request with content-type text/plain.
              self->_response->setResponseCode(rest::ResponseCode::OK);
              self->_response->setContentType(rest::ContentType::TEXT);
              auto payload = r.response().stealPayload();
              self->_response->addRawPayload(std::string_view(
                  reinterpret_cast<char const*>(payload->data()),
                  payload->size()));
            }
            return RestStatus::DONE;
          }));
    }
  }

  if (foundType && foundMode) {
    error += "Can't use type parameter with mode parameter.\n";
  }

  if (foundType) {
    if (!ServerState::instance()->isDBServer()) {
      error += "Can't supply type parameter to non-DBServer.\n";
    }
    if (type != "json") {
      error += "Unknown value of type parameter.\n";
    }
  }

  auto it = kModes.find(mode);
  if (foundMode) {
    if (!ServerState::instance()->isCoordinator()) {
      error += "Can't supply mode parameter to non-Coordinator.\n";
    }
    if (it == kModes.end()) {
      error += "Unknown value of mode parameter.\n";
    }
  }

  if (!error.empty()) {
    // TODO(MBkkt) Now our API return 400 errorCode for 404 HTTP response code
    //             I think we should fix it, but its breaking change
    generateError(notFound == error.size() ? rest::ResponseCode::NOT_FOUND
                                           : rest::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER, error);
    return RestStatus::DONE;
  }

  auto& metrics = server().getFeature<metrics::MetricsFeature>();
  if (!metrics.exportAPI()) {
    // don't export metrics, if so desired
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return RestStatus::DONE;
  }

  if (foundType) {
    VPackBuilder builder;
    metrics.toVPack(builder);
    _response->setResponseCode(rest::ResponseCode::OK);
    _response->setContentType(rest::ContentType::VPACK);
    _response->addPayload(builder.slice());
  } else {
    std::string result;
    metrics.toPrometheus(result, [&] {
      if (it != kModes.end()) {
        return it->second;
      }
      return metrics::CollectMode::Local;
    }());
    _response->setResponseCode(rest::ResponseCode::OK);
    _response->setContentType(rest::ContentType::TEXT);
    _response->addRawPayload(result);
  }
  return RestStatus::DONE;
}

}  // namespace arangodb
