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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb::cluster::rebalance {
struct Zone {
  std::string id;
};

struct DBServer {
  std::string id;
  std::string shortName;
  uint64_t volumeSize;       // bytes
  uint64_t freeDiskSize;     // bytes
  double CPUcapacity = 1.0;  // only relative size counts to others
  uint32_t zone = 0;         // index in list
};

struct Shard {
  uint32_t id;  // index in shard list
  std::string name{};
  uint32_t leader;                    // index in dbServer list
  uint32_t replicationFactor;         // leader plus number of followers
  std::vector<uint32_t> followers{};  // index in DBServer list
  uint64_t size;                      // bytes
  uint64_t collectionId;
  double weight = 1.0;  // for leadership optimization
  bool blocked;         // flag, if shard is blocked by config
  bool ignored;         // flag, if shard is ignored by config
};

struct Collection {
  std::vector<uint32_t> shards{};
  std::string name;
  uint64_t id;
  uint64_t dbId;
  double weight = 1.0;  // for leadership optimization
  bool blocked;         // flag, if collection is blocked by config
  bool ignored;         // flag, if collection is ignored by config
};

struct Database {
  std::vector<uint32_t> collections{};
  std::string name;
  uint64_t id;
  double weight = 1.0;  // for leadership optimization
  bool blocked;         // flag, if database is blocked by configuration
  bool ignored;         // flag, if database is ignored by configuration
};

struct ShardImbalance {
  std::vector<double> sizeUsed;  // bytes, total size used on each DBServer
  std::vector<double> targetSize;
  std::vector<uint64_t> numberShards;  // number of shards per DBServer
  double totalUsed;                    // sum of all sizes
  uint64_t totalShards;  // total number of all shards (leader or follower)
  double imbalance;      // total imbalance according to formula
                         // in design document
  ShardImbalance(size_t numberDBServers)
      : totalUsed(0.0), totalShards(0), imbalance(0) {
    sizeUsed.resize(numberDBServers, 0);
    targetSize.resize(numberDBServers, 0);
    numberShards.resize(numberDBServers, 0);
  }
};

template<class Inspector>
auto inspect(Inspector& f, ShardImbalance& x) {
  return f.object(x).fields(
      f.field("sizeUsed", x.sizeUsed), f.field("targetSize", x.targetSize),
      f.field("numberShards", x.numberShards),
      f.field("totalUsed", x.totalUsed), f.field("totalShards", x.totalShards),
      f.field("imbalance", x.imbalance));
}

struct LeaderImbalance {
  std::vector<double> weightUsed;  // number of shards * weight
                                   // for shard leaderships for each dbserver
  std::vector<double> targetWeight;
  std::vector<uint64_t> numberShards;  // number of shards
  std::vector<double> leaderDupl;      // leader duplication number for each
                                       // DBServer (pi in design)
  double totalWeight;                  // sum of all weights
  uint64_t totalShards;                // total number of leader shards
  double imbalance;                    // total imbalance according to formula
                                       // in design document
  LeaderImbalance(size_t numberDBServers)
      : totalWeight(0.0), totalShards(0), imbalance(0) {
    weightUsed.resize(numberDBServers, 0);
    targetWeight.resize(numberDBServers, 0);
    numberShards.resize(numberDBServers, 0);
    leaderDupl.resize(numberDBServers, 0);
  }
};

template<class Inspector>
auto inspect(Inspector& f, LeaderImbalance& x) {
  return f.object(x).fields(f.field("weightUsed", x.weightUsed),
                            f.field("targetWeight", x.targetWeight),
                            f.field("numberShards", x.numberShards),
                            f.field("leaderDupl", x.leaderDupl),
                            f.field("totalWeight", x.totalWeight),
                            f.field("imbalance", x.imbalance),
                            f.field("totalShards", x.totalShards));
}

struct MoveShardJob {
  uint32_t shardId;  // index in shard list
  uint32_t from;     // index in dbServers
  uint32_t to;       // index in dbServers
  bool isLeader;     // true if this is a leader change to an in sync follower,
                     // false if this is a follower move to another server
  bool movesData;    // true if data actually needs to be moved
  double score;
  ShardImbalance shardImbAfter;
  LeaderImbalance leaderImbAfter;
  MoveShardJob(uint32_t shardId, uint32_t from, uint32_t to, bool isLeader,
               bool movesData, uint32_t nrDBServers)
      : shardId(shardId),
        from(from),
        to(to),
        isLeader(isLeader),
        movesData(movesData),
        score(0.0),
        shardImbAfter(nrDBServers),
        leaderImbAfter(nrDBServers) {}
};

struct AutoRebalanceProblem {
  std::vector<DBServer> dbServers;
  std::vector<Zone> zones;
  std::vector<Shard> shards;
  std::vector<Collection> collections;
  std::vector<Database> databases;
  std::unordered_map<std::string, uint64_t> dbCollByName;
  std::unordered_map<std::string, uint64_t> dbByName;

 private:
  double _piFactor = 256e6;
  // Factor to balance the effect of uneven distribution
  // of leader shards **within** a collection against
  // global uneven distribution of leader shards
 public:
  uint64_t createDatabase(std::string const& name, double weight = 1.0);
  uint64_t createCollection(std::string const& name, std::string const& dbName,
                            uint32_t numberOfShards, uint32_t replicationFactor,
                            double weight = 1.0);
  void createCluster(uint32_t nrDBserver, bool withZones = false);
  void createRandomDatabasesAndCollections(uint32_t nrDBs, uint32_t nrColls,
                                           uint32_t minReplFactor,
                                           uint32_t maxReplFactor);
  void setPiFactor(double p) { _piFactor = p; }
  void distributeShardsRandomly(std::vector<double> const& probabilities);
  void moveToBuilder(MoveShardJob const& m, VPackBuilder& mb) const;
  // moves will be overwritten!
  int optimize(bool considerLeaderChanges, bool considerFollowerMoves,
               bool considerLeaderMoves, size_t atMost,
               std::vector<MoveShardJob>& moves);

  ShardImbalance computeShardImbalance() const;
  std::vector<double> piCoefficients(Collection const& c) const;
  LeaderImbalance computeLeaderImbalance() const;
  int applyMoveShardJob(MoveShardJob const& job, bool dryRun,
                        ShardImbalance* shardImb, LeaderImbalance* leaderImb);
  // Will find groups of a few thousand, where jobs for two shards of the same
  // collection are always in the same group.
  std::vector<std::vector<MoveShardJob>> findAllMoveShardJobs(
      bool considerLeaderChanges, bool considerFollowerMoves,
      bool considerLeaderMoves) const;
};
}  // namespace arangodb::cluster::rebalance
