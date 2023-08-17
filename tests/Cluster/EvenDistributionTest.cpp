////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include "Cluster/Utils/EvenDistribution.h"

namespace arangodb::tests {
class EvenDistributionTest : public ::testing::Test {
 protected:
  std::vector<ServerID> generateServerNames(uint64_t numberOfServers) {
    std::vector<ServerID> result;
    result.reserve(numberOfServers);
    for (uint64_t i = 0; i < numberOfServers; ++i) {
      result.emplace_back("PRMR_" + std::to_string(i));
    }
    return result;
  }

  void assertOnlyAllowedServersUsed(std::unordered_set<ServerID> const& planned,
                                    std::vector<ServerID> const& allowed) {
    for (auto const& server : planned) {
      auto it = std::find(allowed.begin(), allowed.end(), server);
      EXPECT_TRUE(it != allowed.end())
          << "Planned server " << server << " that is not allowed";
    }
  }

  void assertAllGetExactFollowers(EvenDistribution const& testee,
                                  uint64_t nrShards,
                                  uint64_t replicationFactor) {
    for (uint64_t s = 0; s < nrShards; ++s) {
      auto list = testee.getServersForShardIndex(s);
      EXPECT_EQ(list.servers.size(), replicationFactor)
          << "Incorrect number of followers for shardIndex: " << s;
    }
  }

  void assertMinAndMaxDifferByOneAtMost(
      std::unordered_map<ServerID, size_t> const& serverCounter,
      uint64_t nrServers) {
    size_t maxCounter = 0;
    size_t minCounter = std::numeric_limits<size_t>::max();
    for (auto const& [_, count] : serverCounter) {
      if (count > maxCounter) {
        maxCounter = count;
      }
      if (count < minCounter) {
        minCounter = count;
      }
    }
    if (serverCounter.size() < nrServers) {
      // If we have less shards than server,
      // do not pick any one twice
      EXPECT_EQ(maxCounter, 1);
    }
    ASSERT_GT(maxCounter, 0);
    EXPECT_LE(maxCounter - 1, minCounter)
        << "The minCounter is more than one point away from maxCounter";
    EXPECT_GE(minCounter + 1, maxCounter)
        << "The minCounter is more than one point away from maxCounter";
  }

  void assertEveryServerIsUsedEquallyOftenAsLeader(
      EvenDistribution const& testee, uint64_t nrServers, uint64_t nrShards) {
    std::unordered_map<ServerID, size_t> serverCounter;
    for (uint64_t s = 0; s < nrShards; ++s) {
      auto list = testee.getServersForShardIndex(s);
      auto l = list.getLeader();
      if (!serverCounter.contains(l)) {
        serverCounter.emplace(l, 0);
      }
      serverCounter[l]++;
    }
    assertMinAndMaxDifferByOneAtMost(serverCounter, nrServers);
  }

  void assertEveryServerIsUsedEquallyOften(EvenDistribution const& testee,
                                           uint64_t nrServers,
                                           uint64_t nrShards) {
    std::unordered_map<ServerID, size_t> serverCounter;
    for (uint64_t s = 0; s < nrShards; ++s) {
      auto list = testee.getServersForShardIndex(s);
      for (auto const& l : list.servers) {
        if (!serverCounter.contains(l)) {
          serverCounter.emplace(l, 0);
        }
        serverCounter[l]++;
      }
    }
    assertMinAndMaxDifferByOneAtMost(serverCounter, nrServers);
  }

  void assertNoServerIsUsedTwiceForTheSameShard(EvenDistribution const& testee,
                                                uint64_t nrShards) {}
};

TEST_F(EvenDistributionTest, should_create_one_entry_per_shard) {
  uint64_t nrShards = 9;

  auto names = generateServerNames(10);
  EvenDistribution testee{nrShards, 1, {}, true};

  std::unordered_set<ServerID> planned;
  auto res = testee.planShardsOnServers(names, planned);
  EXPECT_TRUE(res.ok());
  // We want an even distribution, we have more servers than required
  // so all servers should be used
  EXPECT_EQ(planned.size(), nrShards);
  // Every Planned server, must be available
  assertOnlyAllowedServersUsed(planned, names);
  assertAllGetExactFollowers(testee, nrShards, 1);
  assertEveryServerIsUsedEquallyOftenAsLeader(testee, names.size(), nrShards);
  assertEveryServerIsUsedEquallyOften(testee, names.size(), nrShards);
}

TEST_F(EvenDistributionTest, should_create_exact_number_of_replicas) {
  uint64_t nrShards = 9;
  uint64_t replicationFactor = 3;

  auto names = generateServerNames(10);
  EvenDistribution testee{nrShards, replicationFactor, {}, true};

  std::unordered_set<ServerID> planned;
  auto res = testee.planShardsOnServers(names, planned);
  EXPECT_TRUE(res.ok());
  // We want an even distribution, we have fewer servers than required
  // so all servers should be used
  EXPECT_EQ(planned.size(), 10);
  // Every Planned server, must be available
  assertOnlyAllowedServersUsed(planned, names);
  assertAllGetExactFollowers(testee, nrShards, replicationFactor);
  assertEveryServerIsUsedEquallyOftenAsLeader(testee, names.size(), nrShards);
  assertEveryServerIsUsedEquallyOften(testee, names.size(), nrShards);
}

TEST_F(EvenDistributionTest, should_not_use_avoid_servers) {
  uint64_t nrShards = 9;
  uint64_t replicationFactor = 3;

  auto names = generateServerNames(10);
  std::vector<ServerID> allowedNames{};
  std::vector<ServerID> forbiddenNames{};
  for (size_t i = 0; i < names.size(); ++i) {
    if (i % 2 == 0) {
      allowedNames.emplace_back(names.at(i));
    } else {
      forbiddenNames.emplace_back(names.at(i));
    }
  }

  EvenDistribution testee{nrShards, replicationFactor, forbiddenNames, true};

  std::unordered_set<ServerID> planned;
  auto res = testee.planShardsOnServers(names, planned);
  EXPECT_TRUE(res.ok());
  // We want an even distribution, we have more servers than required
  // so all servers should be used
  EXPECT_EQ(planned.size(), allowedNames.size());
  // Every Planned server, must be available
  assertOnlyAllowedServersUsed(planned, allowedNames);
  assertAllGetExactFollowers(testee, nrShards, replicationFactor);
  assertEveryServerIsUsedEquallyOftenAsLeader(testee, allowedNames.size(),
                                              nrShards);
  assertEveryServerIsUsedEquallyOften(testee, allowedNames.size(), nrShards);
}

TEST_F(EvenDistributionTest,
       should_fail_if_replication_is_larger_than_servers) {
  uint64_t nrShards = 9;
  uint64_t replicationFactor = 5;

  auto names = generateServerNames(3);

  ASSERT_LT(names.size(), replicationFactor)
      << "This test should test that replicationFactor is higher than allowed "
         "servers, this setup precondition is violated";
  EvenDistribution testee{nrShards, replicationFactor, {}, true};

  std::unordered_set<ServerID> planned;
  auto res = testee.planShardsOnServers(names, planned);
  EXPECT_FALSE(res.ok());
}

TEST_F(EvenDistributionTest,
       should_fail_if_replication_is_larger_than_servers_not_ignored) {
  uint64_t nrShards = 9;
  uint64_t replicationFactor = 6;

  auto names = generateServerNames(10);
  std::vector<ServerID> allowedNames{};
  std::vector<ServerID> forbiddenNames{};
  for (size_t i = 0; i < names.size(); ++i) {
    if (i % 2 == 0) {
      allowedNames.emplace_back(names.at(i));
    } else {
      forbiddenNames.emplace_back(names.at(i));
    }
  }
  ASSERT_LT(allowedNames.size(), replicationFactor)
      << "This test should test that replicationFactor is higher than allowed "
         "servers, this setup precondition is violated";

  // We have 10 server, we dissallow 5 of them => we cannot fulfill
  // replicationFactor 6.
  EvenDistribution testee{nrShards, replicationFactor, forbiddenNames, true};

  std::unordered_set<ServerID> planned;
  auto res = testee.planShardsOnServers(names, planned);
  EXPECT_FALSE(res.ok());
}

TEST_F(EvenDistributionTest,
       should_allow_if_replication_is_larger_than_servers_but_not_forced) {
  uint64_t nrShards = 9;
  uint64_t replicationFactor = 5;

  auto names = generateServerNames(3);
  EvenDistribution testee{nrShards, replicationFactor, {}, false};

  std::unordered_set<ServerID> planned;
  ASSERT_LT(names.size(), replicationFactor)
      << "This test should test that replicationFactor is higher than allowed "
         "servers, this setup precondition is violated";
  auto res = testee.planShardsOnServers(names, planned);
  EXPECT_TRUE(res.ok());
  // We want an even distribution, we have fewer servers than required
  // so all servers should be used
  EXPECT_EQ(planned.size(), names.size());
  // Every Planned server, must be available
  assertOnlyAllowedServersUsed(planned, names);
  // We doe not have enough servers, take what we can get.
  assertAllGetExactFollowers(testee, nrShards, names.size());
  assertEveryServerIsUsedEquallyOftenAsLeader(testee, names.size(), nrShards);
  assertEveryServerIsUsedEquallyOften(testee, names.size(), nrShards);
}

TEST_F(
    EvenDistributionTest,
    should_fail_if_replication_is_larger_than_servers_not_ignored_but_not_forced) {
  uint64_t nrShards = 9;
  uint64_t replicationFactor = 6;

  auto names = generateServerNames(10);
  std::vector<ServerID> allowedNames{};
  std::vector<ServerID> forbiddenNames{};
  for (size_t i = 0; i < names.size(); ++i) {
    if (i % 2 == 0) {
      allowedNames.emplace_back(names.at(i));
    } else {
      forbiddenNames.emplace_back(names.at(i));
    }
  }
  ASSERT_LT(allowedNames.size(), replicationFactor)
      << "This test should test that replicationFactor is higher than allowed "
         "servers, this setup precondition is violated";

  // We have 10 server, we dissallow 5 of them => we cannot fulfill
  // replicationFactor 6.
  EvenDistribution testee{nrShards, replicationFactor, forbiddenNames, false};

  std::unordered_set<ServerID> planned;
  auto res = testee.planShardsOnServers(names, planned);
  EXPECT_TRUE(res.ok());
  // We want an even distribution, we have more servers than required
  // so all servers should be used
  EXPECT_EQ(planned.size(), allowedNames.size());
  // Every Planned server, must be available
  assertOnlyAllowedServersUsed(planned, allowedNames);
  // We can only have allowedNames many followers, although replicationFactor is
  // higher
  assertAllGetExactFollowers(testee, nrShards, allowedNames.size());
  assertEveryServerIsUsedEquallyOftenAsLeader(testee, allowedNames.size(),
                                              nrShards);
  assertEveryServerIsUsedEquallyOften(testee, allowedNames.size(), nrShards);
}

}  // namespace arangodb::tests
