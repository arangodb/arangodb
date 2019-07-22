////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/StringUtils.h"
#include "Basics/system-functions.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "ShardDistributionReporter.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using namespace arangodb::cluster;

struct SyncCountInfo {
  bool insync;
  uint64_t total;
  uint64_t current;
  std::vector<ServerID> followers;

  SyncCountInfo() : insync(false), total(1), current(0) {}
  ~SyncCountInfo() {}
};

//////////////////////////////////////////////////////////////////////////////
/// @brief the pointer to the singleton instance
//////////////////////////////////////////////////////////////////////////////

std::shared_ptr<ShardDistributionReporter> arangodb::cluster::ShardDistributionReporter::_theInstance;

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
                                uint64_t total, uint64_t current, VPackBuilder& result) {
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

  // We have somehow invalid, mostlikely no shard has responded in time
  if (total == current) {
    // Reset current
    current = 0;
  }
  result.openObject();
  result.add("total", VPackValue(total));
  result.add("current", VPackValue(current));
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

      if (c.insync) {
        ReportShardNoProgress(s.first, s.second, aliases, result);
      } else {
        if (progress) {
          ReportShardProgress(s.first, s.second, aliases, c.total, c.current, result);
        } else {
          ReportShardNoProgress(s.first, s.second, aliases, result);
        }
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

ShardDistributionReporter::ShardDistributionReporter(std::shared_ptr<ClusterComm> cc,
                                                     ClusterInfo* ci)
    : _cc(cc), _ci(ci) {
  TRI_ASSERT(_cc != nullptr);
  TRI_ASSERT(_ci != nullptr);
}

ShardDistributionReporter::~ShardDistributionReporter() {}

std::shared_ptr<ShardDistributionReporter> ShardDistributionReporter::instance() {
  if (_theInstance == nullptr) {
    _theInstance =
        std::make_shared<ShardDistributionReporter>(ClusterComm::instance(),
                                                    ClusterInfo::instance());
  }
  return _theInstance;
}

void ShardDistributionReporter::helperDistributionForDatabase(
    std::string const& dbName, VPackBuilder& result,
    std::queue<std::shared_ptr<LogicalCollection>>& todoSyncStateCheck, double endtime,
    std::unordered_map<std::string, std::string>& aliases, bool progress) {
  if (!todoSyncStateCheck.empty()) {
    CoordTransactionID coordId = TRI_NewTickServer();
    std::unordered_map<ShardID, SyncCountInfo> counters;
    std::vector<ServerID> serversToAsk;
    while (!todoSyncStateCheck.empty()) {
      counters.clear();

      auto const col = todoSyncStateCheck.front();
      auto allShards = col->shardIds();
      auto cic = _ci->getCollectionCurrent(dbName, std::to_string(col->id()));

      // Send requests
      for (auto const& s : *(allShards.get())) {
        double timeleft = endtime - TRI_microtime();
        serversToAsk.clear();
        uint64_t requestsInFlight = 0;
        OperationID leaderOpId = 0;
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
            std::string path = "/_db/" + basics::StringUtils::urlEncode(dbName) +
                               "/_api/collection/" +
                               basics::StringUtils::urlEncode(s.first) +
                               "/count";
            auto body = std::make_shared<std::string const>();

            {
              // First Ask the leader
              std::unordered_map<std::string, std::string> headers;
              leaderOpId = _cc->asyncRequest(coordId, "server:" + s.second.at(0),
                                             rest::RequestType::GET, path, body,
                                             headers, nullptr, timeleft);
            }

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
            std::unordered_map<std::string, std::string> headers;
            for (auto const& server : serversToAsk) {
              _cc->asyncRequest(coordId, "server:" + server, rest::RequestType::GET,
                                path, body, headers, nullptr, timeleft);
              requestsInFlight++;
            }

            // Wait for responses
            // First wait for Leader
            {
              auto result = _cc->wait(coordId, leaderOpId, "");
              if (result.status != CL_COMM_RECEIVED) {
                // We did not even get count for leader, use defaults
                _cc->drop(coordId, 0, "");
                // Just in case, to get a new state
                coordId = TRI_NewTickServer();
                continue;
              }
              auto body = result.result->getBodyVelocyPack();
              VPackSlice response = body->slice();
              if (!response.isObject()) {
                LOG_TOPIC("c02b2", WARN, arangodb::Logger::CLUSTER)
                    << "Received invalid response for count. Shard "
                       "distribution "
                       "inaccurate";
                continue;
              }
              response = response.get("count");
              if (!response.isNumber()) {
                LOG_TOPIC("fe868", WARN, arangodb::Logger::CLUSTER)
                    << "Received invalid response for count. Shard "
                       "distribution "
                       "inaccurate";
                continue;
              }
              entry.total = response.getNumber<uint64_t>();
              entry.current = entry.total;  // << We use this to flip around min/max test
            }

            // Now wait for others
            while (requestsInFlight > 0) {
              auto result = _cc->wait(coordId, 0, "");
              requestsInFlight--;
              if (result.status != CL_COMM_RECEIVED) {
                // We do not care for errors of any kind.
                // We can continue here because all other requests will be
                // handled by the accumulated timeout
                continue;
              } else {
                auto body = result.result->getBodyVelocyPack();
                VPackSlice response = body->slice();
                if (!response.isObject()) {
                  LOG_TOPIC("fcbb3", WARN, arangodb::Logger::CLUSTER)
                      << "Received invalid response for count. Shard "
                         "distribution inaccurate";
                  continue;
                }
                response = response.get("count");
                if (!response.isNumber()) {
                  LOG_TOPIC("8d7b0", WARN, arangodb::Logger::CLUSTER)
                      << "Received invalid response for count. Shard "
                         "distribution inaccurate";
                  continue;
                }
                uint64_t other = response.getNumber<uint64_t>();
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

void ShardDistributionReporter::getCollectionDistributionForDatabase(
    std::string const& dbName, std::string const& colName, VPackBuilder& result) {
  double endtime = TRI_microtime() + 2.0;  // We add two seconds

  result.openObject();

  auto aliases = _ci->getServerAliases();
  auto col = _ci->getCollection(dbName, colName);

  std::queue<std::shared_ptr<LogicalCollection>> todoSyncStateCheck;

  auto allShards = col->shardIds();
  if (testAllShardsInSync(dbName, col.get(), allShards.get())) {
    ReportInSync(col.get(), allShards.get(), aliases, result);
  } else {
    todoSyncStateCheck.push(col);
  }

  const bool progress = true;
  helperDistributionForDatabase(dbName, result, todoSyncStateCheck, endtime, aliases, progress);
  result.close();
}

void ShardDistributionReporter::getDistributionForDatabase(std::string const& dbName,
                                                           VPackBuilder& result) {
  double endtime = TRI_microtime() + 2.0;  // We add two seconds

  result.openObject();

  auto aliases = _ci->getServerAliases();
  auto cols = _ci->getCollections(dbName);

  std::queue<std::shared_ptr<LogicalCollection>> todoSyncStateCheck;

  for (auto col : cols) {
    auto allShards = col->shardIds();
    if (testAllShardsInSync(dbName, col.get(), allShards.get())) {
      ReportInSync(col.get(), allShards.get(), aliases, result);
    } else {
      todoSyncStateCheck.push(col);
    }
  }

  const bool progress = false;
  helperDistributionForDatabase(dbName, result, todoSyncStateCheck, endtime, aliases, progress);
  result.close();
}

bool ShardDistributionReporter::testAllShardsInSync(std::string const& dbName,
                                                    LogicalCollection const* col,
                                                    ShardMap const* shardIds) {
  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(shardIds != nullptr);

  auto cic = _ci->getCollectionCurrent(dbName, std::to_string(col->id()));

  for (auto const& s : *shardIds) {
    auto curServers = cic->servers(s.first);

    if (s.second.empty() || curServers.empty() || !TestIsShardInSync(s.second, curServers)) {
      return false;
    }
  }

  return true;
}
