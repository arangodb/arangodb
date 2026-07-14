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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "SupportInfoBuilder.h"

#include "Agency/AsyncAgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/process-utils.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Rest/Version.h"
#include "RestServer/CpuUsageFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/EnvironmentFeature.h"
#include "RestServer/FileDescriptorsFeature.h"
#include "Metrics/MetricsFeature.h"
#include "StorageEngine/StorageEngine.h"

#include <velocypack/Builder.h>

using namespace arangodb;
using namespace arangodb::rest;
using namespace std::literals;
using application_features::ApplicationServer;

void SupportInfoBuilder::buildInfoMessage(VPackBuilder& result,
                                          std::string const& dbName,
                                          ApplicationServer& server,
                                          bool isLocal) {
  bool isSingleServer = ServerState::instance()->isSingleServer();
  // used for all types of responses
  VPackBuilder hostInfo;

  auto& environment = server.getFeature<EnvironmentFeature>();
  auto& fileDescriptors = server.getFeature<FileDescriptorsFeature>();
  auto& cpuUsage = server.getFeature<CpuUsageFeature>();
  auto& databaseFeature = server.getFeature<DatabaseFeature>();
  buildHostInfo(hostInfo, environment, fileDescriptors, cpuUsage,
                databaseFeature);

  std::string timeString;
  LogTimeFormats::writeTime(timeString,
                            LogTimeFormats::TimeFormat::UTCDateString,
                            std::chrono::system_clock::now());

  bool fanout = ServerState::instance()->isCoordinator() && !isLocal;

  result.openObject();

  if (isSingleServer) {
    result.add("deployment", VPackValue(VPackValueType::Object));
    result.add("type", VPackValue("single"));
    result.close();  // deployment
    result.add("host", hostInfo.slice());
    result.add("date", VPackValue(timeString));
  } else {
    // cluster
    if (fanout) {
      result.add("deployment", VPackValue(VPackValueType::Object));
      TRI_ASSERT(ServerState::instance()->isCoordinator());
      result.add("type", VPackValue("cluster"));

      // build results for all servers
      // we come first!
      auto serverId = ServerState::instance()->getId();
      result.add("servers", VPackValue(VPackValueType::Object));
      result.add(serverId, hostInfo.slice());

      // now all other servers
      NetworkFeature const& nf = server.getFeature<NetworkFeature>();
      network::ConnectionPool* pool = nf.pool();
      if (pool == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }

      std::vector<network::FutureRes> futures;

      network::RequestOptions options;
      options.timeout = network::Timeout(30.0);
      options.database = dbName;
      options.param("local", "true");
      options.param("support", "true");

      size_t coordinators = 0;
      size_t dbServers = 0;
      size_t agents = 0;

      std::vector<std::string> dbServerNames;
      ClusterInfo& ci = server.getFeature<ClusterFeature>().clusterInfo();
      for (auto const& server : ci.getServers()) {
        if (server.first.starts_with("CRDN")) {
          ++coordinators;
        } else if (server.first.starts_with("PRMR")) {
          dbServerNames.emplace_back(server.first);
          ++dbServers;
        } else if (server.first.starts_with("SNGL")) {
          // SNGL counts as DB server here
          ++dbServers;
        }
        if (server.first == ServerState::instance()->getId()) {
          // ourselves!
          continue;
        }

        std::string reqUrl = "/_admin/support-info";
        auto f = network::sendRequestRetry(pool, "server:" + server.first,
                                           fuerte::RestVerb::Get, reqUrl,
                                           VPackBuffer<uint8_t>{}, options);
        futures.emplace_back(std::move(f));
      }

      if (!futures.empty()) {
        auto responses = futures::collectAll(futures).waitAndGet();
        for (auto const& it : responses) {
          auto& resp = it.get();
          auto res = resp.combinedResult();
          if (res.fail()) {
            LOG_TOPIC("4800b", WARN, Logger::FIXME)
                << "Failed to get server info: " << res.errorMessage();
          } else {
            auto slice = resp.slice();
            // copy results from other server
            if (slice.isObject()) {
              std::string hostId =
                  basics::StringUtils::replace(resp.destination, "server:", "");
              result.add(hostId, slice.get("host"));
            }
          }
        }
      }

      result.close();  // servers

      auto manager = AsyncAgencyCommManager::INSTANCE.get();
      if (manager != nullptr) {
        agents = manager->agents().size();
      }

      result.add("agents", VPackValue(agents));
      result.add("coordinators", VPackValue(coordinators));
      result.add("dbServers", VPackValue(dbServers));

      if (ServerState::instance()->isCoordinator()) {
        result.add(VPackValue("shards"));
        ci.getShardStatisticsGlobal("", result);
      }

      result.close();  // deployment
      result.add("date", VPackValue(timeString));

    } else {
      // DB server or other coordinator
      result.add("host", hostInfo.slice());
    }
  }
  result.close();
}

void SupportInfoBuilder::buildHostInfo(VPackBuilder& result,
                                       EnvironmentFeature const& environment,
                                       FileDescriptorsFeature& fileDescriptors,
                                       CpuUsageFeature& cpuUsage,
                                       DatabaseFeature& databaseFeature) {
  result.openObject();

  if (ServerState::instance()->isRunningInCluster()) {
    auto serverId = ServerState::instance()->getId();
    result.add("id", VPackValue(serverId));
    result.add("alias", VPackValue(ServerState::instance()->getShortName()));
    result.add("endpoint", VPackValue(ServerState::instance()->getEndpoint()));
  }

  result.add("role", VPackValue(ServerState::roleToString(
                         ServerState::instance()->getRole())));
  result.add("maintenance",
             VPackValue(ServerState::instance()->isStartupOrMaintenance()));
  result.add("readOnly", VPackValue(ServerState::instance()->readOnly()));

  result.add("version", VPackValue(ARANGODB_VERSION));
  result.add("build", VPackValue(Version::getBuildRepository()));
#ifdef USE_ENTERPRISE
  result.add("license", VPackValue("enterprise"));
#else
  result.add("license", VPackValue("community"));
#endif
  result.add("os", VPackValue(environment.operatingSystem()));
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
  result.add("processUptime",
             VPackValue(metrics::MetricsFeature::serverUptime()));

  ProcessInfo info = TRI_ProcessInfoSelf();
  result.add("numberOfThreads", VPackValue(info._numberThreads));
  result.add("virtualSize", VPackValue(info._virtualSize));
  result.add("residentSetSize", VPackValue(info._residentSize));
  result.add("fileDescrtors", VPackValue(fileDescriptors.current()));
  result.add("fileDescrtorsLimit", VPackValue(fileDescriptors.limit()));
  result.close();  // processStats

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
    StorageEngine& engine = databaseFeature.engine();
    engine.getStatistics(stats);
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
    for (auto& name : names) {
      if (auto slice = stats.slice().get(name); !slice.isNone()) {
        result.add(name, slice);
      }
    }
    result.close();  // engineStats
  }

  result.close();
}
