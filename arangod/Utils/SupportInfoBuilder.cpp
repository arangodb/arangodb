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
#include "Basics/files.h"
#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/process-utils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "Metrics/MetricsFeature.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Replication/ReplicationFeature.h"
#include "Rest/Version.h"
#include "RestServer/CpuUsageFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/EnvironmentFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "Statistics/ServerStatistics.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/DatabaseGuard.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"
#include "VocBase/Methods/Indexes.h"

#include <velocypack/Builder.h>
#include <absl/strings/str_replace.h>

#include <array>

using namespace arangodb;
using namespace arangodb::rest;
using namespace std::literals;

void SupportInfoBuilder::addDatabaseInfo(VPackBuilder& result,
                                         VPackSlice infoSlice,
                                         ArangodServer& server) {
  DatabaseFeature& dbFeature = server.getFeature<DatabaseFeature>();

  std::vector<std::string> databases = methods::Databases::list(server, "");

  containers::FlatHashMap<std::string_view, uint32_t> dbViews;
  for (auto const& database : databases) {
    auto vocbase = dbFeature.useDatabase(database);
    if (vocbase == nullptr) {
      continue;
    }

    LogicalView::enumerate(*vocbase,
                           [&dbViews, &database = std::as_const(database)](
                               LogicalView::ptr const& view) -> bool {
                             dbViews[database]++;
                             return true;
                           });
  }

  struct DbCollStats {
    size_t numDocColls;
    size_t numEdgeColls;
    size_t numSmartColls;
    size_t numDisjointSmartColls;
    VPackBuilder builder;
  };

  // because there could be replication, so we gather info from all of the
  // databases with the same name, but only add one to the response it's the
  // database by name and its own properties
  std::unordered_map<std::string_view, DbCollStats> visitedDatabases;
  // plan id of the visited collection and a set containing their already
  // visited shards
  containers::FlatHashMap<size_t, containers::FlatHashSet<std::string_view>>
      visitedColls;
  // plan id of the collection and its amount of docs
  containers::FlatHashMap<size_t, size_t> collNumDocs;
  // merge all collections belonging to the database into
  // the same object, as the database might be in more
  // than one dbserver
  for (auto dbItFromServers : velocypack::ObjectIterator(infoSlice)) {
    for (auto dbIt : velocypack::ArrayIterator(dbItFromServers.value)) {
      std::string_view dbName = dbIt.get("name").stringView();
      if (visitedDatabases.find(dbName) == visitedDatabases.end()) {
        auto insertedEl =
            visitedDatabases.insert(std::make_pair(dbName, DbCollStats()));
        VPackBuilder& dbBuilder = insertedEl.first->second.builder;
        dbBuilder.openObject();
        dbBuilder.add("n_views", VPackValue(dbViews[dbName]));
        dbBuilder.add("single_shard",
                      VPackValue(dbIt.get("single_shard").getBoolean()));
        dbBuilder.add("colls", VPackValue(VPackValueType::Array));
      }

      for (auto const collIt : velocypack::ArrayIterator(dbIt.get("colls"))) {
        size_t planId = collIt.get("plan_id").getNumber<decltype(planId)>();

        std::string_view collName = collIt.get("name").stringView();
        if (auto const visitedCollIt = visitedColls.find(planId);
            visitedCollIt != visitedColls.end()) {
          if (visitedCollIt->second.find(collName) ==
              visitedCollIt->second.end()) {
            collNumDocs[planId] += collIt.get("n_docs").getNumber<size_t>();
            visitedColls[planId].insert(collName);
          }
        } else {
          collNumDocs[planId] += collIt.get("n_docs").getNumber<size_t>();
          auto& visitedDbInfo = visitedDatabases[dbName];
          visitedDbInfo.builder.add(collIt);
          std::string_view collType = collIt.get("type").stringView();
          if (collType == "document") {
            visitedDbInfo.numDocColls++;
          } else if (collType == "edge") {
            visitedDbInfo.numEdgeColls++;
          }
          if (collIt.get("smart_graph").getBoolean()) {
            visitedDbInfo.numSmartColls++;
          }
          if (collIt.get("disjoint").getBoolean()) {
            visitedDbInfo.numDisjointSmartColls++;
          }
          visitedColls[planId].insert(collName);
        }
      }
    }
  }

  for (auto& [dbName, dbInfo] : visitedDatabases) {
    if (!dbInfo.builder.isEmpty() && dbInfo.builder.isOpenArray()) {
      dbInfo.builder.close();
    }
    if (!dbInfo.builder.isEmpty() && !dbInfo.builder.isClosed()) {
      dbInfo.builder.close();
    }
    result.openObject();
    result.add("n_doc_colls", VPackValue(dbInfo.numDocColls));
    result.add("n_edge_colls", VPackValue(dbInfo.numEdgeColls));
    result.add("n_smart_colls", VPackValue(dbInfo.numSmartColls));
    result.add("n_disjoint_smart_colls",
               VPackValue(dbInfo.numDisjointSmartColls));

    for (auto const dbIt : velocypack::ObjectIterator(dbInfo.builder.slice())) {
      std::string_view key = dbIt.key.stringView();
      auto const value = dbIt.value;
      if (key == "colls") {
        result.add("colls", VPackValue(VPackValueType::Array));
        for (auto const collIt : velocypack::ArrayIterator(value)) {
          result.openObject();
          for (auto const collIt2 : velocypack::ObjectIterator(collIt)) {
            std::string_view key2 = collIt2.key.stringView();
            auto const value2 = collIt2.value;
            if (key2 == "n_docs") {
              auto foundIt =
                  collNumDocs.find(collIt.get("plan_id").getNumber<size_t>());
              if (foundIt != collNumDocs.end()) {
                result.add(key2, VPackValue(foundIt->second));
              }
            } else if (key2 != "name") {
              result.add(key2, value2);
            }
          }
          result.close();
        }
        result.close();

      } else {
        result.add(key, value);
      }
    }
    result.close();
  }
}

// if it's a telemetrics request, keys can only have lowercase letters and
// underscores
void SupportInfoBuilder::normalizeKeyForTelemetrics(std::string& key) {
  arangodb::basics::StringUtils::tolowerInPlace(key);
  key = absl::StrReplaceAll(std::move(key), {{".", "_"}, {"-", "_"}});
}

void SupportInfoBuilder::buildInfoMessage(VPackBuilder& result,
                                          std::string const& dbName,
                                          ArangodServer& server, bool isLocal,
                                          bool isTelemetricsReq) {
  bool isSingleServer = ServerState::instance()->isSingleServer();
  auto const serverId = ServerIdFeature::getId().id();
  // used for all types of responses
  VPackBuilder hostInfo;

  buildHostInfo(hostInfo, server, isTelemetricsReq);

  std::string timeString;
  LogTimeFormats::writeTime(timeString,
                            LogTimeFormats::TimeFormat::UTCDateString,
                            std::chrono::system_clock::now());

  bool fanout = ServerState::instance()->isCoordinator() && !isLocal;

  result.openObject();

  if (isSingleServer) {
    result.add("deployment", VPackValue(VPackValueType::Object));
    if (isTelemetricsReq) {
      // it's single server, but we maintain the format the same as cluster
      std::string envValue;
      TRI_GETENV("ARANGODB_STARTUP_MODE", envValue);
      result.add("startup_mode", VPackValue(envValue));
      result.add("persisted_id",
                 VPackValue("single_" + std::to_string(serverId)));
#ifdef USE_ENTERPRISE
      result.add("license", VPackValue("enterprise"));
#else
      result.add("license", VPackValue("community"));
#endif
      result.add("servers", VPackValue(VPackValueType::Array));
      result.add(hostInfo.slice());
      result.close();  // servers
      result.add("date", VPackValue(timeString));
      VPackBuilder serverInfo;
      buildDbServerDataStoredInfo(serverInfo, server);
      result.add("databases", VPackValue(VPackValueType::Array));
      VPackBuilder dbInfoBuilder;
      dbInfoBuilder.openObject();
      dbInfoBuilder.add(std::to_string(serverId),
                        serverInfo.slice().get("databases"));
      dbInfoBuilder.close();
      addDatabaseInfo(result, dbInfoBuilder.slice(), server);
      result.close();
    }
    result.add("type", VPackValue("single"));
    result.close();  // deployment
    if (!isTelemetricsReq) {
      result.add("host", hostInfo.slice());
      result.add("date", VPackValue(timeString));
    }
  } else {
    // cluster
    if (fanout) {
      result.add("deployment", VPackValue(VPackValueType::Object));
      if (isTelemetricsReq) {
#ifdef USE_ENTERPRISE
        result.add("license", VPackValue("enterprise"));
#else
        result.add("license", VPackValue("community"));
#endif
        if (ServerState::instance()->hasPersistedId()) {
          result.add("persisted_id",
                     VPackValue(arangodb::basics::StringUtils::tolower(
                         ServerState::instance()->getPersistedId())));
        } else {
          result.add("persisted_id",
                     VPackValue("id" + std::to_string(serverId)));
        }

        std::string envValue;
        TRI_GETENV("ARANGODB_STARTUP_MODE", envValue);
        result.add("startup_mode", VPackValue(envValue));
      }
      TRI_ASSERT(ServerState::instance()->isCoordinator());
      result.add("type", VPackValue("cluster"));

      // build results for all servers
      // we come first!
      auto serverId = ServerState::instance()->getId();
      if (isTelemetricsReq) {
        result.add("servers", VPackValue(VPackValueType::Array));
        normalizeKeyForTelemetrics(serverId);
        result.add(hostInfo.slice());
      } else {
        result.add("servers", VPackValue(VPackValueType::Object));
        result.add(serverId, hostInfo.slice());
      }

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

        std::string reqUrl =
            isTelemetricsReq ? "/_admin/telemetrics" : "/_admin/support-info";
        auto f = network::sendRequestRetry(pool, "server:" + server.first,
                                           fuerte::RestVerb::Get, reqUrl,
                                           VPackBuffer<uint8_t>{}, options);
        futures.emplace_back(std::move(f));
      }

      VPackBuilder dbInfoBuilder;
      if (!futures.empty()) {
        dbInfoBuilder.openObject();
        auto responses = futures::collectAll(futures).waitAndGet();
        for (auto const& it : responses) {
          auto& resp = it.get();
          auto res = resp.combinedResult();
          if (res.fail()) {
            LOG_TOPIC("4800b", WARN, Logger::STATISTICS)
                << "Failed to get server info: " << res.errorMessage();
          } else {
            auto slice = resp.slice();
            // copy results from other server
            if (slice.isObject()) {
              std::string hostId =
                  basics::StringUtils::replace(resp.destination, "server:", "");
              if (isTelemetricsReq) {
                result.add(slice.get("host"));
              } else {
                result.add(hostId, slice.get("host"));
              }
              if (isTelemetricsReq) {
                auto const databasesSlice = slice.get("databases");
                if (!databasesSlice.isNone()) {
                  dbInfoBuilder.add(hostId, databasesSlice);
                }
              }
            }
          }
        }
        dbInfoBuilder.close();
      }

      result.close();  // servers

      auto manager = AsyncAgencyCommManager::INSTANCE.get();
      if (manager != nullptr) {
        agents = manager->endpoints().size();
      }

      result.add("agents", VPackValue(agents));
      result.add("coordinators", VPackValue(coordinators));
      std::string dbServersKey = "dbServers";
      std::string shardsStatitsics = "shards";
      if (isTelemetricsReq) {
        dbServersKey = "db_servers";
        shardsStatitsics = "shards_statistics";
      }
      result.add(dbServersKey, VPackValue(dbServers));

      if (ServerState::instance()->isCoordinator()) {
        result.add(VPackValue(shardsStatitsics));
        ci.getShardStatisticsGlobal("", result);
      }

      if (isTelemetricsReq) {
        result.add("date", VPackValue(timeString));
        result.add("databases", VPackValue(VPackValueType::Array));
        if (!dbInfoBuilder.isEmpty()) {
          addDatabaseInfo(result, dbInfoBuilder.slice(), server);
        }
        result.close();
      }

      result.close();  // deployment
      if (!isTelemetricsReq) {
        result.add("date", VPackValue(timeString));
      }

    } else {
      // DB server or other coordinator
      result.add("host", hostInfo.slice());
      if (isTelemetricsReq && !ServerState::instance()->isCoordinator()) {
        VPackBuilder serverInfo;
        buildDbServerDataStoredInfo(serverInfo, server);
        result.add("databases", serverInfo.slice().get("databases"));
      }
    }
  }
  result.close();
}

void SupportInfoBuilder::buildHostInfo(VPackBuilder& result,
                                       ArangodServer& server,
                                       bool isTelemetricsReq) {
  result.openObject();

  if (isTelemetricsReq || ServerState::instance()->isRunningInCluster()) {
    auto serverId = ServerState::instance()->getId();
    if (isTelemetricsReq) {
      bool isSingleServer = ServerState::instance()->isSingleServer();
      if (isSingleServer) {
        auto const serverIdSingle = ServerIdFeature::getId().id();
        result.add("id",
                   VPackValue("single_" + std::to_string(serverIdSingle)));
      } else {
        normalizeKeyForTelemetrics(serverId);
        result.add("id", VPackValue(serverId));
      }
    } else {
      result.add("id", VPackValue(serverId));
    }
    result.add("alias", VPackValue(ServerState::instance()->getShortName()));
    result.add("endpoint", VPackValue(ServerState::instance()->getEndpoint()));
  }

  result.add("role", VPackValue(ServerState::roleToString(
                         ServerState::instance()->getRole())));
  result.add("maintenance",
             VPackValue(ServerState::instance()->isStartupOrMaintenance()));
  containers::FlatHashMap<std::string_view, std::string_view> keys;

  if (isTelemetricsReq) {
    keys["readOnly"] = "read_only";
    keys["physMem"] = "phys_mem";
    keys["nCores"] = "n_cores";
    keys["processStats"] = "process_stats";
    keys["processUptime"] = "process_uptime";
    keys["nThreads"] = "n_threads";
    keys["virtualSize"] = "virtual_size";
    keys["residentSetSize"] = "resident_set_size";
    keys["engineStats"] = "engine_stats";
  } else {
    keys["readOnly"] = "readOnly";
    keys["physMem"] = "physicalMemory";
    keys["nCores"] = "numberOfCores";
    keys["processStats"] = "processStats";
    keys["processUptime"] = "processUptime";
    keys["nThreads"] = "numberOfThreads";
    keys["virtualSize"] = "virtualSize";
    keys["residentSetSize"] = "residentSetSize";
    keys["engineStats"] = "engineStats";
  }
  result.add(keys["readOnly"], VPackValue(ServerState::instance()->readOnly()));

  result.add("version", VPackValue(ARANGODB_VERSION));
  result.add("build", VPackValue(Version::getBuildRepository()));
  if (!isTelemetricsReq) {
#ifdef USE_ENTERPRISE
    result.add("license", VPackValue("enterprise"));
#else
    result.add("license", VPackValue("community"));
#endif
  }
  EnvironmentFeature const& ef = server.getFeature<EnvironmentFeature>();
  result.add("os", VPackValue(ef.operatingSystem()));
  result.add("platform", VPackValue(Version::getPlatform()));

  result.add(keys["physMem"], VPackValue(VPackValueType::Object));
  result.add("value", VPackValue(PhysicalMemory::getValue()));
  result.add("overridden", VPackValue(PhysicalMemory::overridden()));
  result.close();  // physical memory

  result.add(keys["nCores"], VPackValue(VPackValueType::Object));
  result.add("value", VPackValue(NumberOfCores::getValue()));
  result.add("overridden", VPackValue(NumberOfCores::overridden()));
  result.close();  // number of cores

  result.add(keys["processStats"], VPackValue(VPackValueType::Object));
  ServerStatistics const& serverInfo =
      server.getFeature<metrics::MetricsFeature>().serverStatistics();
  result.add(keys["processUptime"], VPackValue(serverInfo.uptime()));

  ProcessInfo info = TRI_ProcessInfoSelf();
  result.add(keys["nThreads"], VPackValue(info._numberThreads));
  result.add(keys["virtualSize"], VPackValue(info._virtualSize));
  result.add(keys["residentSetSize"], VPackValue(info._residentSize));
  result.close();  // processStats

  CpuUsageFeature& cpuUsage = server.getFeature<CpuUsageFeature>();
  if (cpuUsage.isEnabled() && !isTelemetricsReq) {
    auto snapshot = cpuUsage.snapshot();
    result.add("cpuStats", VPackValue(VPackValueType::Object));
    result.add("userPercent", VPackValue(snapshot.userPercent()));
    result.add("systemPercent", VPackValue(snapshot.systemPercent()));
    result.add("idlePercent", VPackValue(snapshot.idlePercent()));
    result.add("iowaitPercent", VPackValue(snapshot.iowaitPercent()));
    result.close();  // cpustats
  }

  if (!ServerState::instance()->isCoordinator()) {
    result.add(keys["engineStats"], VPackValue(VPackValueType::Object));
    VPackBuilder stats;
    StorageEngine& engine = server.getFeature<EngineSelectorFeature>().engine();
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
      std::string newName(name);
      if (isTelemetricsReq) {
        normalizeKeyForTelemetrics(newName);
      }
      if (auto slice = stats.slice().get(name); !slice.isNone()) {
        result.add(newName, slice);
      } else if (isTelemetricsReq) {
        result.add(newName, VPackValue(0));
      }
    }
    result.close();  // engineStats
  }

  result.close();
}

void SupportInfoBuilder::buildDbServerDataStoredInfo(
    velocypack::Builder& result, ArangodServer& server) {
  DatabaseFeature& dbFeature = server.getFeature<DatabaseFeature>();
  std::vector<std::string> databases = methods::Databases::list(server, "");

  ExecContextSuperuserScope ctxScope;

  result.openObject();

  result.add("databases", VPackValue(VPackValueType::Array));
  for (auto const& database : databases) {
    auto vocbase = dbFeature.useDatabase(database);
    if (vocbase == nullptr) {
      continue;
    }

    result.openObject();
    result.add("name", database);

    size_t numDocColls = 0;
    size_t numEdgeColls = 0;
    size_t numSmartColls = 0;
    size_t numDisjointSmartColls = 0;

    result.add("colls", VPackValue(VPackValueType::Array));

    containers::FlatHashSet<size_t> collsAlreadyVisited;

    DatabaseGuard guard(dbFeature, database);
    methods::Collections::enumerate(
        &guard.database(), [&](std::shared_ptr<LogicalCollection> const& coll) {
          result.openObject();
          size_t numShards = coll->numberOfShards();
          result.add("n_shards", VPackValue(numShards));
          result.add("rep_factor", VPackValue(coll->replicationFactor()));

          auto& collName = coll->name();
          result.add("name", VPackValue(collName));
          size_t planId = coll->planId().id();
          result.add("plan_id", VPackValue(planId));

          auto origin =
              transaction::OperationOriginInternal{"counting document(s)"};
          auto ctx = transaction::StandaloneContext::create(*vocbase, origin);

          SingleCollectionTransaction trx(std::move(ctx), collName,
                                          AccessMode::Type::READ);

          Result res = trx.begin();

          if (res.ok()) {
            OperationOptions options(ExecContext::current());

            OperationResult opResult =
                trx.count(collName, transaction::CountType::kNormal, options);
            std::ignore = trx.finish(opResult.result);
            if (opResult.fail()) {
              LOG_TOPIC("8ae00", WARN, Logger::STATISTICS)
                  << "Failed to get number of documents: "
                  << res.errorMessage();
            } else {
              result.add("n_docs", VPackSlice(opResult.buffer->data()));
            }

          } else {
            LOG_TOPIC("e7497", WARN, Logger::STATISTICS)
                << "Failed to begin transaction for getting number of "
                   "documents: "
                << res.errorMessage();
          }

          if (collsAlreadyVisited.find(planId) == collsAlreadyVisited.end()) {
            auto collType = coll->type();
            if (collType == TRI_COL_TYPE_EDGE) {
              result.add("type", VPackValue("edge"));
              numEdgeColls++;
            } else {
              TRI_ASSERT(collType == TRI_COL_TYPE_DOCUMENT);
              result.add("type", VPackValue("document"));
              numDocColls++;
            }
            if (coll->isSmart()) {
              numSmartColls++;
              result.add("smart_graph", VPackValue(true));
            } else {
              result.add("smart_graph", VPackValue(false));
            }
            if (coll->isDisjoint()) {
              numDisjointSmartColls++;
              result.add("disjoint", VPackValue(true));
            } else {
              result.add("disjoint", VPackValue(false));
            }

            containers::FlatHashMap<std::string, size_t> idxTypesToAmounts;

            auto flags = Index::makeFlags(Index::Serialize::Estimates,
                                          Index::Serialize::Figures);
            constexpr std::array<std::string_view, 13> idxTypes = {
                "edge",       "geo",      "hash",   "fulltext", "inverted",
                "persistent", "skiplist", "ttl",    "mdi",      "mdi-prefixed",
                "iresearch",  "primary",  "unknown"};
            for (auto const& type : idxTypes) {
              idxTypesToAmounts.try_emplace(type, 0);
            }

            VPackBuilder output;
            std::ignore = methods::Indexes::getAll(*coll, flags, true, output);
            velocypack::Slice outSlice = output.slice();

            result.add("idxs", VPackValue(VPackValueType::Array));
            for (auto const it : velocypack::ArrayIterator(outSlice)) {
              auto idxType = it.get("type").stringView();
              result.openObject();

              if (auto figures = it.get("figures"); !figures.isNone()) {
                auto idxSlice = figures.get("memory");
                uint64_t memUsage = 0;
                if (!idxSlice.isNone()) {
                  memUsage = idxSlice.getNumber<decltype(memUsage)>();
                }
                result.add("mem", VPackValue(memUsage));
                idxSlice = figures.get("cache_in_use");
                bool cacheInUse = false;
                if (!idxSlice.isNone()) {
                  cacheInUse = idxSlice.getBoolean();
                }
                uint64_t cacheSize = 0;
                uint64_t cacheUsage = 0;
                if (cacheInUse) {
                  cacheSize = figures.get("cache_size")
                                  .getNumber<decltype(cacheSize)>();
                  cacheUsage = figures.get("cache_size")
                                   .getNumber<decltype(cacheUsage)>();
                }
                result.add("cache_size", VPackValue(cacheSize));
                result.add("cache_usage", VPackValue(cacheUsage));
              }

              if (idxType == "arangosearch") {
                // this is because the telemetrics parser for the object in the
                // endpoint should contain the arangosearch index with the name
                // "iresearch" hardcoded, so, until it's changed, we write the
                // amounts of indexes of this type as "iresearch" in the object
                idxType = "iresearch";
              }

              if (idxType == "geo1" || idxType == "geo2") {
                // older deployments can have indexes of type "geo1"
                // and "geo2". these are old names for "geo" indexes with
                // specific setting. simply rename them to "geo" here.
                idxType = "geo";
              }
              result.add("type", VPackValue(idxType));
              bool isSparse = false;
              auto idxSlice = it.get("sparse");
              if (!idxSlice.isNone()) {
                isSparse = idxSlice.getBoolean();
              }
              bool isUnique = false;
              idxSlice = it.get("unique");
              if (!idxSlice.isNone()) {
                isUnique = idxSlice.getBoolean();
              }
              result.add("sparse", VPackValue(isSparse));
              result.add("unique", VPackValue(isUnique));
              idxTypesToAmounts[idxType]++;

              result.close();
            }
            result.close();
            collsAlreadyVisited.insert(planId);
            for (auto const& [type, amount] : idxTypesToAmounts) {
              result.add({"n_" + type}, VPackValue(amount));
            }
          }

          result.close();
        });

    result.close();

    result.add("single_shard", VPackValue(vocbase->isOneShard()));
    result.add("n_doc_colls", VPackValue(numDocColls));
    result.add("n_edge_colls", VPackValue(numEdgeColls));
    result.add("n_smart_colls", VPackValue(numSmartColls));
    result.add("n_disjoint_smart_colls", VPackValue(numDisjointSmartColls));

    result.close();
  }
  result.close();

  result.close();
}
