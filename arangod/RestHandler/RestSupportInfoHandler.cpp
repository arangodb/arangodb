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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestSupportInfoHandler.h"

#include "Agency/AsyncAgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CpuUsageFeature.h"
#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/process-utils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Logger/LogTimeFormat.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Replication/ReplicationFeature.h"
#include "Rest/Version.h"
#include "RestServer/EnvironmentFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/ServerFeature.h"
#include "Statistics/ServerStatistics.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/ExecContext.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include <chrono>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
network::Headers buildHeaders() {
  auto auth = AuthenticationFeature::instance();

  network::Headers headers;
  if (auth != nullptr && auth->isActive()) {
    headers.try_emplace(StaticStrings::Authorization,
                        "bearer " + auth->tokenCache().jwtToken());
  }
  return headers;
}
}  // namespace

RestSupportInfoHandler::RestSupportInfoHandler(
    application_features::ApplicationServer& server, GeneralRequest* request,
    GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestSupportInfoHandler::execute() {
  GeneralServerFeature& gs = server().getFeature<GeneralServerFeature>();
  auto const& apiPolicy = gs.supportInfoApiPolicy();
  TRI_ASSERT(apiPolicy != "disabled");

  if (apiPolicy == "jwt") {
    if (!ExecContext::current().isSuperuser()) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                    "insufficent permissions");
      return RestStatus::DONE;
    }
  }

  if (apiPolicy == "hardened") {
    ServerSecurityFeature& security =
        server().getFeature<ServerSecurityFeature>();
    if (!security.canAccessHardenedApi()) {
      // superuser can still access API even if hardened
      if (!ExecContext::current().isSuperuser()) {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                      "insufficent permissions");
        return RestStatus::DONE;
      }
    } else if (!ExecContext::current().isAdminUser()) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                    "insufficent permissions");
      return RestStatus::DONE;
    }
  }

  if (_request->databaseName() != StaticStrings::SystemDatabase) {
    generateError(
        GeneralResponse::responseCode(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE),
        TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
    return RestStatus::DONE;
  }

  // used for all types of responses
  VPackBuilder hostInfo;
  buildHostInfo(hostInfo);
  std::string timeString;
  LogTimeFormats::writeTime(timeString,
                            LogTimeFormats::TimeFormat::UTCDateString,
                            std::chrono::system_clock::now());

  bool const isActiveFailover =
      server().getFeature<ReplicationFeature>().isActiveFailoverEnabled();
  bool const fanout =
      (ServerState::instance()->isCoordinator() || isActiveFailover) &&
      !_request->parsedValue("local", false);

  VPackBuilder result;
  result.openObject();

  if (ServerState::instance()->isSingleServer() && !isActiveFailover) {
    result.add("deployment", VPackValue(VPackValueType::Object));
    result.add("type", VPackValue("single"));
    result.close();  // deployment
    result.add("host", hostInfo.slice());
    result.add("date", VPackValue(timeString));
  } else {
    // cluster or active failover
    if (fanout) {
      // cluster coordinator or active failover case, fan out to other servers!
      result.add("deployment", VPackValue(VPackValueType::Object));
      if (isActiveFailover) {
        TRI_ASSERT(!ServerState::instance()->isCoordinator());
        result.add("type", VPackValue("active-failover"));
      } else {
        TRI_ASSERT(ServerState::instance()->isCoordinator());
        result.add("type", VPackValue("cluster"));
      }

      // build results for all servers
      result.add("servers", VPackValue(VPackValueType::Object));
      // we come first!
      result.add(ServerState::instance()->getId(), hostInfo.slice());

      // now all other servers
      NetworkFeature const& nf = server().getFeature<NetworkFeature>();
      network::ConnectionPool* pool = nf.pool();
      if (pool == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }

      std::vector<network::FutureRes> futures;

      network::RequestOptions options;
      options.timeout = network::Timeout(30.0);
      options.database = _request->databaseName();
      options.param("local", "true");
      options.param("support", "true");

      size_t coordinators = 0;
      size_t dbServers = 0;
      size_t agents = 0;

      ClusterInfo& ci = server().getFeature<ClusterFeature>().clusterInfo();
      for (auto const& server : ci.getServers()) {
        if (server.first.compare(0, 4, "CRDN", 4) == 0) {
          ++coordinators;
        } else if (server.first.compare(0, 4, "PRMR", 4) == 0) {
          ++dbServers;
        } else if (server.first.compare(0, 4, "SNGL", 4) == 0) {
          // SNGL counts as DB server here
          TRI_ASSERT(isActiveFailover);
          ++dbServers;
        }
        if (server.first == ServerState::instance()->getId()) {
          // ourselves!
          continue;
        }

        auto f = network::sendRequestRetry(
            pool, "server:" + server.first, fuerte::RestVerb::Get,
            _request->requestPath(), VPackBuffer<uint8_t>{}, options,
            ::buildHeaders());
        futures.emplace_back(std::move(f));
      }

      if (!futures.empty()) {
        auto responses = futures::collectAll(futures).get();
        for (auto const& it : responses) {
          auto& resp = it.get();
          auto res = resp.combinedResult();
          if (res.fail()) {
            THROW_ARANGO_EXCEPTION(res);
          }

          auto slice = resp.slice();
          // copy results from other server
          if (slice.isObject()) {
            result.add(
                basics::StringUtils::replace(resp.destination, "server:", ""),
                slice.get("host"));
          }
        }
      }

      result.close();  // servers

      auto manager = AsyncAgencyCommManager::INSTANCE.get();
      if (manager != nullptr) {
        agents = manager->endpoints().size();
      }

      result.add("agents", VPackValue(agents));
      result.add("coordinators", VPackValue(coordinators));
      result.add("dbServers", VPackValue(dbServers));

      if (ServerState::instance()->isCoordinator()) {
        result.add(VPackValue("shards"));
        ci.getShardStatisticsGlobal(/*restrictServer*/ "", result);
      }

      result.close();  // deployment

      result.add("date", VPackValue(timeString));
    } else {
      // DB server or other coordinator
      result.add("host", hostInfo.slice());
    }
  }

  result.close();

  generateResult(rest::ResponseCode::OK, result.slice());
  return RestStatus::DONE;
}

void RestSupportInfoHandler::buildHostInfo(VPackBuilder& result) {
  bool const isActiveFailover =
      server().getFeature<ReplicationFeature>().isActiveFailoverEnabled();

  result.openObject();

  if (ServerState::instance()->isRunningInCluster() || isActiveFailover) {
    result.add("id", VPackValue(ServerState::instance()->getId()));
    result.add("alias", VPackValue(ServerState::instance()->getShortName()));
    result.add("endpoint", VPackValue(ServerState::instance()->getEndpoint()));
  }

  result.add("role", VPackValue(ServerState::roleToString(
                         ServerState::instance()->getRole())));
  result.add("maintenance",
             VPackValue(ServerState::instance()->isMaintenance()));
  result.add("readOnly", VPackValue(ServerState::instance()->readOnly()));

  result.add("version", VPackValue(ARANGODB_VERSION));
  result.add("build", VPackValue(Version::getBuildRepository()));
#ifdef USE_ENTERPRISE
  result.add("license", VPackValue("enterprise"));
#else
  result.add("license", VPackValue("community"));
#endif

  EnvironmentFeature const& ef = server().getFeature<EnvironmentFeature>();
  result.add("os", VPackValue(ef.operatingSystem()));
  result.add("platform", VPackValue(Version::getPlatform()));

  result.add("physicalMemory", VPackValue(VPackValueType::Object));
  result.add("value", VPackValue(PhysicalMemory::getValue()));
  result.add("overridden", VPackValue(PhysicalMemory::overridden()));
  result.close();  // physical memory

  result.add("numberOfCores", VPackValue(VPackValueType::Object));
  result.add("value", VPackValue(NumberOfCores::getValue()));
  result.add("overridden", VPackValue(NumberOfCores::overridden()));
  result.close();  // number of cores

  result.add("processStats", VPackValue(VPackValueType::Object));
  ServerStatistics const& serverInfo =
      server().getFeature<MetricsFeature>().serverStatistics();
  result.add("processUptime", VPackValue(serverInfo.uptime()));

  ProcessInfo info = TRI_ProcessInfoSelf();
  result.add("numberOfThreads", VPackValue(info._numberThreads));
  result.add("virtualSize", VPackValue(info._virtualSize));
  result.add("residentSetSize", VPackValue(info._residentSize));
  result.close();  // processStats

  CpuUsageFeature& cpuUsage = server().getFeature<CpuUsageFeature>();
  if (cpuUsage.isEnabled()) {
    auto snapshot = cpuUsage.snapshot();
    result.add("cpuStats", VPackValue(VPackValueType::Object));
    result.add("userPercent", VPackValue(snapshot.userPercent()));
    result.add("systemPercent", VPackValue(snapshot.systemPercent()));
    result.add("idlePercent", VPackValue(snapshot.idlePercent()));
    result.add("iowaitPercent", VPackValue(snapshot.iowaitPercent()));
    result.close();  // cpustats
  }

  if (!ServerState::instance()->isCoordinator()) {
    result.add("engineStats", VPackValue(VPackValueType::Object));
    VPackBuilder stats;
    StorageEngine& engine =
        server().getFeature<EngineSelectorFeature>().engine();
    engine.getStatistics(stats, /*v2*/ true);

    auto names = {
        // edge cache
        "cache.limit",
        "cache.allocated",
        // sizes
        "rocksdb.estimate-num-keys",
        "rocksdb.estimate-live-data-size",
        "rocksdb.live-sst-files-size",
        // block cache
        "rocksdb.block-cache-capacity",
        "rocksdb.block-cache-usage",
        // disk
        "rocksdb.free-disk-space",
        "rocksdb.total-disk-space",
    };
    for (auto const& name : names) {
      if (auto slice = stats.slice().get(name); !slice.isNone()) {
        result.add(name, slice);
      }
    }
    result.close();  // engineStats
  }

  result.close();
}
