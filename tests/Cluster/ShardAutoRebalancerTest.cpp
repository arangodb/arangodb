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
