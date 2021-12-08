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
  _snapshot.getOrCreate("/Plan/Collections/database/123/shards/s1")
    = createNode(R"=(["leader", "follower1", "follower2"])=");
  _snapshot.getOrCreate("/Current/Collections/database/123/s1/servers")
    = createNode(R"=(["leader", "follower1", "follower2"])=");

  std::shared_ptr<VPackBuilder> envelope = runEnforceReplication(_snapshot);
  VPackSlice v = envelope->slice();

  EXPECT_EQ(v.length(), 1);
  v = v["/Target/ToDo/1"];
  checkSupervisionJob(v, "removeFollower", "database", "123", "s1");
  EXPECT_EQ(v["jobId"].copyString(), "1");
}

TEST_F(SupervisionTestClass, schedule_addfollower) {
  _snapshot.getOrCreate("/Plan/Collections/database/123/shards/s1")
    = createNode(R"=(["leader"])=");
  _snapshot.getOrCreate("/Current/Collections/database/123/s1/servers")
    = createNode(R"=(["leader"])=");

  std::shared_ptr<VPackBuilder> envelope = runEnforceReplication(_snapshot);
  VPackSlice v = envelope->slice();

  EXPECT_EQ(v.length(), 1);
  v = v["/Target/ToDo/1"];
  checkSupervisionJob(v, "addFollower", "database", "123", "s1");
}

TEST_F(SupervisionTestClass, schedule_addfollower_rf_3) {
  _snapshot.getOrCreate("/Plan/Collections/database/123/replicationFactor")
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
  _snapshot.getOrCreate("/Supervision/Health/follower1")
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
  _snapshot.getOrCreate("/Plan/Collections/database/123/replicationFactor")
    = createNode(R"=(3)=");
  _snapshot.getOrCreate("/Plan/Collections/database/123/shards/s1")
    = createNode(R"=(["leader", "follower1", "follower2", "follower3"])=");
  _snapshot.getOrCreate("/Current/Collections/database/123/s1/servers")
    = createNode(R"=(["leader", "follower1", "follower2"])=");
  _snapshot.getOrCreate("/Supervision/Health/follower1")
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
  _snapshot.getOrCreate("/Plan/Collections/database/124/replicationFactor")
    = createNode(R"=(3)=");
  _snapshot.getOrCreate("/Plan/Collections/database/124/shards/s2")
    = createNode(R"=(["leader", "follower1", "follower2", "follower3"])=");
  _snapshot.getOrCreate("/Plan/Collections/database/125/shards/s3")
    = createNode(R"=(["leader", "follower1", "follower2", "follower3"])=");
  _snapshot.getOrCreate("/Plan/Collections/database/126/shards/s4")
    = createNode(R"=(["leader", "follower1", "follower2", "follower3"])=");
  _snapshot.getOrCreate("/Current/Collections/database/124/s2/servers")
    = createNode(R"=(["leader", "follower1", "follower2", "follower3"])=");
  _snapshot.getOrCreate("/Current/Collections/database/125/s3/servers")
    = createNode(R"=(["leader", "follower1", "follower3"])=");
  _snapshot.getOrCreate("/Current/Collections/database/126/s4/servers")
    = createNode(R"=(["leader", "follower1", "follower2"])=");
  std::shared_ptr<VPackBuilder> envelope = runEnforceReplication(_snapshot);
  VPackSlice content = envelope->slice();
  EXPECT_EQ(content.length(), 0);
}

static void makeHotbackupTransferJob(Node& snapshot, size_t year, size_t id,
                                     char const* job) {
  std::string st = std::string(job)
      + "\"Timestamp\": \"" + std::to_string(year)
      + "-02-25T12:38:29Z\"\n}";
  snapshot.getOrCreate("/Target/HotBackup/TransferJobs/" + std::to_string(id))
    = createNode(st.c_str());
}

TEST_F(SupervisionTestClass, cleanup_hotback_transfer_jobs) {
  for (size_t i = 0; i < 200; ++i) {
    makeHotbackupTransferJob(_snapshot, 1900 + i, 1000000 + i,
R"=(
{
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
  "BackupId": "2021-02-25T12.38.11Z_c5656558-54ac-42bd-8851-08969d1a53f0",
)=");
  };

  auto envelope = std::make_shared<VPackBuilder>();
  arangodb::consensus::cleanupHotbackupTransferJobsFunctional(
    _snapshot, envelope);
  VPackSlice content = envelope->slice();
  EXPECT_TRUE(content.isArray());
  EXPECT_EQ(content.length(), 1);
  content = content[0];
  EXPECT_EQ(content.length(), 100);
  // We expect the first 100 jobs to be deleted:
  for (size_t i = 0; i < 100; ++i) {
    std::string jobId = "/Target/HotBackup/TransferJobs/" + std::to_string(1000000 + i);
    VPackSlice guck = content.get(jobId);
    EXPECT_TRUE(guck.isObject());
    EXPECT_TRUE(guck.hasKey("op"));
    EXPECT_EQ(guck.get("op").copyString(), "delete");
  }
}

TEST_F(SupervisionTestClass, cleanup_hotback_transfer_jobs_empty) {
  for (size_t i = 0; i < 200; ++i) {
    makeHotbackupTransferJob(_snapshot, 1900 + i, 1000000 + i,
R"=(
{
  "DBServers": {
    "PRMR-b9b08faa-6286-4745-9c37-15e85b3a7d27": {
    },
    "PRMR-a0b13c71-2472-4985-bc48-ffa091d26e03": {
    }
  },
  "BackupId": "2021-02-25T12.38.11Z_c5656558-54ac-42bd-8851-08969d1a53f0",
)=");
  }

  auto envelope = std::make_shared<VPackBuilder>();
  arangodb::consensus::cleanupHotbackupTransferJobsFunctional(
    _snapshot, envelope);
  VPackSlice content = envelope->slice();
  EXPECT_TRUE(content.isArray());
  EXPECT_EQ(content.length(), 1);
  content = content[0];
  EXPECT_EQ(content.length(), 100);
  // We expect the first 100 jobs to be deleted:
  for (size_t i = 0; i < 100; ++i) {
    std::string jobId = "/Target/HotBackup/TransferJobs/" + std::to_string(1000000 + i);
    VPackSlice guck = content.get(jobId);
    EXPECT_TRUE(guck.isObject());
    EXPECT_TRUE(guck.hasKey("op"));
    EXPECT_EQ(guck.get("op").copyString(), "delete");
  }
}

TEST_F(SupervisionTestClass, cleanup_hotback_transfer_jobs_diverse) {
  // First create the jobs which shall remain:
  for (size_t i = 0; i < 100; ++i) {
    makeHotbackupTransferJob(_snapshot, 2000 + i, 2000000 + i,
R"=(
{
  "DBServers": {
    "PRMR-b9b08faa-6286-4745-9c37-15e85b3a7d27": {
    },
    "PRMR-a0b13c71-2472-4985-bc48-ffa091d26e03": {
    }
  },
  "BackupId": "2021-02-25T12.38.11Z_c5656558-54ac-42bd-8851-08969d1a53f0",
)=");
  }
  // Now create a selection of jobs which ought to be removed, since they
  // are old:

  // An old job which is ongoing:
  makeHotbackupTransferJob(_snapshot, 1900, 1000000,
R"=(
{
  "DBServers": {
    "PRMR-b9b08faa-6286-4745-9c37-15e85b3a7d27": {
      "Progress": {
        "Total": 5,
        "Time": "2021-02-25T12:38:29Z",
        "Done": 3
      },
      "Status": "STARTED"
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
  "BackupId": "2021-02-25T12.38.11Z_c5656558-54ac-42bd-8851-08969d1a53f0",
)=");

  // An old job which is has fewer DBServers:
  makeHotbackupTransferJob(_snapshot, 1901, 1000001,
R"=(
{
  "DBServers": {
    "PRMR-b9b08faa-6286-4745-9c37-15e85b3a7d27": {
      "Progress": {
        "Total": 5,
        "Time": "2021-02-25T12:38:29Z",
        "Done": 3
      },
      "Status": "STARTED"
    },
    "PRMR-a0b13c71-2472-4985-bc48-ffa091d26e03": {
    }
  },
  "BackupId": "2021-02-25T12.38.11Z_c5656558-54ac-42bd-8851-08969d1a53f0",
)=");

  // An old job which has never started:
  makeHotbackupTransferJob(_snapshot, 1902, 1000002,
R"=(
{
  "DBServers": {
    "PRMR-b9b08faa-6286-4745-9c37-15e85b3a7d27": {
    },
    "PRMR-a0b13c71-2472-4985-bc48-ffa091d26e03": {
    }
  },
  "BackupId": "2021-02-25T12.38.11Z_c5656558-54ac-42bd-8851-08969d1a53f0",
)=");

  // An old job which is partially failed:
  makeHotbackupTransferJob(_snapshot, 1903, 1000003,
R"=(
{
  "DBServers": {
    "PRMR-b9b08faa-6286-4745-9c37-15e85b3a7d27": {
      "Progress": {
        "Total": 5,
        "Time": "2021-02-25T12:38:29Z",
        "Done": 3
      },
      "Status": "FAILED"
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
  "BackupId": "2021-02-25T12.38.11Z_c5656558-54ac-42bd-8851-08969d1a53f0",
)=");

  // An old job which is partially failed:
  makeHotbackupTransferJob(_snapshot, 1904, 1000004,
R"=(
{
  "DBServers": {
    "PRMR-b9b08faa-6286-4745-9c37-15e85b3a7d27": {
      "Progress": {
        "Total": 5,
        "Time": "2021-02-25T12:38:29Z",
        "Done": 3
      },
      "Status": "FAILED"
    },
    "PRMR-a0b13c71-2472-4985-bc48-ffa091d26e03": {
    }
  },
  "BackupId": "2021-02-25T12.38.11Z_c5656558-54ac-42bd-8851-08969d1a53f0",
)=");

  // Now a new style job which is ongoing, must not be deleted:
  makeHotbackupTransferJob(_snapshot, 1905, 1000005,
R"=(
{
  "DBServers": {
    "PRMR-b9b08faa-6286-4745-9c37-15e85b3a7d27": {
      "Progress": {
        "Total": 5,
        "Time": "2021-02-25T12:38:29Z",
        "Done": 3
      },
      "rebootId": 1,
      "Status": "STARTED"
    },
    "PRMR-a0b13c71-2472-4985-bc48-ffa091d26e03": {
      "Progress": {
        "Total": 5,
        "Time": "2021-02-25T12:38:29Z",
        "Done": 5
      },
      "rebootId": 1,
      "Status": "STARTED"
    }
  },
  "BackupId": "2021-02-25T12.38.11Z_c5656558-54ac-42bd-8851-08969d1a53f0",
)=");

  // Now a new style job which has not even started:
  makeHotbackupTransferJob(_snapshot, 1906, 1000006,
R"=(
{
  "DBServers": {
    "PRMR-b9b08faa-6286-4745-9c37-15e85b3a7d27": {
      "Status": "NEW"
    },
    "PRMR-a0b13c71-2472-4985-bc48-ffa091d26e03": {
      "Status": "NEW"
    }
  },
  "BackupId": "2021-02-25T12.38.11Z_c5656558-54ac-42bd-8851-08969d1a53f0",
)=");

  auto envelope = std::make_shared<VPackBuilder>();
  arangodb::consensus::cleanupHotbackupTransferJobsFunctional(
    _snapshot, envelope);
  VPackSlice content = envelope->slice();
  EXPECT_TRUE(content.isArray());
  EXPECT_EQ(content.length(), 1);
  content = content[0];
  EXPECT_EQ(content.length(), 5);
  // We expect the oldest suitable jobs to be deleted:
  for (size_t i = 0; i < 5; ++i) {
    std::string jobId = "/Target/HotBackup/TransferJobs/" + std::to_string(1000000 + i);
    VPackSlice guck = content.get(jobId);
    EXPECT_TRUE(guck.isObject());
    EXPECT_TRUE(guck.hasKey("op"));
    EXPECT_EQ(guck.get("op").copyString(), "delete");
  }
}

TEST_F(SupervisionTestClass, cleanup_hotback_transfer_locks) {
  // Add 5 old completed transfer jobs:
  for (size_t i = 0; i < 5; ++i) {
    makeHotbackupTransferJob(_snapshot, 1900 + i, 1000000 + i,
R"=(
{
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
  "BackupId": "2021-02-25T12.38.11Z_c5656558-54ac-42bd-8851-08969d1a53f0",
)=");
  };
  // Add 200 old transfer locks:
  for (size_t i = 0; i < 200; ++i) {
    _snapshot.getOrCreate("/Target/HotBackup/Transfers/Upload/xyz" + std::to_string(i) + "abc")
        = createNode(R"=(
{
  "some": 1,
  "arbitrary": 2,
  "data": 3
}
          )=");
  }

  auto envelope = std::make_shared<VPackBuilder>();
  arangodb::consensus::cleanupHotbackupTransferJobsFunctional(
    _snapshot, envelope);
  VPackSlice content = envelope->slice();
  EXPECT_TRUE(content.isArray());
  EXPECT_EQ(content.length(), 2);
  VPackSlice action = content[0];
  EXPECT_TRUE(action.isObject());
  EXPECT_EQ(action.length(), 1);
  // We expect all transfer locks to be deleted:
  VPackSlice guck = action.get("/Target/HotBackup/Transfers/");
  EXPECT_TRUE(guck.isObject());
  EXPECT_TRUE(guck.hasKey("op"));
  EXPECT_EQ(guck.get("op").copyString(), "set");
  EXPECT_TRUE(guck.hasKey("new"));
  VPackSlice guck2 = guck.get("new");
  EXPECT_TRUE(guck2.isObject());
  EXPECT_EQ(guck2.length(), 0);
  // The second item is an enormous precondition:
  VPackSlice precond = content[1];
  EXPECT_TRUE(precond.isObject());
  EXPECT_EQ(precond.length(), 1);
  guck = precond.get("/Target/HotBackup/TransferJobs/");
  EXPECT_TRUE(guck.isObject());
}

TEST_F(SupervisionTestClass, cleanup_hotback_transfer_locks_dont) {
  // Add 5 new running transfer jobs:
  for (size_t i = 0; i < 5; ++i) {
    makeHotbackupTransferJob(_snapshot, 1900 + i, 1000000 + i,
R"=(
{
  "DBServers": {
    "PRMR-b9b08faa-6286-4745-9c37-15e85b3a7d27": {
      "Progress": {
        "Total": 5,
        "Time": "2021-02-25T12:38:29Z",
        "Done": 4
      },
      "rebootId": 1,
      "Status": "RUNNING"
    },
    "PRMR-a0b13c71-2472-4985-bc48-ffa091d26e03": {
      "Progress": {
        "Total": 5,
        "Time": "2021-02-25T12:38:29Z",
        "Done": 5
      },
      "rebootId": 1,
      "Status": "COMPLETED"
    }
  },
  "BackupId": "2021-02-25T12.38.11Z_c5656558-54ac-42bd-8851-08969d1a53f0",
)=");
  };
  // Add 200 old transfer locks:
  for (size_t i = 0; i < 200; ++i) {
    _snapshot.getOrCreate("/Target/HotBackup/Transfers/Upload/xyz" + std::to_string(i) + "abc")
        = createNode(R"=(
{
  "some": 1,
  "arbitrary": 2,
  "data": 3
}
          )=");
  }

  auto envelope = std::make_shared<VPackBuilder>();
  arangodb::consensus::cleanupHotbackupTransferJobsFunctional(
    _snapshot, envelope);
  EXPECT_TRUE(envelope->isEmpty());
}

TEST_F(SupervisionTestClass, fail_hotbackup_transfer_jobs) {
  // We put in three transfer jobs. One of the DBServers is healthy
  // and its rebootId has not changed ==> nothing ought to be done.
  // For the other ones either the dbserver is FAILED or its rebootId
  // has changed, in this case we want the job aborted and the lock
  // removed.
  _snapshot.getOrCreate("/Target/HotBackup/TransferJobs/" + std::to_string(1234567))
      = createNode(R"=(
{
  "Timestamp": "2021-02-25T12:38:29Z",
  "DBServers": {
    "PRMR-b9b08faa-6286-4745-9c37-15e85b3a7d27": {
      "Progress": {
        "Total": 5,
        "Time": "2021-02-25T12:38:29Z",
        "Done": 4
      },
      "rebootId": 1,
      "lockLocation": "Upload/local:/tmp/backups/2021-11-26T09.21.00Z_c95725ed-7572-4dac-bc8d-ea786d05f833/PRMR-b9b08faa-6286-4745-9c37-15e85b3a7d27",
      "Status": "RUNNING"
    },
    "PRMR-fe142532-2536-426f-23aa-123534feb253": {
      "Progress": {
        "Total": 5,
        "Time": "2021-02-25T12:38:29Z",
        "Done": 2
      },
      "rebootId": 1,
      "lockLocation": "Upload/local:/tmp/backups/2021-11-26T09.21.00Z_c95725ed-7572-4dac-bc8d-ea786d05f833/PRMR-fe142532-2536-426f-23aa-123534feb253",
      "Status": "RUNNING"
    },
    "PRMR-a0b13c71-2472-4985-bc48-ffa091d26e03": {
      "Progress": {
        "Total": 5,
        "Time": "2021-02-25T12:38:29Z",
        "Done": 3
      },
      "rebootId": 1,
      "lockLocation": "Upload/local:/tmp/backups/2021-11-26T09.21.00Z_c95725ed-7572-4dac-bc8d-ea786d05f833/PRMR-a0b13c71-2472-4985-bc48-ffa091d26e03",
      "Status": "RUNNING"
    }
  },
  "BackupId": "2021-02-25T12.38.11Z_c5656558-54ac-42bd-8851-08969d1a53f0"
}
        )=");
  _snapshot.getOrCreate("/Current/ServersKnown")
      = createNode(R"=(
{
  "PRMR-b9b08faa-6286-4745-9c37-15e85b3a7d27": {
    "rebootId": 1
  },
  "PRMR-fe142532-2536-426f-23aa-123534feb253": {
    "rebootId": 1
  },
  "PRMR-a0b13c71-2472-4985-bc48-ffa091d26e03": {
    "rebootId": 2
  }
}
        )=");
  _snapshot.getOrCreate("/Supervision/Health")
      = createNode(R"=(
{
  "PRMR-b9b08faa-6286-4745-9c37-15e85b3a7d27": {
    "ShortName": "DBServer0001",
    "Endpoint": "tcp://[::1]:8629",
    "Host": "1a00546e9ae740aeadf30f0090f43b8d",
    "SyncStatus": "SERVING",
    "Status": "GOOD",
    "Version": "3.8.4",
    "Engine": "rocksdb",
    "Timestamp": "2021-11-26T11:05:22Z",
    "SyncTime": "2021-11-26T11:05:22Z",
    "LastAckedTime": "2021-11-26T11:05:22Z"
  },
  "PRMR-fe142532-2536-426f-23aa-123534feb253": {
    "ShortName": "DBServer0002",
    "Endpoint": "tcp://[::1]:8630",
    "Host": "1a00546e9ae740aeadf30f0090f43b8d",
    "SyncStatus": "SERVING",
    "Status": "FAILED",
    "Version": "3.8.4",
    "Engine": "rocksdb",
    "Timestamp": "2021-11-26T11:05:22Z",
    "SyncTime": "2021-11-26T11:05:22Z",
    "LastAckedTime": "2021-11-26T11:05:22Z"
  },
  "PRMR-a0b13c71-2472-4985-bc48-ffa091d26e03": {
    "ShortName": "DBServer0003",
    "Endpoint": "tcp://[::1]:8631",
    "Host": "1a00546e9ae740aeadf30f0090f43b8d",
    "SyncStatus": "SERVING",
    "Status": "GOOD",
    "Version": "3.8.4",
    "Engine": "rocksdb",
    "Timestamp": "2021-11-26T11:05:22Z",
    "SyncTime": "2021-11-26T11:05:22Z",
    "LastAckedTime": "2021-11-26T11:05:22Z"
  }
}
        )=");
  auto envelope = std::make_shared<VPackBuilder>();
  arangodb::consensus::failBrokenHotbackupTransferJobsFunctional(
    _snapshot, envelope);
  VPackSlice content = envelope->slice();
  EXPECT_TRUE(content.isArray());
  EXPECT_EQ(content.length(), 2);  // two transactions

  // We are expecting two transactions, but do not really know the order
  // in which they arrive (the Supervision iterates over a std::unordered_map,
  // which is - as the name suggests - unordered:
  size_t i = 0;
  if (!content[0][0].hasKey("/Target/HotBackup/TransferJobs/1234567/DBServers/PRMR-fe142532-2536-426f-23aa-123534feb253/Status")) {
    i = 1;
  }

  VPackSlice trx = content[i];
  EXPECT_TRUE(trx.isArray());
  EXPECT_EQ(trx.length(), 2);  // with precondition

  VPackSlice action = trx[0];
  VPackSlice guck = action.get("/Target/HotBackup/TransferJobs/1234567/DBServers/PRMR-fe142532-2536-426f-23aa-123534feb253/Status");
  EXPECT_TRUE(guck.isObject());
  EXPECT_EQ(guck.length(), 2);
  EXPECT_EQ(guck["op"].copyString(), "set");
  EXPECT_EQ(guck["new"].copyString(), "FAILED");
  guck = action.get("/Target/HotBackup/Transfers/Upload/local:/tmp/backups/2021-11-26T09.21.00Z_c95725ed-7572-4dac-bc8d-ea786d05f833/PRMR-fe142532-2536-426f-23aa-123534feb253");
  EXPECT_TRUE(guck.isObject());
  EXPECT_EQ(guck["op"].copyString(), "delete");
  VPackSlice precond = trx[1];
  guck = precond.get("/Target/HotBackup/TransferJobs/1234567/DBServers/PRMR-fe142532-2536-426f-23aa-123534feb253/Status");
  EXPECT_EQ(guck.copyString(), "RUNNING");

  trx = content[1-i];
  EXPECT_TRUE(trx.isArray());
  EXPECT_EQ(trx.length(), 2);  // with precondition

  action = trx[0];
  guck = action.get("/Target/HotBackup/TransferJobs/1234567/DBServers/PRMR-a0b13c71-2472-4985-bc48-ffa091d26e03/Status");
  EXPECT_TRUE(guck.isObject());
  EXPECT_EQ(guck["op"].copyString(), "set");
  EXPECT_EQ(guck["new"].copyString(), "FAILED");
  guck = action.get("/Target/HotBackup/Transfers/Upload/local:/tmp/backups/2021-11-26T09.21.00Z_c95725ed-7572-4dac-bc8d-ea786d05f833/PRMR-a0b13c71-2472-4985-bc48-ffa091d26e03");
  EXPECT_TRUE(guck.isObject());
  EXPECT_EQ(guck["op"].copyString(), "delete");
  precond = trx[1];
  guck = precond.get("/Target/HotBackup/TransferJobs/1234567/DBServers/PRMR-a0b13c71-2472-4985-bc48-ffa091d26e03/Status");
  EXPECT_EQ(guck.copyString(), "RUNNING");
}

