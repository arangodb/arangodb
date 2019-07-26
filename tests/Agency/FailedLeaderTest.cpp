////////////////////////////////////////////////////////////////////////////////
/// @brief test case for FailedLeader job
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "Agency/AgentInterface.h"
#include "Agency/FailedLeader.h"
#include "Agency/MoveShard.h"
#include "Agency/Node.h"
#include "lib/Basics/StringUtils.h"
#include "lib/Random/RandomGenerator.h"

#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <iostream>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::consensus;
using namespace fakeit;

namespace arangodb {
namespace tests {
namespace failed_leader_test {

const std::string PREFIX = "arango";
const std::string DATABASE = "database";
const std::string COLLECTION = "collection";
const std::string SHARD = "s99";
const std::string SHARD_LEADER = "leader";
const std::string SHARD_FOLLOWER1 = "follower1";
const std::string SHARD_FOLLOWER2 = "follower2";
const std::string FREE_SERVER = "free";
const std::string FREE_SERVER2 = "free2";

bool aborts = false;

const char* agency =
#include "FailedLeaderTest.json"
    ;

Node createNodeFromBuilder(Builder const& builder) {
  Builder opBuilder;
  {
    VPackObjectBuilder a(&opBuilder);
    opBuilder.add("new", builder.slice());
  }

  Node node("");
  node.handle<SET>(opBuilder.slice());
  return node;
}

Builder createBuilder(char const* c) {
  VPackOptions options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(c);

  Builder builder;
  builder.add(parser.steal()->slice());
  return builder;
}

Node createNode(char const* c) {
  return createNodeFromBuilder(createBuilder(c));
}

Node createRootNode() { return createNode(agency); }

char const* todo = R"=({
  "creator":"1", "type":"failedLeader", "database":"database",
  "collection":"collection", "shard":"s99", "fromServer":"leader",
  "jobId":"1", "timeCreated":"2017-01-01 00:00:00"
  })=";

typedef std::function<std::unique_ptr<Builder>(Slice const&, std::string const&)> TestStructureType;

class FailedLeaderTest : public ::testing::Test {
 protected:
  Node baseStructure;
  Builder builder;
  write_ret_t fakeWriteResult;
  std::shared_ptr<Builder> transBuilder;
  trans_ret_t fakeTransResult;

  FailedLeaderTest()
      : baseStructure(createRootNode()),
        fakeWriteResult(true, "", std::vector<apply_ret_t>{APPLIED},
                        std::vector<index_t>{1}),
        transBuilder(std::make_shared<Builder>()),
        fakeTransResult(true, "", 1, 0, transBuilder) {
    RandomGenerator::seed(3);
    baseStructure.toBuilder(builder);
    VPackArrayBuilder a(transBuilder.get());
    transBuilder->add(VPackValue((uint64_t)1));
  }
};

TEST_F(FailedLeaderTest, creating_a_job_should_create_a_job_in_todo) {
  Mock<AgentInterface> mockAgent;

  std::string jobId = "1";
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    auto expectedJobKey = "/arango/Target/ToDo/" + jobId;
    EXPECT_TRUE(std::string(q->slice().typeName()) == "array");
    EXPECT_TRUE(q->slice().length() == 1);
    EXPECT_TRUE(std::string(q->slice()[0].typeName()) == "array");
    EXPECT_TRUE(q->slice()[0].length() == 1);  // we always simply override! no preconditions...
    EXPECT_TRUE(std::string(q->slice()[0][0].typeName()) == "object");
    EXPECT_TRUE(q->slice()[0][0].length() == 1);  // should ONLY do an entry in todo
    EXPECT_TRUE(std::string(q->slice()[0][0].get(expectedJobKey).typeName()) ==
                "object");

    auto job = q->slice()[0][0].get(expectedJobKey);
    EXPECT_TRUE(std::string(job.get("creator").typeName()) == "string");
    EXPECT_TRUE(std::string(job.get("type").typeName()) == "string");
    EXPECT_TRUE(job.get("type").copyString() == "failedLeader");
    EXPECT_TRUE(std::string(job.get("database").typeName()) == "string");
    EXPECT_TRUE(job.get("database").copyString() == DATABASE);
    EXPECT_TRUE(std::string(job.get("collection").typeName()) == "string");
    EXPECT_TRUE(job.get("collection").copyString() == COLLECTION);
    EXPECT_TRUE(std::string(job.get("shard").typeName()) == "string");
    EXPECT_TRUE(job.get("shard").copyString() == SHARD);
    EXPECT_TRUE(std::string(job.get("fromServer").typeName()) == "string");
    EXPECT_TRUE(job.get("fromServer").copyString() == SHARD_LEADER);
    EXPECT_TRUE(std::string(job.get("jobId").typeName()) == "string");
    EXPECT_TRUE(std::string(job.get("timeCreated").typeName()) == "string");

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  auto failedLeader = FailedLeader(baseStructure, &agent, jobId, "unittest",
                                   DATABASE, COLLECTION, SHARD, SHARD_LEADER);
  failedLeader.create();
}

TEST_F(FailedLeaderTest, if_collection_is_missing_job_should_just_finish) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder;
    if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION) {
      return builder;
    }
    builder.reset(new Builder());
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Target/ToDo") {
        builder->add("1", createBuilder(todo).slice());
      }
    } else {
      builder->add(s);
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_TRUE(std::string(q->slice().typeName()) == "array");
    EXPECT_TRUE(q->slice().length() == 1);
    EXPECT_TRUE(std::string(q->slice()[0].typeName()) == "array");
    EXPECT_TRUE(q->slice()[0].length() == 1);  // we always simply override! no preconditions...
    EXPECT_TRUE(std::string(q->slice()[0][0].typeName()) == "object");

    auto writes = q->slice()[0][0];
    EXPECT_TRUE(std::string(writes.get("/arango/Target/ToDo/1").typeName()) ==
                "object");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/ToDo/1").get("op").typeName()) ==
                "string");
    EXPECT_TRUE(writes.get("/arango/Target/ToDo/1").get("op").copyString() ==
                "delete");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Finished/1").typeName()) ==
                "object");
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, distributeshardslike_should_immediately_fail) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION) {
        builder->add("distributeShardsLike", VPackValue("PENG"));
      }
      if (path == "/arango/Target/ToDo") {
        builder->add("1", createBuilder(todo).slice());
      }
    } else {
      builder->add(s);
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_TRUE(std::string(q->slice().typeName()) == "array");
    EXPECT_TRUE(q->slice().length() == 1);
    EXPECT_TRUE(std::string(q->slice()[0].typeName()) == "array");
    EXPECT_TRUE(q->slice()[0].length() == 1);  // we always simply override! no preconditions...
    EXPECT_TRUE(std::string(q->slice()[0][0].typeName()) == "object");

    auto writes = q->slice()[0][0];
    EXPECT_TRUE(std::string(writes.get("/arango/Target/ToDo/1").typeName()) ==
                "object");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/ToDo/1").get("op").typeName()) ==
                "string");
    EXPECT_TRUE(writes.get("/arango/Target/ToDo/1").get("op").copyString() ==
                "delete");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Failed/1").typeName()) ==
                "object");
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, if_leader_is_healthy_we_fail_the_job) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Supervision/Health/" + SHARD_LEADER) {
        builder->add("Status", VPackValue("GOOD"));
      }
      if (path == "/arango/Target/ToDo") {
        builder->add("1", createBuilder(todo).slice());
      }
    } else {
      builder->add(s);
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).Do([&](query_t const& q) -> trans_ret_t {
    auto preconditions = q->slice()[0][1];
    EXPECT_TRUE(preconditions
                    .get("/arango/Supervision/Health/" + SHARD_LEADER + "/Status")
                    .get("old")
                    .copyString() == "FAILED");

    char const* json = R"=([{"arango":{"Supervision":{"Health":{"leader":{"Status":"GOOD"}}}}}])=";
    auto transBuilder = std::make_shared<Builder>(createBuilder(json));
    return trans_ret_t(true, "", 0, 1, transBuilder);
  });
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    auto writes = q->slice()[0][0];
    EXPECT_TRUE(std::string(writes.get("/arango/Target/ToDo/1").get("op").typeName()) ==
                "string");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Failed/1").typeName()) ==
                "object");
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  AgentInterface& agent = mockAgent.get();
  auto failedLeader = FailedLeader(agency(PREFIX), &agent, JOB_STATUS::TODO, jobId);
  ASSERT_FALSE(failedLeader.start(aborts));
  Verify(Method(mockAgent, transact));
  Verify(Method(mockAgent, write)).Exactly(Once);
}

TEST_F(FailedLeaderTest, job_must_not_be_started_if_no_server_is_in_sync) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Target/ToDo") {
        builder->add("1", createBuilder(todo).slice());
      }
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION +
                      "/" + SHARD + "/servers") {
        VPackArrayBuilder a(builder.get());
        builder->add(VPackValue(SHARD_LEADER));
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createNodeFromBuilder(*builder);

  // nothing should happen
  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  ASSERT_FALSE(failedLeader.start(aborts));
}

TEST_F(FailedLeaderTest, job_must_not_be_started_if_distributeshardslike_shard_is_not_in_sync) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }

      if (path == "/arango/Current/Collections/" + DATABASE) {
        // we fake that follower2 is in sync
        char const* json1 = R"=({"s100":{"servers":["leader","follower2"]}})=";
        builder->add("linkedcollection1", createBuilder(json1).slice());
        // for the other shard there is only follower1 in sync
        char const* json2 = R"=({"s101":{"servers":["leader","follower1"]}})=";
        builder->add("linkedcollection2", createBuilder(json2).slice());
      } else if (path == "/arango/Plan/Collections/" + DATABASE) {
        char const* json1 = R"=({"distributeShardsLike":"collection","shards":
          {"s100":["leader","follower1","follower2"]}})=";
        builder->add("linkedcollection1", createBuilder(json1).slice());
        char const* json2 = R"=({"distributeShardsLike":"collection","shards":
          {"s101":["leader","follower1","follower2"]}})=";
        builder->add("linkedcollection2", createBuilder(json2).slice());
      } else if (path == "/arango/Target/ToDo") {
        builder->add("1", createBuilder(todo).slice());
      }
    } else {
      builder->add(s);
    }
    return builder;
  };
  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createNodeFromBuilder(*builder);

  // nothing should happen
  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    EXPECT_TRUE(false);
    return trans_ret_t();
  });
  AgentInterface& agent = mockAgent.get();
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, abort_any_moveshard_job_blocking) {
  Mock<AgentInterface> moveShardMockAgent;

  Builder moveShardBuilder;
  When(Method(moveShardMockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_TRUE(std::string(q->slice().typeName()) == "array");
    EXPECT_TRUE(q->slice().length() == 1);
    EXPECT_TRUE(std::string(q->slice()[0].typeName()) == "array");
    EXPECT_TRUE(q->slice()[0].length() > 0);  // preconditions!
    EXPECT_TRUE(std::string(q->slice()[0][0].typeName()) == "object");
    EXPECT_TRUE(std::string(q->slice()[0][0].get("/arango/Target/ToDo/2").typeName()) ==
                "object");
    moveShardBuilder.add(q->slice()[0][0].get("/arango/Target/ToDo/2"));

    return fakeWriteResult;
  });
  When(Method(moveShardMockAgent, waitFor)).Return();
  AgentInterface& moveShardAgent = moveShardMockAgent.get();
  auto moveShard =
      MoveShard(baseStructure("arango"), &moveShardAgent, "2", "strunz", DATABASE,
                COLLECTION, SHARD, SHARD_LEADER, FREE_SERVER, true, true);
  moveShard.create();

  std::string jobId = "1";
  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder(new Builder());
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Supervision/Shards") {
        builder->add(SHARD, VPackValue("2"));
      } else if (path == "/arango/Target/ToDo") {
        builder->add("1", createBuilder(todo).slice());
      } else if (path == "/arango/Target/Pending") {
        builder->add("2", moveShardBuilder.slice());
      }
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION +
                      "/" + SHARD + "/servers") {
        VPackArrayBuilder a(builder.get());
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER2));
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    // check that moveshard is being moved to failed
    EXPECT_TRUE(std::string(q->slice().typeName()) == "array");
    EXPECT_TRUE(q->slice().length() == 1);
    EXPECT_TRUE(std::string(q->slice()[0].typeName()) == "array");
    EXPECT_TRUE(std::string(q->slice()[0][0].typeName()) == "object");
    EXPECT_TRUE(std::string(q->slice()[0][0].get("/arango/Target/Failed/2").typeName()) ==
                "object");
    return fakeWriteResult;
  });

  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  ASSERT_FALSE(failedLeader.start(aborts));
  Verify(Method(mockAgent, write));
}

TEST_F(FailedLeaderTest, job_should_be_written_to_pending) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder(new Builder());
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Target/ToDo") {
        builder->add("1", createBuilder(todo).slice());
      }
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION +
                      "/" + SHARD + "/servers") {
        VPackArrayBuilder a(builder.get());
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER2));
      } else {
        builder->add(s);
      }
    }
    return builder;
  };
  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    EXPECT_TRUE(std::string(q->slice().typeName()) == "array");
    EXPECT_TRUE(q->slice().length() == 1);
    EXPECT_TRUE(std::string(q->slice()[0].typeName()) == "array");
    EXPECT_TRUE(q->slice()[0].length() == 2);  // preconditions!
    EXPECT_TRUE(std::string(q->slice()[0][0].typeName()) == "object");
    EXPECT_TRUE(std::string(q->slice()[0][1].typeName()) == "object");

    auto writes = q->slice()[0][0];
    EXPECT_TRUE(std::string(writes.get("/arango/Target/ToDo/1").typeName()) ==
                "object");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/ToDo/1").get("op").typeName()) ==
                "string");
    EXPECT_TRUE(writes.get("/arango/Target/ToDo/1").get("op").copyString() ==
                "delete");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/ToDo/1").typeName()) ==
                "object");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Pending/1").typeName()) ==
                "object");

    auto job = writes.get("/arango/Target/Pending/1");
    EXPECT_TRUE(std::string(job.get("toServer").typeName()) == "string");
    EXPECT_TRUE(job.get("toServer").copyString() == SHARD_FOLLOWER2);
    EXPECT_TRUE(std::string(job.get("timeStarted").typeName()) == "string");

    EXPECT_TRUE(std::string(writes
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)
                                .typeName()) == "array");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .length() == 4);
    EXPECT_TRUE(std::string(writes
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)[0]
                                .typeName()) == "string");
    EXPECT_TRUE(std::string(writes
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)[1]
                                .typeName()) == "string");
    EXPECT_TRUE(std::string(writes
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)[2]
                                .typeName()) == "string");
    EXPECT_TRUE(std::string(writes
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)[3]
                                .typeName()) == "string");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[0]
                    .copyString() == SHARD_FOLLOWER2);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[1]
                    .copyString() == SHARD_LEADER);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[2]
                    .copyString() == SHARD_FOLLOWER1);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[3]
                    .copyString()
                    .compare(0, 4, FREE_SERVER) == 0);

    auto preconditions = q->slice()[0][1];
    EXPECT_TRUE(
        std::string(preconditions.get("/arango/Supervision/Shards/" + SHARD).typeName()) ==
        "object");
    EXPECT_TRUE(
        std::string(preconditions.get("/arango/Supervision/Shards/" + SHARD)
                        .get("oldEmpty")
                        .typeName()) == "bool");
    EXPECT_TRUE(
        preconditions.get("/arango/Supervision/Shards/" + SHARD).get("oldEmpty").getBool() == true);
    EXPECT_TRUE(preconditions
                    .get("/arango/Supervision/Health/" + SHARD_LEADER + "/Status")
                    .get("old")
                    .copyString() == "FAILED");
    EXPECT_TRUE(preconditions
                    .get("/arango/Supervision/Health/" + SHARD_FOLLOWER2 + "/Status")
                    .get("old")
                    .copyString() == "GOOD");

    EXPECT_TRUE(std::string(preconditions
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)
                                .typeName()) == "object");
    EXPECT_TRUE(std::string(preconditions
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)
                                .get("old")
                                .typeName()) == "array");
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .get("old")
                    .length() == 3);
    EXPECT_TRUE(std::string(preconditions
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)
                                .get("old")[0]
                                .typeName()) == "string");
    EXPECT_TRUE(std::string(preconditions
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)
                                .get("old")[1]
                                .typeName()) == "string");
    EXPECT_TRUE(std::string(preconditions
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)
                                .get("old")[2]
                                .typeName()) == "string");
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .get("old")[0]
                    .copyString() == SHARD_LEADER);
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .get("old")[1]
                    .copyString() == SHARD_FOLLOWER1);
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .get("old")[2]
                    .copyString() == SHARD_FOLLOWER2);

    auto result = std::make_shared<Builder>();
    result->openArray();
    result->add(VPackValue((uint64_t)1));
    result->close();
    return trans_ret_t(true, "1", 1, 0, result);
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  // new server will randomly be selected...so seed the random number generator
  srand(1);
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, if_collection_is_missing_job_should_just_finish_2) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder;
    if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION) {
      return builder;
    }
    builder.reset(new Builder());
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Target/Pending") {
        Builder jobBuilder;
        {
          VPackObjectBuilder b(&jobBuilder);
          jobBuilder.add("creator", VPackValue("1"));
          jobBuilder.add("type", VPackValue("failedLeader"));
          jobBuilder.add("database", VPackValue(DATABASE));
          jobBuilder.add("collection", VPackValue(COLLECTION));
          jobBuilder.add("shard", VPackValue(SHARD));
          jobBuilder.add("fromServer", VPackValue(SHARD_LEADER));
          jobBuilder.add("toServer", VPackValue(SHARD_FOLLOWER1));
          jobBuilder.add("jobId", VPackValue(jobId));
          jobBuilder.add("timeCreated", VPackValue("2017-01-01 00:00:00"));
        }
        builder->add("1", jobBuilder.slice());
      }
    } else {
      builder->add(s);
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_TRUE(std::string(q->slice().typeName()) == "array");
    EXPECT_TRUE(q->slice().length() == 1);
    EXPECT_TRUE(std::string(q->slice()[0].typeName()) == "array");
    EXPECT_TRUE(q->slice()[0].length() == 1);  // we always simply override! no preconditions...
    EXPECT_TRUE(std::string(q->slice()[0][0].typeName()) == "object");

    auto writes = q->slice()[0][0];
    EXPECT_TRUE(std::string(writes.get("/arango/Target/ToDo/1").typeName()) ==
                "object");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/ToDo/1").get("op").typeName()) ==
                "string");
    EXPECT_TRUE(writes.get("/arango/Target/ToDo/1").get("op").copyString() ==
                "delete");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Finished/1").typeName()) ==
                "object");
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::PENDING, jobId);
  failedLeader.run(aborts);
}

TEST_F(FailedLeaderTest, if_new_leader_doesnt_catch_up_we_wait) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Target/Pending") {
        Builder jobBuilder;
        {
          VPackObjectBuilder b(&jobBuilder);
          jobBuilder.add("creator", VPackValue("1"));
          jobBuilder.add("type", VPackValue("failedLeader"));
          jobBuilder.add("database", VPackValue(DATABASE));
          jobBuilder.add("collection", VPackValue(COLLECTION));
          jobBuilder.add("shard", VPackValue(SHARD));
          jobBuilder.add("fromServer", VPackValue(SHARD_LEADER));
          jobBuilder.add("toServer", VPackValue(SHARD_FOLLOWER1));
          jobBuilder.add("jobId", VPackValue(jobId));
          jobBuilder.add("timeCreated",
                         VPackValue(timepointToString(std::chrono::system_clock::now())));
        }
        builder->add("1", jobBuilder.slice());
      }
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION +
                      "/" + SHARD + "/servers") {
        VPackArrayBuilder a(builder.get());
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                             COLLECTION + "/shards/" + SHARD) {
        VPackArrayBuilder a(builder.get());
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(SHARD_LEADER));
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::PENDING, jobId);
  failedLeader.run(aborts);
}

TEST_F(FailedLeaderTest, if_timeout_job_should_be_aborted) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Target/Pending") {
        Builder jobBuilder;
        {
          VPackObjectBuilder b(&jobBuilder);
          jobBuilder.add("creator", VPackValue("1"));
          jobBuilder.add("type", VPackValue("failedLeader"));
          jobBuilder.add("database", VPackValue(DATABASE));
          jobBuilder.add("collection", VPackValue(COLLECTION));
          jobBuilder.add("shard", VPackValue(SHARD));
          jobBuilder.add("fromServer", VPackValue(SHARD_LEADER));
          jobBuilder.add("toServer", VPackValue(SHARD_FOLLOWER1));
          jobBuilder.add("jobId", VPackValue(jobId));
          jobBuilder.add("timeCreated", VPackValue("2015-01-03T20:00:00Z"));
        }
        builder->add("1", jobBuilder.slice());
      }
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION +
                      "/" + SHARD + "/servers") {
        VPackArrayBuilder a(builder.get());
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                             COLLECTION + "/shards/" + SHARD) {
        VPackArrayBuilder a(builder.get());
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(SHARD_LEADER));
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_TRUE(std::string(q->slice().typeName()) == "array");
    EXPECT_TRUE(q->slice().length() == 1);
    EXPECT_TRUE(std::string(q->slice()[0].typeName()) == "array");
    EXPECT_TRUE(q->slice()[0].length() == 2);  // we always simply override! no preconditions...
    EXPECT_TRUE(std::string(q->slice()[0][0].typeName()) == "object");
    auto preconditions = q->slice()[0][1];
    EXPECT_TRUE(preconditions.get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION).get("oldEmpty").isFalse());

    auto writes = q->slice()[0][0];
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Pending/1").typeName()) ==
                "object");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Pending/1").get("op").typeName()) ==
                "string");
    EXPECT_TRUE(writes.get("/arango/Target/Pending/1").get("op").copyString() ==
                "delete");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Failed/1").typeName()) ==
                "object");
    EXPECT_TRUE(std::string(writes
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)
                                .typeName()) == "array");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[0]
                    .copyString() == SHARD_LEADER);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[1]
                    .copyString() == SHARD_FOLLOWER1);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::PENDING, jobId);
  failedLeader.run(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(FailedLeaderTest, when_everything_is_finished_there_should_be_cleanup) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Target/Pending") {
        Builder jobBuilder;
        {
          VPackObjectBuilder b(&jobBuilder);
          jobBuilder.add("creator", VPackValue("1"));
          jobBuilder.add("type", VPackValue("failedLeader"));
          jobBuilder.add("database", VPackValue(DATABASE));
          jobBuilder.add("collection", VPackValue(COLLECTION));
          jobBuilder.add("shard", VPackValue(SHARD));
          jobBuilder.add("fromServer", VPackValue(SHARD_LEADER));
          jobBuilder.add("toServer", VPackValue(SHARD_FOLLOWER1));
          jobBuilder.add("jobId", VPackValue(jobId));
          jobBuilder.add("timeCreated",
                         VPackValue(timepointToString(std::chrono::system_clock::now())));
        }
        builder->add("1", jobBuilder.slice());
      }
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION +
                      "/" + SHARD + "/servers") {
        VPackArrayBuilder a(builder.get());
        builder->add(VPackValue(SHARD_FOLLOWER1));
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                             COLLECTION + "/shards/" + SHARD) {
        VPackArrayBuilder a(builder.get());
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(SHARD_LEADER));
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_TRUE(std::string(q->slice().typeName()) == "array");
    EXPECT_TRUE(q->slice().length() == 1);
    EXPECT_TRUE(std::string(q->slice()[0].typeName()) == "array");
    EXPECT_TRUE(q->slice()[0].length() == 1);  // we always simply override! no preconditions...
    EXPECT_TRUE(std::string(q->slice()[0][0].typeName()) == "object");

    auto writes = q->slice()[0][0];
    EXPECT_TRUE(std::string(writes.get("/arango/Supervision/Shards/" + SHARD).typeName()) ==
                "object");
    EXPECT_TRUE(writes.get("/arango/Supervision/Shards/" + SHARD).get("op").copyString() ==
                "delete");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Pending/1").get("op").typeName()) ==
                "string");
    EXPECT_TRUE(writes.get("/arango/Target/Pending/1").get("op").copyString() ==
                "delete");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Finished/1").typeName()) ==
                "object");
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::PENDING, jobId);
  failedLeader.run(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(FailedLeaderTest, failedleader_must_not_take_follower_into_account_if_it_has_dropped_out) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder(new Builder());
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Target/ToDo") {
        builder->add("1", createBuilder(todo).slice());
      }
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION +
                      "/" + SHARD + "/servers") {
        // follower2 in sync
        VPackArrayBuilder a(builder.get());
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER2));
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                             COLLECTION + "/shards/" + SHARD) {
        // but not part of the plan => will drop collection on next occasion
        VPackArrayBuilder a(builder.get());
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
      } else {
        builder->add(s);
      }
    }
    return builder;
  };
  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // must NOT be called!
    EXPECT_TRUE(false);
    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  // new server will randomly be selected...so seed the random number generator
  srand(1);
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

}  // namespace failed_leader_test
}  // namespace tests
}  // namespace arangodb
