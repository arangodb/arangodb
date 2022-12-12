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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "SupportInfoBuilder.h"

#include "Agency/AsyncAgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/process-utils.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Indexes/Index.h"
#include "Metrics/MetricsFeature.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Replication/ReplicationFeature.h"
#include "Rest/Version.h"
#include "RestServer/CpuUsageFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/EnvironmentFeature.h"
#include "Statistics/ServerStatistics.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/DatabaseGuard.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"
#include "VocBase/Methods/Indexes.h"

#include <velocypack/Builder.h>

#include "Logger/LogMacros.h"

using namespace arangodb;
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

void SupportInfoBuilder::buildInfoMessage(VPackBuilder& result,
                                          std::string const& dbName,
                                          ArangodServer& server, bool isLocal,
                                          bool detailed) {
  bool isSingleServer = ServerState::instance()->isSingleServer();
  // used for all types of responses
  VPackBuilder hostInfo;

  buildHostInfo(hostInfo, server);

  std::string timeString;
  LogTimeFormats::writeTime(timeString,
                            LogTimeFormats::TimeFormat::UTCDateString,
                            std::chrono::system_clock::now());

  bool const isActiveFailover =
      server.getFeature<ReplicationFeature>().isActiveFailoverEnabled();
  bool fanout =
      (ServerState::instance()->isCoordinator() || isActiveFailover) &&
      !isLocal;

  result.openObject();

  if (isSingleServer && !isActiveFailover) {
    result.add("deployment", VPackValue(VPackValueType::Object));
    result.add("type", VPackValue("single"));
    result.close();  // deployment
    result.add("host", hostInfo.slice());
    result.add("date", VPackValue(timeString));

    if (detailed) {
      buildDbServerInfo(result, server);
    }

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
          TRI_ASSERT(isActiveFailover);
          ++dbServers;
        }
        if (server.first == ServerState::instance()->getId()) {
          // ourselves!
          continue;
        }

        auto f = network::sendRequestRetry(
            pool, "server:" + server.first, fuerte::RestVerb::Get,
            "/_admin/support-info", VPackBuffer<uint8_t>{}, options,
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
        ci.getShardStatisticsGlobal("", result);
      }
      result.close();  // deployment
      result.add("date", VPackValue(timeString));

      if (detailed) {
        futures.clear();
        for (auto const& serverName : dbServerNames) {
          auto f = network::sendRequestRetry(
              pool, "server:" + serverName, fuerte::RestVerb::Get,
              "/_admin/server-info", VPackBuffer<uint8_t>{}, options,
              ::buildHeaders());
          futures.emplace_back(std::move(f));
        }
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
            result.add("databases", slice.get("databases"));
          }
        }
      }

    } else {
      // DB server or other coordinator
      result.add("host", hostInfo.slice());
    }
  }

  /*
  DatabaseFeature& dbFeature = server.getFeature<DatabaseFeature>();
  std::vector<std::string> databases = methods::Databases::list(server, "");
  result.add("numDatabases", VPackValue(databases.size()));

  size_t numSingleShards = 0;
  try {
    result.add("databases", VPackValue(VPackValueType::Array));

    for (auto const& database : databases) {
      auto vocbase = dbFeature.lookupDatabase(database);
      result.openObject();
      result.add("name", VPackValue(database));

      bool isSingleShard = true;
      size_t numColls = 0;
      size_t numGraphColls = 0;
      size_t numSmartColls = 0;
      result.add("collections", VPackValue(VPackValueType::Array));

      DatabaseGuard guard(dbFeature, database);
      methods::Collections::enumerate(
          &guard.database(),
          [&](std::shared_ptr<LogicalCollection> const& coll) {
            result.openObject();
            size_t numShards = coll->numberOfShards();
            if (!isSingleServer && numShards != 1) {
              isSingleShard = false;
            }
            result.add("numShards", VPackValue(numShards));

            LOG_DEVEL << "BEFORE";
            if (!isSingleServer) {
              auto shardMap = coll->shardIds();
              for (auto const& shardInfo : *(shardMap.get())) {
                LOG_DEVEL << "for " << shardInfo.first;
                for (auto const& serverId : shardInfo.second) {
                  LOG_DEVEL << serverId;
                }
              }
            }
            LOG_DEVEL << "AFTER";


            auto ctx = transaction::StandaloneContext::Create(*vocbase);

            auto& collName = coll->name();
            SingleCollectionTransaction trx(ctx, collName,
                                            AccessMode::Type::READ);

            Result res = trx.begin();

            if (res.ok()) {
              OperationOptions options(ExecContext::current());
              OperationResult opResult = trx.count(
                  collName, transaction::CountType::Normal, options);
              std::ignore = trx.finish(opResult.result);
              if (opResult.fail()) {
                LOG_TOPIC("8ae00", WARN, Logger::STATISTICS)
                    << "Failed to get number of documents: "
                    << res.errorMessage();
              } else {
                VPackBuilder builder(*(opResult.buffer));
                result.add("numDocs", VPackValue(builder.slice().getUInt()));
              }
            } else {
              LOG_TOPIC("e7497", WARN, Logger::STATISTICS)
                  << "Failed to begin transaction for getting number of "
                     "documents: "
                  << res.errorMessage();
            }

            auto collType = coll->type();
            if (collType == TRI_COL_TYPE_DOCUMENT) {
              result.add("type", VPackValue("document"));
            } else if (collType == TRI_COL_TYPE_EDGE) {
              result.add("type", VPackValue("edge"));
              numGraphColls++;
              if (coll->isSmart()) {
                numSmartColls++;
                result.add("smartGraph", VPackValue(true));
              } else {
                result.add("smartGraph", VPackValue(false));
              }
            } else {
              result.add("type", VPackValue("unknown"));
            }

            std::unordered_map<std::string, size_t> idxTypesToAmounts;

            auto flags = Index::makeFlags(Index::Serialize::Estimates,
                                          Index::Serialize::Figures);
            VPackBuilder output;
            std::ignore =
                methods::Indexes::getAll(coll.get(), flags, false, output);
            velocypack::Slice outSlice = output.slice();

            result.add("idxs", VPackValue(VPackValueType::Array));
            for (auto const it : velocypack::ArrayIterator(outSlice)) {
              result.openObject();
              if (auto figures = it.get("figures"); !figures.isNone()) {

              }
              using std::operator""sv;
              auto idxType = it.get("type").stringView();
              result.add("type", VPackValue(idxType));
              if (idxType.compare("edge"sv) == 0) {
                idxTypesToAmounts["edge"]++;
              } else if (idxType.compare("geo"sv) == 0) {
                idxTypesToAmounts["geo"]++;
              } else if (idxType.compare("hash"sv) == 0) {
                idxTypesToAmounts["hash"]++;
              } else if (idxType.compare("fulltext"sv) == 0) {
                idxTypesToAmounts["fulltext"]++;
              } else if (idxType.compare("inverted"sv) == 0) {
                idxTypesToAmounts["inverted"]++;
              } else if (idxType.compare("no-access"sv) == 0) {
                idxTypesToAmounts["no-access"]++;
              } else if (idxType.compare("persistent"sv) == 0) {
                idxTypesToAmounts["persistent"]++;
              } else if (idxType.compare("iresearch"sv) == 0) {
                idxTypesToAmounts["iresearch"]++;
              } else if (idxType.compare("skiplist"sv) == 0) {
                idxTypesToAmounts["skiplist"]++;
              } else if (idxType.compare("ttl"sv) == 0) {
                idxTypesToAmounts["ttl"]++;
              } else if (idxType.compare("zkd"sv) == 0) {
                idxTypesToAmounts["zkd"]++;
              } else if (idxType.compare("primary"sv) == 0) {
                idxTypesToAmounts["primary"]++;
              } else {
                idxTypesToAmounts["unknown"]++;
              }
              result.close();
            }
            result.close();

            for (auto const& [type, amount] : idxTypesToAmounts) {
              result.add(std::string("amount" + type), VPackValue(amount));
            }

            result.close();
          });

      result.close();

      if (isSingleShard) {
        numSingleShards++;
      }
      result.add("amountSingleShards", VPackValue(numSingleShards));
      result.add("numCollections", VPackValue(numColls));
      result.add("numGraphCollections", VPackValue(numGraphColls));
      result.add("numSmartCollections", VPackValue(numSmartColls));

      result.close();
    }

    result.close();

  } catch (const std::exception& e) {
    LOG_DEVEL << "ERROR " << e.what();
    // must ignore any errors here in case a database or collection
    // got deleted in the meantime
  }
   */
  result.close();
}

void SupportInfoBuilder::buildHostInfo(VPackBuilder& result,
                                       ArangodServer& server) {
  bool const isActiveFailover =
      server.getFeature<ReplicationFeature>().isActiveFailoverEnabled();

  result.openObject();

  if (ServerState::instance()->isRunningInCluster() || isActiveFailover) {
    result.add("id", VPackValue(ServerState::instance()->getId()));
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

  EnvironmentFeature const& ef = server.getFeature<EnvironmentFeature>();
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
      server.getFeature<metrics::MetricsFeature>().serverStatistics();
  result.add("processUptime", VPackValue(serverInfo.uptime()));

  ProcessInfo info = TRI_ProcessInfoSelf();
  result.add("numberOfThreads", VPackValue(info._numberThreads));
  result.add("virtualSize", VPackValue(info._virtualSize));
  result.add("residentSetSize", VPackValue(info._residentSize));
  result.close();  // processStats

  CpuUsageFeature& cpuUsage = server.getFeature<CpuUsageFeature>();
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
    for (auto const& name : names) {
      if (auto slice = stats.slice().get(name); !slice.isNone()) {
        result.add(name, slice);
      }
    }
    result.close();  // engineStats
  }

  result.close();
}

void SupportInfoBuilder::buildDbServerInfo(velocypack::Builder& result,
                                           ArangodServer& server) {
  bool isSingleServer = ServerState::instance()->isSingleServer();

  DatabaseFeature& dbFeature = server.getFeature<DatabaseFeature>();
  std::vector<std::string> databases = methods::Databases::list(server, "");
  result.openObject();
  result.add("numDatabases", VPackValue(databases.size()));

  // size_t numSingleShards = 0;
  try {
    result.add("databases", VPackValue(VPackValueType::Array));

    for (auto const& database : databases) {
      auto vocbase = dbFeature.lookupDatabase(database);
      result.openObject();
      result.add("name", VPackValue(database));
      auto dbViews = vocbase->views();
      result.add("numViews", VPackValue(dbViews.size()));
      /*
      result.add("views", VPackValue(VPackValueType::Array));
      for (auto const& viewInfo : dbViews) {
        result.openObject();

            result.close();
      }
      result.close();
       */

      // bool isSingleShard = true;
      size_t numColls = 0;
      size_t numGraphColls = 0;
      size_t numSmartColls = 0;
      size_t numDisjointGraphs = 0;
      result.add("collections", VPackValue(VPackValueType::Array));

      DatabaseGuard guard(dbFeature, database);
      methods::Collections::enumerate(
          &guard.database(),
          [&](std::shared_ptr<LogicalCollection> const& coll) {
            result.openObject();
            size_t numShards = coll->numberOfShards();
            /*
            if (!isSingleServer && numShards != 1) {
              isSingleShard = false;
            }
             */
            result.add("numShards", VPackValue(numShards));

            /*
            if (!isSingleServer) {
              auto shardMap = coll->shardIds();
              for (auto const& shardInfo : *(shardMap.get())) {
                LOG_DEVEL << "for " << shardInfo.first;
                for (auto const& serverId : shardInfo.second) {
                  LOG_DEVEL << serverId;
                }
              }
            }
             */

            auto ctx = transaction::StandaloneContext::Create(*vocbase);

            auto& collName = coll->name();
            SingleCollectionTransaction trx(ctx, collName,
                                            AccessMode::Type::READ);

            Result res = trx.begin();

            if (res.ok()) {
              OperationOptions options(ExecContext::current());
              OperationResult opResult =
                  trx.count(collName, transaction::CountType::Normal, options);
              std::ignore = trx.finish(opResult.result);
              if (opResult.fail()) {
                LOG_TOPIC("8ae00", WARN, Logger::STATISTICS)
                    << "Failed to get number of documents: "
                    << res.errorMessage();
              } else {
                VPackBuilder builder(*(opResult.buffer));
                result.add("numDocs", VPackValue(builder.slice().getUInt()));
              }
            } else {
              LOG_TOPIC("e7497", WARN, Logger::STATISTICS)
                  << "Failed to begin transaction for getting number of "
                     "documents: "
                  << res.errorMessage();
            }

            auto collType = coll->type();
            if (collType == TRI_COL_TYPE_DOCUMENT) {
              result.add("type", VPackValue("document"));
            } else if (collType == TRI_COL_TYPE_EDGE) {
              result.add("type", VPackValue("edge"));
              numGraphColls++;
              if (coll->isSmart()) {
                numSmartColls++;
                result.add("smartGraph", VPackValue(true));
              } else {
                result.add("smartGraph", VPackValue(false));
              }
              if (coll->isDisjoint()) {
                numDisjointGraphs++;
                result.add("disjoint", VPackValue(true));
              } else {
                result.add("disjoint", VPackValue(false));
              }
            } else {
              result.add("type", VPackValue("unknown"));
            }

            std::unordered_map<std::string, size_t> idxTypesToAmounts;

            auto flags = Index::makeFlags(Index::Serialize::Estimates,
                                          Index::Serialize::Figures);
            VPackBuilder output;
            std::ignore =
                methods::Indexes::getAll(coll.get(), flags, false, output);
            velocypack::Slice outSlice = output.slice();

            result.add("idxs", VPackValue(VPackValueType::Array));
            for (auto const it : velocypack::ArrayIterator(outSlice)) {
              result.openObject();
              if (auto figures = it.get("figures"); !figures.isNone()) {
                result.add("memory",
                           VPackValue(figures.get("memory").getUInt()));
                bool cacheInUse = figures.get("cacheInUse").getBoolean();
                result.add("cacheInUse", VPackValue(cacheInUse));
                if (cacheInUse) {
                  result.add("cacheSize",
                             VPackValue(figures.get("cacheSize").getUInt()));
                  result.add("cacheUsage",
                             VPackValue(figures.get("cacheUsage").getUInt()));
                }
              }
              using std::operator""sv;
              auto idxType = it.get("type").stringView();
              result.add("type", VPackValue(idxType));
              result.add("sparse", VPackValue(idxType));
              result.add("unique", VPackValue(idxType));
              if (idxType.compare("edge"sv) == 0) {
                idxTypesToAmounts["edge"]++;
              } else if (idxType.compare("geo"sv) == 0) {
                idxTypesToAmounts["geo"]++;
              } else if (idxType.compare("hash"sv) == 0) {
                idxTypesToAmounts["hash"]++;
              } else if (idxType.compare("fulltext"sv) == 0) {
                idxTypesToAmounts["fulltext"]++;
              } else if (idxType.compare("inverted"sv) == 0) {
                idxTypesToAmounts["inverted"]++;
              } else if (idxType.compare("no-access"sv) == 0) {
                idxTypesToAmounts["no-access"]++;
              } else if (idxType.compare("persistent"sv) == 0) {
                idxTypesToAmounts["persistent"]++;
              } else if (idxType.compare("iresearch"sv) == 0) {
                idxTypesToAmounts["iresearch"]++;
              } else if (idxType.compare("skiplist"sv) == 0) {
                idxTypesToAmounts["skiplist"]++;
              } else if (idxType.compare("ttl"sv) == 0) {
                idxTypesToAmounts["ttl"]++;
              } else if (idxType.compare("zkd"sv) == 0) {
                idxTypesToAmounts["zkd"]++;
              } else if (idxType.compare("primary"sv) == 0) {
                idxTypesToAmounts["primary"]++;
              } else {
                idxTypesToAmounts["unknown"]++;
              }
              result.close();
            }
            result.close();

            for (auto const& [type, amount] : idxTypesToAmounts) {
              result.add(std::string("amount" + type), VPackValue(amount));
            }

            result.close();
          });

      result.close();

      result.add("isSingleShard", VPackValue(vocbase->isOneShard()));
      result.add("numCollections", VPackValue(numColls));
      result.add("numGraphCollections", VPackValue(numGraphColls));
      result.add("numSmartCollections", VPackValue(numSmartColls));
      result.add("numDisjointGraphs", VPackValue(numDisjointGraphs));

      result.close();
    }

    result.close();

  } catch (const std::exception& e) {
    LOG_DEVEL << "ERROR " << e.what();
    // must ignore any errors here in case a database or collection
    // got deleted in the meantime
  }
  result.close();
}
