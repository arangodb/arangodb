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
/// @author Matthew Von-Maszewski
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "TakeoverShardLeadership.h"

#include "Agency/AgencyCommon.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/MaintenanceFeature.h"
#include "Futures/Utilities.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "Transaction/ClusterUtils.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/DatabaseGuard.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"

#include <velocypack/Compare.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

namespace {
static std::string serverPrefix("server:");

std::string stripServerPrefix(std::string const& destination) {
  TRI_ASSERT(destination.size() >= serverPrefix.size() &&
             destination.substr(0, serverPrefix.size()) == serverPrefix);
  return destination.substr(serverPrefix.size());
}
}  // namespace

TakeoverShardLeadership::TakeoverShardLeadership(MaintenanceFeature& feature,
                                                 ActionDescription const& desc)
    : ActionBase(feature, desc),
      ShardDefinition(desc.get(DATABASE), desc.get(SHARD)) {
  std::stringstream error;

  _labels.emplace(FAST_TRACK);

  if (!desc.has(COLLECTION)) {
    error << "collection must be specified. ";
  }
  TRI_ASSERT(desc.has(COLLECTION));

  if (!ShardDefinition::isValid()) {
    error << "database and shard must be specified. ";
  }

  if (!desc.has(LOCAL_LEADER)) {
    error << "local leader must be specified. ";
  }
  TRI_ASSERT(desc.has(LOCAL_LEADER));

  TRI_ASSERT(desc.has(PLAN_RAFT_INDEX));

  if (!error.str().empty()) {
    LOG_TOPIC("2aa85", ERR, Logger::MAINTENANCE)
        << "TakeoverLeadership: " << error.str();
    result(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }
}

TakeoverShardLeadership::~TakeoverShardLeadership() = default;

static void sendLeaderChangeRequests(
    network::ConnectionPool* pool, std::vector<ServerID> const& currentServers,
    std::shared_ptr<std::vector<ServerID>>& realInsyncFollowers,
    std::string const& databaseName, LogicalCollection& shard,
    std::string const& oldLeader) {
  if (pool == nullptr) {
    // nullptr happens only during controlled shutdown
    return;
  }

  std::string const& sid = ServerState::instance()->getId();

  network::RequestOptions options;
  options.database = databaseName;
  options.timeout = network::Timeout(3.0);
  options.skipScheduler = true;  // hack to speed up future.get()

  std::string const url = "/_api/replication/set-the-leader";

  std::vector<network::FutureRes> futures;
  futures.reserve(currentServers.size());

  for (auto const& srv : currentServers) {
    if (srv == sid) {
      continue;  // ignore ourself
    }
    uint64_t followingTermId = shard.followers()->newFollowingTermId(srv);
    VPackBufferUInt8 buffer;
    VPackBuilder bodyBuilder(buffer);
    {
      VPackObjectBuilder ob(&bodyBuilder);
      bodyBuilder.add("leaderId", VPackValue(sid));
      bodyBuilder.add("oldLeaderId", VPackValue(oldLeader));
      bodyBuilder.add("shard", VPackValue(shard.name()));
      bodyBuilder.add(StaticStrings::FollowingTermId,
                      VPackValue(followingTermId));
    }

    LOG_TOPIC("42516", DEBUG, Logger::MAINTENANCE)
        << "Sending " << bodyBuilder.toJson() << " to " << srv;
    auto f = network::sendRequest(pool, "server:" + srv, fuerte::RestVerb::Put,
                                  url, buffer, options);
    futures.emplace_back(std::move(f));
  }

  auto responses = futures::collectAll(futures).get();

  // This code intentionally ignores all errors
  realInsyncFollowers = std::make_shared<std::vector<ServerID>>();
  for (auto const& res : responses) {
    if (res.hasValue() && res.get().ok()) {
      auto& result = res.get();
      if (result.statusCode() == fuerte::StatusOK) {
        realInsyncFollowers->push_back(::stripServerPrefix(result.destination));
      }
    }
  }
}

static void handleLeadership(uint64_t planIndex, LogicalCollection& collection,
                             std::string const& localLeader,
                             std::string const& databaseName,
                             MaintenanceFeature& feature) {
  auto& followers = collection.followers();

  if (!localLeader.empty()) {  // We were not leader, assume leadership
    LOG_TOPIC("5632f", DEBUG, Logger::MAINTENANCE)
        << "handling leadership of shard '" << databaseName << "/"
        << collection.name() << ": becoming leader";

    auto& clusterFeature =
        collection.vocbase().server().getFeature<ClusterFeature>();
    auto& ci = clusterFeature.clusterInfo();
    auto& agencyCache = clusterFeature.agencyCache();

    // The following will block the thread until our ClusterInfo cache
    // fetched a Current version in background thread which is at least
    // as new as the Plan which brought us here. This is important for
    // the assertion below where we check that we are in the list of
    // failoverCandidates!
    uint64_t currVersion = 0;
    while (!collection.vocbase().server().isStopping()) {
      VPackBuilder builder;
      uint64_t raftIndex = agencyCache.get(builder, "Current/Version");
      if (!builder.isEmpty()) {
        VPackSlice currVersionSlice = builder.slice();
        try {
          currVersion = currVersionSlice.getNumber<uint64_t>();
        } catch (std::exception const&) {
        }
      }
      LOG_TOPIC("fe221", DEBUG, Logger::MAINTENANCE)
          << "TakeoverShardLeadership: read Current version " << currVersion
          << " at raft index " << raftIndex << ", planIndex=" << planIndex;
      if (raftIndex >= planIndex) {
        // Good, the index at which we are reading has at least the Plan
        // change which brought us here.
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    LOG_TOPIC("fe222", DEBUG, Logger::MAINTENANCE)
        << "Waiting until ClusterInfo has version " << currVersion;
    ci.waitForCurrentVersion(currVersion).get();

    auto currentInfo = ci.getCollectionCurrent(
        databaseName, std::to_string(collection.planId().id()));
    if (currentInfo == nullptr) {
      // Collection has been dropped. we cannot continue here.
      return;
    }
    TRI_ASSERT(currentInfo != nullptr);
    std::vector<ServerID> currentServers =
        currentInfo->servers(collection.name());
    std::shared_ptr<std::vector<ServerID>> realInsyncFollowers;

    if (!currentServers.empty()) {
      std::string& oldLeader = currentServers[0];
      // Check if the old leader has resigned and stopped all write
      // (if so, we can assume that all servers are still in sync)
      if (!oldLeader.empty() && oldLeader[0] == '_') {
        // remove the underscore from the list as it is useless anyway
        oldLeader = oldLeader.substr(1);

        // Update all follower and tell them that we are the leader now
        NetworkFeature& nf =
            collection.vocbase().server().getFeature<NetworkFeature>();
        network::ConnectionPool* pool = nf.pool();
        sendLeaderChangeRequests(pool, currentServers, realInsyncFollowers,
                                 databaseName, collection, oldLeader);
      }
    }

    std::vector<ServerID> failoverCandidates =
        currentInfo->failoverCandidates(collection.name());
    followers->takeOverLeadership(failoverCandidates, realInsyncFollowers);
    transaction::cluster::abortFollowerTransactionsOnShard(collection.id());
  }
}

bool TakeoverShardLeadership::first() {
  std::string const& database = getDatabase();
  std::string const& collection = _description.get(COLLECTION);
  std::string const& shard = getShard();
  std::string const& localLeader = _description.get(LOCAL_LEADER);
  std::string const& planRaftIndex = _description.get(PLAN_RAFT_INDEX);
  uint64_t planIndex = basics::StringUtils::uint64(planRaftIndex);
  Result res;

  try {
    auto& df = _feature.server().getFeature<DatabaseFeature>();
    DatabaseGuard guard(df, database);
    auto& vocbase = guard.database();

    std::shared_ptr<LogicalCollection> coll;
    Result found = methods::Collections::lookup(vocbase, shard, coll);
    if (found.ok()) {
      TRI_ASSERT(coll);
      LOG_TOPIC("5632a", DEBUG, Logger::MAINTENANCE)
          << "handling leadership of shard '" << database << "/" << shard;
      // We adjust local leadership, note that the planned
      // resignation case is not handled here, since then
      // ourselves does not appear in shards[shard] but only
      // "_" + ourselves.
      handleLeadership(planIndex, *coll, localLeader, vocbase.name(),
                       feature());
    } else {
      std::stringstream error;
      error << "TakeoverShardLeadership: failed to lookup local collection "
            << shard << "in database " + database;
      LOG_TOPIC("65342", ERR, Logger::MAINTENANCE) << error.str();
      res = actionError(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, error.str());
      result(res);
    }
  } catch (std::exception const& e) {
    std::stringstream error;

    error << "action " << _description << " failed with exception " << e.what();
    LOG_TOPIC("79443", WARN, Logger::MAINTENANCE)
        << "TakeoverShardLeadership: " << error.str();
    res.reset(TRI_ERROR_INTERNAL, error.str());
    result(res);
  }

  if (res.fail()) {
    _feature.storeShardError(database, collection, shard,
                             _description.get(SERVER_ID), res);
  }

  return false;
}

void TakeoverShardLeadership::setState(ActionState state) {
  if ((COMPLETE == state || FAILED == state) && _state != state) {
    _feature.unlockShard(getShard());
  }
  ActionBase::setState(state);
}
