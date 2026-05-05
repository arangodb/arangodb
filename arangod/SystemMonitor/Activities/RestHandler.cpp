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
#include <ranges>

#include "Agency/AsyncAgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/voc-errors.h"
#include "Cluster/ClusterInfo.h"
#include "Futures/Utilities.h"
#include "Inspection/VPackWithErrorT.h"
#include "Logger/LogMacros.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Rest/ApiVersion.h"
#include "Cluster/ClusterFeature.h"
#include "Rest/CommonDefines.h"
#include "fuerte/ApiVersion.h"

using namespace arangodb;
using namespace arangodb::activities;
using namespace arangodb::containers;

RestHandler::RestHandler(application_features::ApplicationServer& server,
                         GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _feature(server.getFeature<Feature>()),
      _clusterFeature(server.getFeature<ClusterFeature>()),
      _networkFeature(server.getFeature<NetworkFeature>()) {}

namespace {
template<typename R>
requires std::ranges::input_range<R> &&
    std::convertible_to<std::ranges::range_reference_t<R>, std::string_view>
auto getActivitiesFromServers(
    R&& servers,
    // containers::FlatHashMap<ServerID, std::string> const& servers,
    std::vector<std::pair<ServerID, std::string>> agencies,
    NetworkFeature& networkFeature, std::string const& path)
    -> async<
        std::vector<std::pair<ServerID, futures::Try<network::Response>>>> {
  auto* pool = networkFeature.pool();
  if (pool == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }
  network::RequestOptions options;
  options.timeout = network::Timeout(30.0);
  options.apiVersion = api_version::ApiVersion::Experimental;

  std::vector<network::FutureRes> requests;
  std::vector<ServerID> serverIds;

  for (auto const& serverId : servers) {
    serverIds.emplace_back(serverId);
    requests.emplace_back(network::sendRequestRetry(
        pool, "server:" + serverId, fuerte::RestVerb::Get, path,
        VPackBuffer<uint8_t>{}, options));
  }
  for (auto const& [agencyId, endpoint] : agencies) {
    serverIds.emplace_back(agencyId);
    requests.emplace_back(
        network::sendRequestRetry(pool, endpoint, fuerte::RestVerb::Get, path,
                                  VPackBuffer<uint8_t>{}, options));
  }
  auto responses = co_await futures::collectAll(requests);
  std::vector<std::pair<ServerID, futures::Try<network::Response>>> result;
  auto responseCount = 0;
  for (auto&& response : responses) {
    result.emplace_back(
        std::make_pair(serverIds[responseCount], std::move(response)));
    responseCount++;
  }
  co_return result;
}

auto serializeOneServer(VPackBuilder& builder,
                        futures::Try<network::Response>&& responseFuture)
    -> void {
  if (not responseFuture.valid()) {
    auto result = Result{TRI_ERROR_INTERNAL, "Server response is invalid"};
    velocypack::serialize(builder, result);
    return;
  }
  auto response = std::move(responseFuture).get();
  if (response.fail()) {
    auto error = response.combinedResult();
    velocypack::serialize(builder, error);
    return;
  }
  auto activitiesOneServer =
      inspection::deserializeWithErrorT<Output>(response.slice());
  if (activitiesOneServer.ok()) {
    velocypack::serialize(builder, activitiesOneServer.get().activities);
  } else {
    velocypack::serialize(builder, activitiesOneServer.error().error());
  }
  return;
}

}  // namespace

auto RestHandler::executeAsync() -> futures::Future<futures::Unit> {
  if (_feature.isOnlySuperUserEnabled()) {
    if (!ExecContext::current().isSuperuser()) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                    "You need super user rights for activities operations");
      co_return;
    }
  } else {
    if (!ExecContext::current().isAdminUser()) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                    "you need admin user rights for activities operations");
      co_return;
    }
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

  auto suffixes = _request->suffixes();
  if (suffixes.size() > 1) {
    co_return;
  }

  if (suffixes.size() == 0) {
    switch (_request->requestedApiVersion()) {
      case api_version::experimentalApiVersion: {
        auto output = Output{.activities = _feature.getData()};
        VPackBuilder builder;
        velocypack::serialize(builder, output);
        generateResult(rest::ResponseCode::OK, builder.slice());
        co_return;
      };
      default: {
        generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
        co_return;
      };
    }
  }
  if (suffixes[0] != "all") {
    co_return;
  }

  if (not ServerState::instance()->isCoordinator()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    co_return;
  }

  auto servers = _clusterFeature.clusterInfo().getServers();
  auto myId = ServerState::instance()->getId();
  servers.erase(myId);
  auto agencies = co_await AsyncAgencyComm().getAgencies();
  auto activities_per_server =
      co_await getActivitiesFromServers(servers | std::views::keys, agencies,
                                        _networkFeature, _request->prefix());

  switch (_request->requestedApiVersion()) {
    case api_version::experimentalApiVersion: {
      VPackBuilder builder;
      builder.openObject();
      builder.add(VPackValue("activities_per_server"));
      builder.openObject();

      // me
      builder.add(VPackValue(myId));
      auto myActivities = _feature.getData();
      velocypack::serialize(builder, myActivities);

      for (auto& [serverId, responseFuture] : activities_per_server) {
        builder.add(VPackValue(serverId));
        serializeOneServer(builder, std::move(responseFuture));
      }
      builder.close();
      builder.close();

      generateResult(rest::ResponseCode::OK, builder.slice());
      co_return;
    };
    default: {
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
      co_return;
    };
  }
}
