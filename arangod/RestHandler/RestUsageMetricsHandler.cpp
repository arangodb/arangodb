////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestUsageMetricsHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Metrics/MetricsFeature.h"
#include "Metrics/MetricsParts.h"
#include "Metrics/Types.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Rest/Version.h"
#include "RestServer/ServerFeature.h"

namespace arangodb {

RestUsageMetricsHandler::RestUsageMetricsHandler(ArangodServer& server,
                                                 GeneralRequest* request,
                                                 GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestUsageMetricsHandler::execute() {
  auto& security = server().getFeature<ServerSecurityFeature>();

  if (!security.canAccessHardenedApi()) {
    // don't leak information about server internals here
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
    return RestStatus::DONE;
  }

  if (_request->requestType() != RequestType::GET) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  bool foundServerId = false;
  auto const& serverId = _request->value("serverId", foundServerId);

  foundServerId = foundServerId && ServerState::instance()->isCoordinator() &&
                  serverId != ServerState::instance()->getId();

  if (foundServerId) {
    auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
    if (!ci.serverExists(serverId)) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "Unknown value of serverId parameter.");
      return RestStatus::DONE;
    }
  }

  if (foundServerId) {
    return makeRedirection(serverId);
  }

  _response->setAllowCompression(true);

  auto& metrics = server().getFeature<metrics::MetricsFeature>();
  if (!metrics.exportAPI()) {
    // don't export metrics, if so desired
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return RestStatus::DONE;
  }

  // only export dynamic metrics for shard usage
  metrics::MetricsParts metricsParts(metrics::MetricsSection::Dynamic);

  std::string result;
  metrics.toPrometheus(result, metricsParts, metrics::CollectMode::Local);
  _response->setResponseCode(rest::ResponseCode::OK);
  _response->setContentType(rest::ContentType::TEXT);
  _response->addRawPayload(result);
  return RestStatus::DONE;
}

RestStatus RestUsageMetricsHandler::makeRedirection(
    std::string const& serverId) {
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
      network::addAuthorizationHeader(_request->headers()));

  return waitForFuture(std::move(f).thenValue([self = shared_from_this()](
                                                  network::Response&& r) {
    auto& me = basics::downCast<RestUsageMetricsHandler>(*self);
    if (r.fail() || !r.hasResponse()) {
      TRI_ASSERT(r.fail());
      me.generateError(r.combinedResult());
      return;
    }
    // the response will not contain any velocypack.
    // we need to forward the request with content-type text/plain.
    if (r.response().header.meta().contains(StaticStrings::ContentEncoding)) {
      // forward original Content-Encoding header
      me._response->setHeaderNC(
          StaticStrings::ContentEncoding,
          r.response().header.metaByKey(StaticStrings::ContentEncoding));
    }

    me._response->setResponseCode(rest::ResponseCode::OK);
    me._response->setContentType(rest::ContentType::TEXT);
    auto payload = r.response().stealPayload();
    me._response->addRawPayload(
        {reinterpret_cast<char const*>(payload->data()), payload->size()});
  }));
}

}  // namespace arangodb
