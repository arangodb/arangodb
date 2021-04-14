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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include <queue>

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Basics/system-functions.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Futures/Utilities.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "ShardDistributionReporter.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::cluster;

struct SyncCountInfo {
  bool insync;

  // number of followers _currently_ performing syncing operations
  uint32_t followersSyncing;

  // number of documents on leader
  uint64_t total;
  // number of documents on follower(s). 
  // if there is more than one follower, then this contains the
  // minimum value from all followers
  uint64_t current;
  // percent value (max. value 100) as leader progress, calculated as
  // 100.0 * (sum(num docs on followers) / num followers) / num docs on leader
  // a negative value will mean that the value has not been calculated, or
  // cannot be calculated (e.g. because of division by zero)
  double followerPercent;
  std::vector<ServerID> followers;

  SyncCountInfo() 
    : insync(false), 
      followersSyncing(0),
      total(1), 
      current(0),
      followerPercent(-1.0) {}
  ~SyncCountInfo() = default;
};

//////////////////////////////////////////////////////////////////////////////
/// @brief Static helper functions
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/// @brief Test if one shard is in sync by comparing plan and current
//////////////////////////////////////////////////////////////////////////////

static inline bool TestIsShardInSync(std::vector<ServerID> plannedServers,
                                     std::vector<ServerID> realServers) {
  // The leader at [0] must be the same, while the order of the followers must
  // be ignored.
  TRI_ASSERT(!plannedServers.empty());
  TRI_ASSERT(!realServers.empty());

  std::sort(plannedServers.begin() + 1, plannedServers.end());
  std::sort(realServers.begin() + 1, realServers.end());

  return plannedServers == realServers;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Report a single shard without progress
//////////////////////////////////////////////////////////////////////////////

static void ReportShardNoProgress(std::string const& shardId,
                                  std::vector<ServerID> const& respServers,
                                  std::unordered_map<ServerID, std::string> const& aliases,
                                  VPackBuilder& result) {
  TRI_ASSERT(result.isOpenObject());
  result.add(VPackValue(shardId));
  result.openObject();
  // We always have at least the leader
  TRI_ASSERT(!respServers.empty());
  auto respServerIt = respServers.begin();
  auto al = aliases.find(*respServerIt);
  if (al != aliases.end()) {
    result.add("leader", VPackValue(al->second));
  } else {
    result.add("leader", VPackValue(*respServerIt));
  }
  ++respServerIt;

  result.add(VPackValue("followers"));
  result.openArray();
  while (respServerIt != respServers.end()) {
    auto al = aliases.find(*respServerIt);
    if (al != aliases.end()) {
      result.add(VPackValue(al->second));
    } else {
      result.add(VPackValue(*respServerIt));
    }
    ++respServerIt;
  }
  result.close();  // followers

  result.close();  // shard
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Report a single shard with progress
//////////////////////////////////////////////////////////////////////////////

static void ReportShardProgress(std::string const& shardId,
                                std::vector<ServerID> const& respServers,
                                std::unordered_map<ServerID, std::string> const& aliases,
                                uint64_t total, uint64_t current, 
                                double followerPercent, uint32_t followersSyncing,
                                VPackBuilder& result) {
  TRI_ASSERT(result.isOpenObject());
  result.add(VPackValue(shardId));
  result.openObject();
  // We always have at least the leader
  TRI_ASSERT(!respServers.empty());
  auto respServerIt = respServers.begin();
  auto al = aliases.find(*respServerIt);
  if (al != aliases.end()) {
    result.add("leader", VPackValue(al->second));
  } else {
    result.add("leader", VPackValue(*respServerIt));
  }
  ++respServerIt;

  result.add(VPackValue("followers"));
  result.openArray();
  while (respServerIt != respServers.end()) {
    auto al = aliases.find(*respServerIt);
    if (al != aliases.end()) {
      result.add(VPackValue(al->second));
    } else {
      result.add(VPackValue(*respServerIt));
    }
    ++respServerIt;
  }
  result.close();  // followers

  result.add(VPackValue("progress"));

  // We have somehow invalid, most likely no shard has responded in time
  if (total == current) {
    // Reset current
    current = 0;
  }
  result.openObject();
  result.add("total", VPackValue(total));
  result.add("current", VPackValue(current));
  if (followerPercent >= 0.0) {
    result.add("followerPercent", VPackValue(followerPercent));
  } else {
    result.add("followerPercent", VPackSlice::nullSlice());
  }
  // number of followers _currently_ syncing this shard
  result.add("followersSyncing", VPackValue(followersSyncing));

  result.close();  // progress

  result.close();  // shard
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Report a list of leader and follower based on shardMap
//////////////////////////////////////////////////////////////////////////////

static void ReportPartialNoProgress(ShardMap const* shardIds,
                                    std::unordered_map<ServerID, std::string> const& aliases,
                                    VPackBuilder& result) {
  TRI_ASSERT(result.isOpenObject());
  for (auto const& s : *shardIds) {
    ReportShardNoProgress(s.first, s.second, aliases, result);
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Report a complete collection an the insync format
//////////////////////////////////////////////////////////////////////////////

static void ReportInSync(LogicalCollection const* col, ShardMap const* shardIds,
                         std::unordered_map<ServerID, std::string> const& aliases,
                         VPackBuilder& result) {
  TRI_ASSERT(result.isOpenObject());

  result.add(VPackValue(col->name()));

  // In this report Plan and Current are identical
  result.openObject();
  {
    // Add Plan
    result.add(VPackValue("Plan"));
    result.openObject();
    ReportPartialNoProgress(shardIds, aliases, result);
    result.close();
  }

  {
    // Add Current
    result.add(VPackValue("Current"));
    result.openObject();
    ReportPartialNoProgress(shardIds, aliases, result);
    result.close();
  }
  result.close();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Report a complete collection in the offsync format, with eventually
/// known counts
//////////////////////////////////////////////////////////////////////////////

static void ReportOffSync(LogicalCollection const* col, ShardMap const* shardIds,
                          std::unordered_map<ShardID, SyncCountInfo>& counters,
                          std::unordered_map<ServerID, std::string> const& aliases,
                          VPackBuilder& result, bool progress) {
  TRI_ASSERT(result.isOpenObject());

  result.add(VPackValue(col->name()));

  // In this report Plan and Current are identical
  result.openObject();
  {
    // Add Plan
    result.add(VPackValue("Plan"));
    result.openObject();
    for (auto const& s : *shardIds) {
      TRI_ASSERT(counters.find(s.first) != counters.end());
      auto const& c = counters[s.first];

      if (c.insync || !progress) {
        ReportShardNoProgress(s.first, s.second, aliases, result);
      } else {
        TRI_ASSERT(!c.insync);
        TRI_ASSERT(progress);
        ReportShardProgress(s.first, s.second, aliases, c.total, c.current, c.followerPercent, c.followersSyncing, result);
      }
    }
    result.close();
  }

  {
    // Add Current
    result.add(VPackValue("Current"));
    result.openObject();
    for (auto const& s : *shardIds) {
      TRI_ASSERT(counters.find(s.first) != counters.end());
      auto const& c = counters[s.first];
      if (c.insync) {
        ReportShardNoProgress(s.first, s.second, aliases, result);
      } else if (!c.followers.empty()) {
        ReportShardNoProgress(s.first, c.followers, aliases, result);
      }
    }
    result.close();
  }
  result.close();
}

ShardDistributionReporter::ShardDistributionReporter(ClusterInfo* ci, network::Sender sender)
    : _ci(ci), _send(sender) {
  TRI_ASSERT(_ci != nullptr);
}

ShardDistributionReporter::~ShardDistributionReporter() = default;

std::shared_ptr<ShardDistributionReporter> ShardDistributionReporter::instance(
    application_features::ApplicationServer& server) {
  auto& ci = server.getFeature<ClusterFeature>().clusterInfo();
  auto& nf = server.getFeature<NetworkFeature>();
  auto* pool = nf.pool();
  return std::make_shared<ShardDistributionReporter>(
      &ci,
      [pool](network::DestinationId const& d, arangodb::fuerte::RestVerb v,
             std::string const& u, velocypack::Buffer<uint8_t> b,
             network::RequestOptions const& opts, network::Headers h) -> network::FutureRes {
        return sendRequest(pool, d, v, u, std::move(b), opts, std::move(h));
      });
}

void ShardDistributionReporter::helperDistributionForDatabase(
    std::string const& dbName, VPackBuilder& result,
    std::queue<std::shared_ptr<LogicalCollection>>& todoSyncStateCheck, double endtime,
    std::unordered_map<std::string, std::string>& aliases, bool progress) {
  if (!todoSyncStateCheck.empty()) {
    std::unordered_map<ShardID, SyncCountInfo> counters;
    std::vector<ServerID> serversToAsk;
    while (!todoSyncStateCheck.empty()) {
      counters.clear();

      auto const col = todoSyncStateCheck.front();
      auto allShards = col->shardIds();
      auto cic = _ci->getCollectionCurrent(dbName, std::to_string(col->id().id()));

      // Send requests
      for (auto const& s : *(allShards.get())) {
        double timeleft = endtime - TRI_microtime();
        
        serversToAsk.clear();
        auto curServers = cic->servers(s.first);
        auto& entry = counters[s.first];  // Emplaces a new SyncCountInfo
        if (curServers.empty() || s.second.empty()) {
          // either of the two vectors is empty...
          entry.insync = false;
        } else if (TestIsShardInSync(s.second, curServers)) {
          entry.insync = true;
        } else {
          entry.followers = curServers;
          if (timeleft > 0.0) {
            std::string path = "/_api/collection/" +
                               basics::StringUtils::urlEncode(s.first) +
                               "/count";
            VPackBuffer<uint8_t> body;
            network::RequestOptions reqOpts;
            reqOpts.database = dbName;
            // make sure we have at least 1s for the timeout value. otherwise
            // other parts of the code may fail when seeing a 0s timeout.
            reqOpts.timeout = network::Timeout(std::max<double>(1.0, timeleft));

            // First Ask the leader
            network::Headers headers;
            auto leaderF = _send("server:" + s.second.at(0), fuerte::RestVerb::Get,
                                 path, body, reqOpts, headers);

            // Now figure out which servers need to be asked
            for (auto const& planned : s.second) {
              bool found = false;
              for (auto const& current : entry.followers) {
                if (current == planned) {
                  found = true;
                  break;
                }
              }
              if (!found) {
                serversToAsk.emplace_back(planned);
              }
            }

            // Ask them

            // do not only query the collection counts, but also query the shard sync job status
            // from the maintenance. note older versions will simply ignore the URL parameter and
            // will not report the sync status. the code here is prepared for this.
            reqOpts.param("checkSyncStatus", "true");
            std::vector<network::FutureRes> futures;
            futures.reserve(serversToAsk.size());
            for (auto const& server : serversToAsk) {
              auto f = _send("server:" + server, fuerte::RestVerb::Get, path,
                             body, reqOpts, headers);
              futures.emplace_back(std::move(f));
            }

            // Wait for responses
            // First wait for Leader
            {
              auto const& res = leaderF.get();
              if (res.fail()) {
                // We did not even get count for leader, use defaults
                continue;
              }

              VPackSlice slice = res.slice();
              if (!slice.isObject()) {
                LOG_TOPIC("c02b2", WARN, arangodb::Logger::CLUSTER)
                    << "Received invalid response for count. Shard "
                    << "distribution inaccurate";
                continue;
              }

              VPackSlice response = slice.get("count");
              if (!response.isNumber()) {
                LOG_TOPIC("fe868", WARN, arangodb::Logger::CLUSTER)
                    << "Received invalid response for count. Shard "
                    << "distribution inaccurate";
                continue;
              }

              entry.total = response.getNumber<uint64_t>();
              entry.current = entry.total;  // << We use this to flip around min/max test
              entry.followerPercent = -1.0; // negative values mean "unknown"
              entry.followersSyncing = 0;
            }

            {
              // for in-sync followers, pretend that they have the correct number of docs
              TRI_ASSERT(!s.second.empty());
              TRI_ASSERT((s.second.size() - 1) >= serversToAsk.size());
              uint64_t followersInSync = (s.second.size() - 1) - serversToAsk.size();
              uint64_t followerResponses = followersInSync;
              uint64_t followerTotal = followersInSync * entry.total;

              auto responses = futures::collectAll(futures).get();
              for (futures::Try<network::Response> const& response : responses) {
                if (!response.hasValue() || response.get().fail()) {
                  // We do not care for errors of any kind.
                  // We can continue here because all other requests will be
                  // handled by the accumulated timeout
                  continue;
                }

                auto const& res = response.get();
                VPackSlice slice = res.slice();
                if (!slice.isObject()) {
                  LOG_TOPIC("fcbb3", WARN, arangodb::Logger::CLUSTER)
                      << "Received invalid response for count. Shard "
                      << "distribution inaccurate";
                  continue;
                }

                VPackSlice answer = slice.get("count");
                if (!answer.isNumber()) {
                  LOG_TOPIC("8d7b0", WARN, arangodb::Logger::CLUSTER)
                      << "Received invalid response for count. Shard "
                      << "distribution inaccurate";
                  continue;
                }
              
                uint64_t other = answer.getNumber<uint64_t>();
                followerTotal += other;
                ++followerResponses;

                if (other < entry.total) {
                  // If we have more in total we need the minimum of other
                  // counts
                  if (other < entry.current) {
                    entry.current = other;
                  }
                } else {
                  // If we only have more in total we take the maximum of other
                  // counts
                  if (entry.total <= entry.current && other > entry.current) {
                    entry.current = other;
                  }
                }
               
                // check if the follower is actively working on replicating the shard.
                // note: 3.7 will not provide the "syncing" attribute. it must there
                // be treated as optional in 3.8.
                if (VPackSlice syncing = slice.get("syncing"); syncing.isBoolean() && syncing.getBoolean()) {
                  ++entry.followersSyncing;
                }
              }

              // if leader has documents and at least one follower has documents, report
              // the average percentage of follower docs / leader docs

              if (followerResponses > 0 && entry.total > 0) {
                entry.followerPercent = std::min<double>(
                    100.0, 
                    100.0 * static_cast<double>(followerTotal) / static_cast<double>(followerResponses) / static_cast<double>(entry.total)
                );
              }
            }
          }
        }
      }

      ReportOffSync(col.get(), allShards.get(), counters, aliases, result, progress);
      todoSyncStateCheck.pop();
    }
  }
}

/// @brief fetch distribution for a single collection in db
void ShardDistributionReporter::getCollectionDistributionForDatabase(
    std::string const& dbName, std::string const& colName, VPackBuilder& result) {
  std::vector<std::shared_ptr<LogicalCollection>> cols{ _ci->getCollection(dbName, colName) };
  getCollectionDistribution(dbName, cols, result, true);
}

/// @brief fetch distributions for all collections in db
void ShardDistributionReporter::getDistributionForDatabase(std::string const& dbName,
                                                           VPackBuilder& result) {
  auto cols = _ci->getCollections(dbName);
  getCollectionDistribution(dbName, cols, result, false);
}

/// @brief internal helper function to fetch distributions
void ShardDistributionReporter::getCollectionDistribution(
    std::string const& dbName, std::vector<std::shared_ptr<LogicalCollection>> const& cols, 
    VPackBuilder& result, bool progress) {

  double endtime = TRI_microtime() + 2.0;  // We add two seconds

  auto aliases = _ci->getServerAliases();
  std::queue<std::shared_ptr<LogicalCollection>> todoSyncStateCheck;

  result.openObject();
  for (auto col : cols) {
    auto allShards = col->shardIds();
    if (testAllShardsInSync(dbName, col.get(), allShards.get())) {
      ReportInSync(col.get(), allShards.get(), aliases, result);
    } else {
      todoSyncStateCheck.push(col);
    }
  }

  helperDistributionForDatabase(dbName, result, todoSyncStateCheck, endtime, aliases, progress);
  result.close();
}

bool ShardDistributionReporter::testAllShardsInSync(std::string const& dbName,
                                                    LogicalCollection const* col,
                                                    ShardMap const* shardIds) {
  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(shardIds != nullptr);

  auto cic = _ci->getCollectionCurrent(dbName, std::to_string(col->id().id()));

  for (auto const& s : *shardIds) {
    auto curServers = cic->servers(s.first);

    if (s.second.empty() || curServers.empty() || !TestIsShardInSync(s.second, curServers)) {
      return false;
    }
  }

  return true;
}
