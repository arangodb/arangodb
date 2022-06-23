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
#include "Metrics/Types.h"

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

bool isOutdated(
    GeneralRequest const& oldData,
    std::shared_ptr<metrics::ClusterMetricsFeature::Data> const& data) {
  if (!data || !data->packed) {
    TRI_ASSERT(!data);
    return false;
  }
  velocypack::Slice newData{data->packed->data()};
  auto const& oldVersion = oldData.value("MetricsVersion");
  TRI_ASSERT(!oldVersion.empty());
  auto const newVersion = newData.get("Version").getNumber<uint64_t>();
  if (std::stoull(oldVersion) < newVersion) {
    return true;
  }
  auto const& oldRebootId = oldData.value("MetricsRebootId");
  TRI_ASSERT(!oldRebootId.empty());
  auto const newRebootId = newData.get("RebootId").getNumber<uint64_t>();
  if (std::stoull(oldRebootId) != newRebootId) {
    return true;
  }
  auto const& oldServerId = oldData.value("MetricsServerId");
  auto const newServerId = newData.get("ServerId").stringView();
  return oldServerId != newServerId;
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
  auto const& type = _request->value("type", foundType);
  auto const& modeStr = _request->value("mode", foundMode);
  auto itMode = kModes.find(std::string_view{modeStr});
  auto mode = [&] {
    if (itMode != kModes.end()) {
      return itMode->second;
    }
    return metrics::CollectMode::Local;
  }();

  foundServerId = foundServerId && ServerState::instance()->isCoordinator() &&
                  serverId != ServerState::instance()->getId();
  // TODO(MBkkt) I think in the future we should return an error
  //             if the ServerId is not a Coordinator or it's our ServerId.
  //             But now it will be breaking changes.

  std::string error;

  if (foundMode) {
    if (foundType && type != metrics::kLast) {
      error += "Can't use mode parameter with type parameter.\n";
    }
    if (!ServerState::instance()->isCoordinator()) {
      error += "Can't supply mode parameter to non-Coordinator.\n";
    }
    if (itMode == kModes.end()) {
      error += "Unknown value of mode parameter.\n";
    }
  }

  if (foundType) {
    if (foundServerId) {
      error += "Can't use type parameter with serverId parameter.\n";
    }
    if (type == metrics::kCDJson) {
      auto const& oldVersion = _request->value("MetricsVersion");
      auto const& oldRebootId = _request->value("MetricsRebootId");
      auto const& oldServerId = _request->value("MetricsServerId");
      if (oldVersion.empty() || oldRebootId.empty() || oldServerId.empty()) {
        error += "Incorrect type=cd_json usage.\n";
      }
    }
    if (type == metrics::kLast && !foundMode) {
      error += "Incorrect type=last usage.\n";
    }
    if (type == metrics::kCDJson || type == metrics::kLast) {
      if (!ServerState::instance()->isCoordinator()) {
        error +=
            "Can't supply type=cd_json/last parameter to non-Coordinator.\n";
      }
    } else if (type == metrics::kDBJson) {
      if (!ServerState::instance()->isDBServer()) {
        error += "Can't supply type=db_json parameter to non-DBServer.\n";
      }
    } else {
      error += "Unknown value of type parameter.\n";
    }
  }

  bool const notFound = error.empty();
  if (foundServerId) {
    auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
    if (!ci.serverExists(serverId)) {
      error += "Unknown value of serverId parameter.\n";
    }
  }

  if (!error.empty()) {
    // TODO(MBkkt) Now our API return 400 errorCode for 404 HTTP response code
    //             I think we should fix it, but its breaking change
    generateError(
        notFound ? rest::ResponseCode::NOT_FOUND : rest::ResponseCode::BAD,
        TRI_ERROR_HTTP_BAD_PARAMETER, error);
    return RestStatus::DONE;
  }

  if (foundServerId) {
    return makeRedirection(serverId, false);
  }

  if (type == metrics::kCDJson) {
    _response->setResponseCode(rest::ResponseCode::OK);
    _response->setContentType(rest::ContentType::VPACK);
    auto& metrics = server().getFeature<metrics::ClusterMetricsFeature>();
    auto data = metrics.getData();
    if (isOutdated(*_request, data)) {
      _response->addPayload(VPackSlice{data->packed->data()});
    } else {
      _response->addPayload(VPackSlice::noneSlice());
    }
    return RestStatus::DONE;
  }

  auto& metrics = server().getFeature<metrics::MetricsFeature>();
  if (!metrics.exportAPI()) {
    // don't export metrics, if so desired
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return RestStatus::DONE;
  }

  if (type == metrics::kDBJson) {
    VPackBuilder builder;
    metrics.toVPack(builder);
    _response->setResponseCode(rest::ResponseCode::OK);
    _response->setContentType(rest::ContentType::VPACK);
    _response->addPayload(builder.slice());
    return RestStatus::DONE;
  }

  auto const leader = [&]() -> std::optional<std::string> {
    auto& cm = server().getFeature<metrics::ClusterMetricsFeature>();
    if (cm.isEnabled() && mode != metrics::CollectMode::Local) {
      return cm.update(mode);
    }
    return std::nullopt;
  }();

  if (leader && (leader->empty() || type == metrics::kLast)) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "We didn't find leader server");
    return RestStatus::DONE;
  }

  if (!leader) {
    std::string result;
    metrics.toPrometheus(result, mode);
    _response->setResponseCode(rest::ResponseCode::OK);
    _response->setContentType(rest::ContentType::TEXT);
    _response->addRawPayload(result);
    return RestStatus::DONE;
  }
  TRI_ASSERT(mode == metrics::CollectMode::ReadGlobal ||
             mode == metrics::CollectMode::WriteGlobal)
      << modeStr;
  return makeRedirection(*leader, true);
}

RestStatus RestMetricsHandler::makeRedirection(std::string const& serverId,
                                               bool last) {
  auto* pool = server().getFeature<NetworkFeature>().pool();
  if (pool == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  network::RequestOptions options;
  options.timeout = network::Timeout(30.0);
  options.database = _request->databaseName();
  options.parameters = _request->parameters();
  if (last) {
    options.parameters.try_emplace("type", metrics::kLast);
  }

  auto f =
      network::sendRequest(pool, "server:" + serverId, fuerte::RestVerb::Get,
                           _request->requestPath(), VPackBuffer<uint8_t>{},
                           options, buildHeaders(_request->headers()));

  return waitForFuture(std::move(f).thenValue(
      [self = shared_from_this(), last](network::Response&& r) {
        auto& me = basics::downCast<RestMetricsHandler>(*self);
        if (r.fail() || !r.hasResponse()) {
          TRI_ASSERT(r.fail());
          me.generateError(r.combinedResult());
          return;
        }
        if (last) {
          auto& cm = me.server().getFeature<metrics::ClusterMetricsFeature>();
          if (cm.isEnabled()) {
            cm.update(metrics::CollectMode::TriggerGlobal);
          }
        }
        // TODO(MBkkt) move response
        // the response will not contain any velocypack.
        // we need to forward the request with content-type text/plain.
        me._response->setResponseCode(rest::ResponseCode::OK);
        me._response->setContentType(rest::ContentType::TEXT);
        auto payload = r.response().stealPayload();
        me._response->addRawPayload(
            {reinterpret_cast<char const*>(payload->data()), payload->size()});
      }));
}

}  // namespace arangodb
