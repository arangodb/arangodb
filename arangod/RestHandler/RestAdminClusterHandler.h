////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_REST_ADMIN_CLUSTER_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_ADMIN_CLUSTER_HANDLER_H 1

#include "Futures/Future.h"
#include "Futures/Unit.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

#include <velocypack/Slice.h>

#include <map>
#include <utility>

namespace arangodb {

class RestAdminClusterHandler : public RestVocbaseBaseHandler {
 public:
  RestAdminClusterHandler(application_features::ApplicationServer&,
                          GeneralRequest*, GeneralResponse*);
  ~RestAdminClusterHandler() override = default;

 public:
  RestStatus execute() override;
  char const* name() const final { return "RestAdminClusterHandler"; }
  RequestLane lane() const final { return RequestLane::CLIENT_SLOW; }

 private:
  static std::string const Health;
  static std::string const NumberOfServers;
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

  RestStatus handleHealth();
  RestStatus handleNumberOfServers();
  RestStatus handleMaintenance();

  RestStatus setMaintenance(bool state);
  RestStatus handlePutMaintenance();
  RestStatus handleGetMaintenance();

  RestStatus handleGetNumberOfServers();
  RestStatus handlePutNumberOfServers();

  RestStatus handleNodeVersion();
  RestStatus handleNodeStatistics();
  RestStatus handleNodeEngine();
  RestStatus handleStatistics();

  RestStatus handleShardDistribution();
  RestStatus handleCollectionShardDistribution();

  RestStatus handleCleanoutServer();
  RestStatus handleResignLeadership();
  RestStatus handleMoveShard();
  RestStatus handleQueryJobStatus();

  RestStatus handleRemoveServer();
  RestStatus handleRebalanceShards();

 private:
  struct MoveShardContext {
    std::string database;
    std::string collection;
    std::string shard;
    std::string fromServer;
    std::string toServer;
    std::string collectionID;
    bool remainsFollower;

    MoveShardContext(std::string database, std::string collection, std::string shard,
                     std::string from, std::string to, std::string collectionID,
                     bool remainsFollower)
        : database(std::move(database)),
          collection(std::move(collection)),
          shard(std::move(shard)),
          fromServer(std::move(from)),
          toServer(std::move(to)),
          collectionID(std::move(collectionID)),
          remainsFollower(true) {}

    static std::unique_ptr<MoveShardContext> fromVelocyPack(arangodb::velocypack::Slice slice);
  };

  RestStatus handlePostMoveShard(std::unique_ptr<MoveShardContext>&& ctx);

  RestStatus handleSingleServerJob(std::string const& job);
  RestStatus handleCreateSingleServerJob(std::string const& job, std::string const& server);

  typedef std::chrono::steady_clock clock;
  typedef futures::Future<futures::Unit> FutureVoid;

  FutureVoid waitForSupervisionState(bool state,
                                     clock::time_point startTime = clock::time_point());

  struct RemoveServerContext {
    size_t tries;
    std::string server;

    explicit RemoveServerContext(std::string s) : tries(0), server(std::move(s)) {}
  };

  FutureVoid tryDeleteServer(std::unique_ptr<RemoveServerContext>&& ctx);
  FutureVoid retryTryDeleteServer(std::unique_ptr<RemoveServerContext>&& ctx);
  FutureVoid createMoveShard(std::unique_ptr<MoveShardContext>&& ctx, velocypack::Slice plan);

  RestStatus handleProxyGetRequest(std::string const& url, std::string const& serverFromParameter);
  RestStatus handleGetCollectionShardDistribution(std::string const& collection);

  RestStatus handlePostRemoveServer(std::string const& server);

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
  void getShardDistribution(std::map<std::string, std::unordered_set<CollectionShardPair>>& distr);

  struct MoveShardDescription {
    std::string collection;
    std::string shard;
    std::string from;
    std::string to;
    bool isLeader;
  };

  using ShardMap = std::map<std::string, std::unordered_set<CollectionShardPair>>;
  using ReshardAlgorithm =
      std::function<void(ShardMap&, std::vector<MoveShardDescription>&)>;

 private:
  FutureVoid handlePostRebalanceShards(const ReshardAlgorithm&);
};
}  // namespace arangodb

#endif
