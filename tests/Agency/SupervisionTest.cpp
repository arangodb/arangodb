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
/// @author Kaveh Vahedipour
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Agency/AgentInterface.h"
#include "Agency/Job.h"
#include "Agency/Supervision.h"
#include "Mocks/Servers.h"

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <typeinfo>

using namespace arangodb;
using namespace arangodb::consensus;

std::vector<std::string> servers{"XXX-XXX-XXX", "XXX-XXX-XXY"};

TEST(SupervisionTest, checking_for_the_delete_transaction_0_servers) {
  std::vector<std::string> todelete;
  auto const& transaction = removeTransactionBuilder(todelete);
  auto const& slice = transaction->slice();

  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 1);
  ASSERT_TRUE(slice[0].isArray());
  ASSERT_EQ(slice[0].length(), 1);
  ASSERT_TRUE(slice[0][0].isObject());
  ASSERT_EQ(slice[0][0].length(), 0);
}

TEST(SupervisionTest, checking_for_the_delete_transaction_1_server) {
  std::vector<std::string> todelete{servers[0]};
  auto const& transaction = removeTransactionBuilder(todelete);
  auto const& slice = transaction->slice();

  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 1);
  ASSERT_TRUE(slice[0].isArray());
  ASSERT_EQ(slice[0].length(), 1);
  ASSERT_TRUE(slice[0][0].isObject());
  ASSERT_EQ(slice[0][0].length(), 1);
  for (size_t i = 0; i < slice[0][0].length(); ++i) {
    ASSERT_TRUE(slice[0][0].keyAt(i).copyString() ==
                Supervision::agencyPrefix() +
                    arangodb::consensus::healthPrefix + servers[i]);
    ASSERT_TRUE(slice[0][0].valueAt(i).isObject());
    ASSERT_EQ(slice[0][0].valueAt(i).keyAt(0).copyString(), "op");
    ASSERT_EQ(slice[0][0].valueAt(i).valueAt(0).copyString(), "delete");
  }
}

TEST(SupervisionTest, checking_for_the_delete_transaction_2_servers) {
  std::vector<std::string> todelete = servers;
  auto const& transaction = removeTransactionBuilder(todelete);
  auto const& slice = transaction->slice();

  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 1);
  ASSERT_TRUE(slice[0].isArray());
  ASSERT_EQ(slice[0].length(), 1);
  ASSERT_TRUE(slice[0][0].isObject());
  ASSERT_EQ(slice[0][0].length(), 2);
  for (size_t i = 0; i < slice[0][0].length(); ++i) {
    ASSERT_TRUE(slice[0][0].keyAt(i).copyString() ==
                Supervision::agencyPrefix() +
                    arangodb::consensus::healthPrefix + servers[i]);
    ASSERT_TRUE(slice[0][0].valueAt(i).isObject());
    ASSERT_EQ(slice[0][0].valueAt(i).keyAt(0).copyString(), "op");
    ASSERT_EQ(slice[0][0].valueAt(i).valueAt(0).copyString(), "delete");
  }
}

static Node createNodeFromBuilder(Builder const& builder) {
  Builder opBuilder;
  {
    VPackObjectBuilder a(&opBuilder);
    opBuilder.add("new", builder.slice());
  }
  Node node("");
  node.handle<SET>(opBuilder.slice());
  return node;
}

static Builder createBuilder(char const* c) {
  Options options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(c);

  Builder builder;
  builder.add(parser.steal()->slice());
  return builder;
}

static Node createNode(char const* c) {
  return createNodeFromBuilder(createBuilder(c));
}

static char const* skeleton =
R"=(
{
  "Plan": {
    "Collections": {
      "database": {
        "123": {
          "replicationFactor": 2,
          "shards": {
            "s1": [
              "leader",
              "follower1"
            ]
          }
        },
        "124": {
          "replicationFactor": 2,
          "shards": {
            "s2": [
              "leader",
              "follower1"
            ]
          }
        },
        "125": {
          "replicationFactor": 2,
          "distributeShardsLike": "124",
          "shards": {
            "s3": [
              "leader",
              "follower1"
            ]
          }
        },
        "126": {
          "replicationFactor": 2,
          "distributeShardsLike": "124",
          "shards": {
            "s4": [
              "leader",
              "follower1"
            ]
          }
        }
      }
    },
    "DBServers": {
      "follower1": "none",
      "follower2": "none",
      "follower3": "none",
      "follower4": "none",
      "follower5": "none",
      "follower6": "none",
      "follower7": "none",
      "follower8": "none",
      "follower9": "none",
      "free": "none",
      "free2": "none",
      "leader": "none"
    }
  },
  "Current": {
    "Collections": {
      "database": {
        "123": {
          "s1": {
            "servers": [
              "leader",
              "follower1"
            ]
          }
        },
        "124": {
          "s2": {
            "servers": [
              "leader",
              "follower1"
            ]
          }
        },
        "125": {
          "s3": {
            "servers": [
              "leader",
              "follower1"
            ]
          }
        },
        "126": {
          "s4": {
            "servers": [
              "leader",
              "follower1"
            ]
          }
        }
      }
    }
  },
  "Supervision": {
    "DBServers": {},
    "Health": {
      "follower1": {
        "Status": "GOOD"
      },
      "follower2": {
        "Status": "GOOD"
      },
      "follower3": {
        "Status": "GOOD"
      },
      "follower4": {
        "Status": "GOOD"
      },
      "follower5": {
        "Status": "GOOD"
      },
      "follower6": {
        "Status": "GOOD"
      },
      "follower7": {
        "Status": "GOOD"
      },
      "follower8": {
        "Status": "GOOD"
      },
      "follower9": {
        "Status": "GOOD"
      },
      "leader": {
        "Status": "GOOD"
      },
      "free": {
        "Status": "GOOD"
      },
      "free2": {
        "Status": "FAILED"
      }
    },
    "Shards": {}
  },
  "Target": {
    "Failed": {},
    "Finished": {},
    "ToDo": {},
    "HotBackup": {
      "TransferJobs": {
      }
    }
  }
}
)=";

// Now we want to test the Supervision main function. We do this piece
// by piece for now. We instantiate a Supervision object, manipulate the
// agency snapshot it has and then check behaviour of member functions.

class SupervisionTestClass : public ::testing::Test {
 public:
  SupervisionTestClass()
    : _snapshot(createNode(skeleton)) {
  }
  ~SupervisionTestClass() {}
 protected:
  Node _snapshot;
};

static std::shared_ptr<VPackBuilder> runEnforceReplication(Node const& snap) {
  auto envelope = std::make_shared<VPackBuilder>();
  uint64_t jobId = 1;
  {
    VPackObjectBuilder guard(envelope.get());
    arangodb::consensus::enforceReplicationFunctional(
      snap, jobId, envelope);
  }
  return envelope;
}

static std::shared_ptr<VPackBuilder> runCleanupHotbackupTransferJobs(
    Node const& snap) {
  auto envelope = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder guard(envelope.get());
    arangodb::consensus::cleanupHotbackupTransferJobsFunctional(
      snap, envelope);
  }
  return envelope;
}

static void checkSupervisionJob(VPackSlice v, std::string const& type,
                                std::string const& database,
                                std::string const& coll,
                                std::string const& shard) {
  EXPECT_TRUE(v.isObject());
  EXPECT_EQ(v["creator"].copyString(), "supervision");
  EXPECT_EQ(v["type"].copyString(), type);
  EXPECT_EQ(v["database"].copyString(), database);
  EXPECT_EQ(v["collection"].copyString(), coll); 
  EXPECT_EQ(v["shard"].copyString(), shard);
}


TEST_F(SupervisionTestClass, enforce_replication_nothing_to_do) {
  std::shared_ptr<VPackBuilder> envelope = runEnforceReplication(_snapshot);
  VPackSlice content = envelope->slice();
  EXPECT_EQ(content.length(), 0);
}

TEST_F(SupervisionTestClass, schedule_removefollower) {
  _snapshot("/Plan/Collections/database/123/shards/s1")
    = createNode(R"=(["leader", "follower1", "follower2"])=");
  _snapshot("/Current/Collections/database/123/s1/servers")
    = createNode(R"=(["leader", "follower1", "follower2"])=");

  std::shared_ptr<VPackBuilder> envelope = runEnforceReplication(_snapshot);
  VPackSlice v = envelope->slice();

  EXPECT_EQ(v.length(), 1);
  v = v["/Target/ToDo/1"];
  checkSupervisionJob(v, "removeFollower", "database", "123", "s1");
  EXPECT_EQ(v["jobId"].copyString(), "1");
}

TEST_F(SupervisionTestClass, schedule_addfollower) {
  _snapshot("/Plan/Collections/database/123/shards/s1")
    = createNode(R"=(["leader"])=");
  _snapshot("/Current/Collections/database/123/s1/servers")
    = createNode(R"=(["leader"])=");

  std::shared_ptr<VPackBuilder> envelope = runEnforceReplication(_snapshot);
  VPackSlice v = envelope->slice();

  EXPECT_EQ(v.length(), 1);
  v = v["/Target/ToDo/1"];
  checkSupervisionJob(v, "addFollower", "database", "123", "s1");
}

TEST_F(SupervisionTestClass, schedule_addfollower_rf_3) {
  _snapshot("/Plan/Collections/database/123/replicationFactor")
    = createNode(R"=(3)=");

  std::shared_ptr<VPackBuilder> envelope = runEnforceReplication(_snapshot);
  VPackSlice v = envelope->slice();

  EXPECT_EQ(v.length(), 1);
  v = v["/Target/ToDo/1"];
  checkSupervisionJob(v, "addFollower", "database", "123", "s1");
}

static std::unordered_map<std::string, std::string> tableOfJobs(VPackSlice envelope) {
  std::unordered_map<std::string, std::string> res;
  for (auto const& p : VPackObjectIterator(envelope)) {
    res.emplace(std::pair(p.value.get("collection").copyString(),
                          p.key.copyString()));
  }
  return res;
}

TEST_F(SupervisionTestClass, schedule_addfollower_bad_server) {
  _snapshot("/Supervision/Health/follower1")
    = createNode(R"=("FAILED")=");

  std::shared_ptr<VPackBuilder> envelope = runEnforceReplication(_snapshot);
  VPackSlice todo = envelope->slice();

  EXPECT_EQ(todo.length(), 2);
  auto table = tableOfJobs(todo);
  checkSupervisionJob(todo[table["123"]], "addFollower", "database", "123", "s1");
  checkSupervisionJob(todo[table["124"]], "addFollower", "database", "124", "s2");
}

TEST_F(SupervisionTestClass, no_remove_follower_loop) {
  // This tests the case which used to have an unholy loop of scheduling
  // a removeFollower job and immediately terminating it and so on.
  // Now, no removeFollower job should be scheduled.
  _snapshot("/Plan/Collections/database/123/replicationFactor")
    = createNode(R"=(3)=");
  _snapshot("/Plan/Collections/database/123/shards/s1")
    = createNode(R"=(["leader", "follower1", "follower2", "follower3"])=");
  _snapshot("/Current/Collections/database/123/s1/servers")
    = createNode(R"=(["leader", "follower1", "follower2"])=");
  _snapshot("/Supervision/Health/follower1")
    = createNode(R"=("FAILED")=");
  std::shared_ptr<VPackBuilder> envelope = runEnforceReplication(_snapshot);
  VPackSlice content = envelope->slice();
  EXPECT_EQ(content.length(), 1);
  VPackSlice w = content["/Target/ToDo/1"];
  checkSupervisionJob(w, "addFollower", "database", "124", "s2");
}

TEST_F(SupervisionTestClass, no_remove_follower_loop_distributeshardslike) {
  // This tests another case which used to have an unholy loop of scheduling
  // a removeFollower job and immediately terminating it and so on.
  // Now, no removeFollower job should be scheduled.
  _snapshot("/Plan/Collections/database/124/replicationFactor")
    = createNode(R"=(3)=");
  _snapshot("/Plan/Collections/database/124/shards/s2")
    = createNode(R"=(["leader", "follower1", "follower2", "follower3"])=");
  _snapshot("/Plan/Collections/database/125/shards/s3")
    = createNode(R"=(["leader", "follower1", "follower2", "follower3"])=");
  _snapshot("/Plan/Collections/database/126/shards/s4")
    = createNode(R"=(["leader", "follower1", "follower2", "follower3"])=");
  _snapshot("/Current/Collections/database/124/s2/servers")
    = createNode(R"=(["leader", "follower1", "follower2", "follower3"])=");
  _snapshot("/Current/Collections/database/125/s3/servers")
    = createNode(R"=(["leader", "follower1", "follower3"])=");
  _snapshot("/Current/Collections/database/126/s4/servers")
    = createNode(R"=(["leader", "follower1", "follower2"])=");
  std::shared_ptr<VPackBuilder> envelope = runEnforceReplication(_snapshot);
  VPackSlice content = envelope->slice();
  EXPECT_EQ(content.length(), 0);
}

TEST_F(SupervisionTestClass, cleanup_hotback_transfer_jobs) {
  for (size_t i = 0; i < 200; ++i) {
    _snapshot("/Target/HotBackup/TransferJobs/" + std::to_string(1000000 + i))
      = createNode(R"=(
{
  "Timestamp": "2021-02-25T12:38:29Z",
  "DBServers": {
    "PRMR-b9b08faa-6286-4745-9c37-15e85b3a7d27": {
      "Progress": {
        "Total": 5,
        "Time": "2021-02-25T12:38:29Z",
        "Done": 5
      },
      "Status": "COMPLETED"
    },
    "PRMR-a0b13c71-2472-4985-bc48-ffa091d26e03": {
      "Progress": {
        "Total": 5,
        "Time": "2021-02-25T12:38:29Z",
        "Done": 5
      },
      "Status": "COMPLETED"
    }
  },
  "BackupId": "2021-02-25T12.38.11Z_c5656558-54ac-42bd-8851-08969d1a53f0"
}
        )=");
  }
  std::shared_ptr<VPackBuilder> envelope = runCleanupHotbackupTransferJobs(
      _snapshot);
  VPackSlice content = envelope->slice();
  EXPECT_EQ(content.length(), 100);
}

