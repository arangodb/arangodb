////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Agency/AgencyComm.h"
#include "Agency/AgencyFeature.h"
#include "Agency/Agent.h"
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

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
network::Headers buildHeaders(std::unordered_map<std::string, std::string> const& originalHeaders) {
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

RestMetricsHandler::RestMetricsHandler(application_features::ApplicationServer& server,
                                       GeneralRequest* request, GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestMetricsHandler::execute() {
  ServerSecurityFeature& security = server().getFeature<ServerSecurityFeature>();

  if (!security.canAccessHardenedApi()) {
    // dont leak information about server internals here
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
    return RestStatus::DONE;
  }

  if (_request->requestType() != RequestType::GET) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  bool foundServerIdParameter;
  std::string const& serverId = _request->value("serverId", foundServerIdParameter);

  if (ServerState::instance()->isCoordinator() && foundServerIdParameter) {
    if (serverId != ServerState::instance()->getId()) {
      // not ourselves! - need to pass through the request
      auto& ci = server().getFeature<ClusterFeature>().clusterInfo();

      bool found = false;
      for (auto const& srv : ci.getServers()) {
        // validate if server id exists
        if (srv.first == serverId) {
          found = true;
          break;
        }
      }

      if (!found) {
        generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_BAD_PARAMETER,
                      std::string("unknown serverId supplied."));
        return RestStatus::DONE;
      }

      NetworkFeature const& nf = server().getFeature<NetworkFeature>();
      network::ConnectionPool* pool = nf.pool();
      if (pool == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }

      network::RequestOptions options;
      options.timeout = network::Timeout(30.0);
      options.database = _request->databaseName();
      options.parameters = _request->parameters();

      auto f = network::sendRequest(pool, "server:" + serverId, fuerte::RestVerb::Get,
                                    _request->requestPath(), VPackBuffer<uint8_t>{},
                                    options, buildHeaders(_request->headers()));
      return waitForFuture(std::move(f).thenValue(
          [self = std::dynamic_pointer_cast<RestMetricsHandler>(shared_from_this())](
              network::Response const& r) {
            if (r.fail()) {
              self->generateError(r.combinedResult());
            } else {
             // the response will not contain any velocypack.
             // we need to forward the request with content-type text/plain.
              self->_response->setResponseCode(rest::ResponseCode::OK);
              self->_response->setContentType(rest::ContentType::TEXT);
              auto payload = r.response().stealPayload();
              self->_response->addRawPayload(VPackStringRef(reinterpret_cast<char const*>(payload->data()), payload->size()));
            }
            return RestStatus::DONE;
          }));
    }
  }

  MetricsFeature& metrics = server().getFeature<MetricsFeature>();
  if (!metrics.exportAPI()) {
    // dont export metrics, if so desired
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return RestStatus::DONE;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();

  bool v2 = false;
  if (suffixes.size() > 0 && suffixes[0] == "v2") {
    v2 = true;
  }

  std::string result;
  metrics.toPrometheus(result, v2);
  _response->setResponseCode(rest::ResponseCode::OK);
  _response->setContentType(rest::ContentType::TEXT);
  _response->addRawPayload(VPackStringRef(result));
  
  return RestStatus::DONE;
}
