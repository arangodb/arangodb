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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Futures/Future.h"
#include "Futures/Unit.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

#include <velocypack/Slice.h>

#include <cstdint>
#include <map>
#include <utility>

namespace arangodb {
namespace cluster::rebalance {
struct AutoRebalanceProblem;
}
class RestAdminClusterHandler : public RestVocbaseBaseHandler {
 public:
  RestAdminClusterHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);
  ~RestAdminClusterHandler() override = default;

 public:
  auto executeAsync() -> futures::Future<futures::Unit> override;
  char const* name() const override final { return "RestAdminClusterHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }

 private:
  static std::string const CancelJob;
  static std::string const Health;
  static std::string const NumberOfServers;
  static std::string const UniqId;
  static std::string const Maintenance;
  static std::string const NodeVersion;
  static std::string const NodeStatistics;
  static std::string const NodeEngine;
  static std::string const Statistics;
  static std::string const ShardDistribution;
  static std::string const CollectionShardDistribution;
  static std::string const CleanoutServer;
  static std::string const ResignLeadership;
  static std::string const MoveShard;
  static std::string const QueryJobStatus;
  static std::string const RemoveServer;
  static std::string const RebalanceShards;
  static std::string const Rebalance;
  static std::string const ShardStatistics;
  static std::string const VPackSortMigration;
  static std::string const VPackSortMigrationCheck;
  static std::string const VPackSortMigrationMigrate;
  static std::string const VPackSortMigrationStatus;

  async<void> handleHealth();
  async<void> handleNumberOfServers();
  async<void> handleMaintenance();
  async<void> handleDBServerMaintenance(std::string const& serverId);

  // timeout can be used to set an arbitrary timeout for the maintenance
  // duration. it will be ignored if "state" is not true.
  async<void> setMaintenance(bool state, uint64_t timeout);
  async<void> setDBServerMaintenance(std::string const& serverId,
                                     std::string const& mode, uint64_t timeout);
  async<void> handlePutMaintenance();
  async<void> handleGetMaintenance();
  async<void> handlePutDBServerMaintenance(std::string const& serverId);
  async<void> handleGetDBServerMaintenance(std::string const& serverId);

  async<void> handleGetNumberOfServers();
  async<void> handlePutNumberOfServers();

  async<void> handleUniqId();
  async<void> handlePutUniqId();

  async<void> handleNodeVersion();
  async<void> handleNodeStatistics();
  async<void> handleNodeEngine();
  async<void> handleStatistics();

  void handleShardDistribution();
  void handleCollectionShardDistribution();
  void handleShardStatistics();

  async<void> handleCleanoutServer();
  async<void> handleResignLeadership();
  async<void> handleMoveShard();
  async<void> handleCancelJob();
  async<void> handleQueryJobStatus();

  async<void> handleRemoveServer();
  async<void> handleRebalanceShards();
  async<void> handleRebalance();
  void handleRebalanceGet();
  async<void> handleRebalanceExecute();
  async<void> handleRebalancePlan();

  async<void> handleVPackSortMigration(std::string const& subCommand);

  struct MoveShardContext {
    std::string database;
    std::string collection;
    std::string shard;
    std::string fromServer;
    std::string toServer;
    std::string collectionID;
    bool remainsFollower;
    bool tryUndo;

    MoveShardContext(std::string database, std::string collection,
                     std::string shard, std::string from, std::string to,
                     std::string collectionID, bool remainsFollower,
                     bool tryUndo = false)
        : database(std::move(database)),
          collection(std::move(collection)),
          shard(std::move(shard)),
          fromServer(std::move(from)),
          toServer(std::move(to)),
          collectionID(std::move(collectionID)),
          remainsFollower(remainsFollower),
          tryUndo(tryUndo) {}

    static std::unique_ptr<MoveShardContext> fromVelocyPack(
        arangodb::velocypack::Slice slice);
  };

  async<void> handlePostMoveShard(std::unique_ptr<MoveShardContext>&& ctx);

  async<void> handleSingleServerJob(std::string const& job);
  async<void> handleCreateSingleServerJob(std::string const& job,
                                          std::string const& server,
                                          VPackSlice body);

  typedef std::chrono::steady_clock clock;

  futures::Future<futures::Unit> waitForSupervisionState(
      bool state, std::string const& reactivationTime,
      clock::time_point startTime);

  futures::Future<futures::Unit> waitForDBServerMaintenance(
      std::string const& serverId, bool waitForMaintenance);

  struct RemoveServerContext {
    size_t tries;
    std::string server;

    explicit RemoveServerContext(std::string s)
        : tries(0), server(std::move(s)) {}
  };

  futures::Future<futures::Unit> tryDeleteServer(
      std::unique_ptr<RemoveServerContext>&& ctx);
  futures::Future<futures::Unit> retryTryDeleteServer(
      std::unique_ptr<RemoveServerContext>&& ctx);
  async<void> createMoveShard(std::unique_ptr<MoveShardContext>&& ctx,
                              velocypack::Slice plan);

  async<void> handleProxyGetRequest(std::string const& url,
                                    std::string const& serverFromParameter);
  void handleGetCollectionShardDistribution(std::string const& collection);

  async<void> handlePostRemoveServer(std::string const& server);

  std::string resolveServerNameID(std::string const&);

 public:
  struct CollectionShardPair {
    std::string collection;
    std::string shard;
    bool isLeader;

    bool operator==(CollectionShardPair const& other) const {
      return collection == other.collection && shard == other.shard &&
             isLeader == other.isLeader;
    }
  };
  void getShardDistribution(
      std::map<std::string, std::unordered_set<CollectionShardPair>>& distr);

  struct MoveShardDescription {
    std::string database;
    std::string collection;
    std::string shard;
    std::string from;
    std::string to;
    bool isLeader;
  };

  using ShardMap =
      std::map<std::string, std::unordered_set<CollectionShardPair>>;
  using ReshardAlgorithm =
      std::function<void(ShardMap&, std::vector<MoveShardDescription>&,
                         std::uint32_t, std::string)>;

 private:
  futures::Future<futures::Unit> handlePostRebalanceShards(
      ReshardAlgorithm const&);

  cluster::rebalance::AutoRebalanceProblem collectRebalanceInformation(
      std::vector<std::string> const& excludedDatabases,
      bool excludeSystemCollections);

  struct MoveShardCount {
    std::size_t todo;
    std::size_t pending;
  };

  MoveShardCount countAllMoveShardJobs();
};

template<class Inspector>
auto inspect(Inspector& f, RestAdminClusterHandler::MoveShardDescription& x) {
  return f.object(x).fields(
      f.field("collection", x.collection), f.field("database", x.database),
      f.field("shard", x.shard), f.field("from", x.from), f.field("to", x.to),
      f.field("isLeader", x.isLeader));
}

}  // namespace arangodb
