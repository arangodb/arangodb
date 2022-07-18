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
  EXPECT_LE(moves.size(), atMostJobs);
}
