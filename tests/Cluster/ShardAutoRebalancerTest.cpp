#include <gtest/gtest.h>

#include "Cluster/AutoRebalance.h"
#include "Logger/LogMacros.h"

using namespace arangodb::cluster::rebalance;

TEST(AutoShardRebalancer, simple_randomized_test) {
  const auto nrDBServers = 3;
  const auto nrDBs = 5;
  const auto nrColls = 500;
  const auto minReplFactor = 1;
  const auto maxReplFactor = 3;
  const auto atMostJobs = 500;

  AutoRebalanceProblem p;
  p.createCluster(nrDBServers, true);
  p.createRandomDatabasesAndCollections(nrDBs, nrColls, minReplFactor,
                                        maxReplFactor);
  std::vector<double> probs;
  double q = 0.0;
  double pp = 4 / 7.0;
  for (uint32_t i = 0; i < nrDBServers; ++i) {
    q += pp;
    probs.push_back(q);
    pp /= 2.0;
  }
  p.distributeShardsRandomly(probs);
  std::vector<MoveShardJob> moves;
  int res = p.optimize(true, true, true, atMostJobs, moves);
  ASSERT_EQ(res, 0) << "Internal error, should not have happened: " << res
                    << " !";
  EXPECT_GT(moves.size(), 0);
  EXPECT_LE(moves.size(), atMostJobs);
}

TEST(AutoShardRebalancer, regression_leaderless_server_ignored) {
  const auto nrDBServers = 3;

  AutoRebalanceProblem p;
  p.createCluster(nrDBServers, true);
  p.createDatabase("d", 1);
  auto collId = p.createCollection("c", "d", 32, 4, 1.0);
  auto& coll = p.collections[collId];
  auto& shardIds = coll.shards;
  // Distribute leaders across servers 0 and 1, but not 2:
  uint32_t cur = 0;
  for (auto shardId : shardIds) {
    auto& shard = p.shards[shardId];
    shard.leader = cur;
    shard.followers.clear();
    for (uint32_t i = 0; i < 3; ++i) {
      if (i != cur) {
        shard.followers.push_back(i);
      }
    }
    cur = 1 - cur;
  }

  std::vector<MoveShardJob> moves;
  int res = p.optimize(true, false, false, 10, moves);
  ASSERT_EQ(res, 0) << "Internal error, should not have happened: " << res
                    << " !";
  ASSERT_NE(moves.size(), 0);
}
