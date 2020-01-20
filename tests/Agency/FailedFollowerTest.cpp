////////////////////////////////////////////////////////////////////////////////
/// @brief test case for FailedFollower job
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

#include <iostream>

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Mocks/LogLevels.h"

#include "Agency/AgentInterface.h"
#include "Agency/FailedFollower.h"
#include "Agency/MoveShard.h"
#include "Agency/Node.h"
#include "Basics/StringUtils.h"
#include "Random/RandomGenerator.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::consensus;
using namespace fakeit;

namespace arangodb {
namespace tests {
namespace failed_follower_test {

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

typedef std::function<std::unique_ptr<Builder>(Slice const&, std::string const&)> TestStructureType;

const char* agency =
#include "FailedFollowerTest.json"
    ;

VPackBuilder createJob() {
  VPackBuilder builder;
  {
    VPackObjectBuilder a(&builder);
    builder.add("creator", VPackValue("1"));
    builder.add("type", VPackValue("failedFollower"));
    builder.add("database", VPackValue(DATABASE));
    builder.add("collection", VPackValue(COLLECTION));
    builder.add("shard", VPackValue(SHARD));
    builder.add("fromServer", VPackValue(SHARD_FOLLOWER1));
    builder.add("jobId", VPackValue("1"));
    builder.add("timeCreated",
                VPackValue(timepointToString(std::chrono::system_clock::now())));
  }
  return builder;
}

Node createNodeFromBuilder(VPackBuilder const& builder) {
  VPackBuilder opBuilder;
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

  VPackBuilder builder;
  builder.add(parser.steal()->slice());
  return builder;
}

Node createNode(char const* c) {
  return createNodeFromBuilder(createBuilder(c));
}

Node createRootNode() { return createNode(agency); }

class FailedFollowerTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::SUPERVISION, arangodb::LogLevel::ERR> {
 protected:
  std::shared_ptr<Builder> transBuilder;
  Node baseStructure;
  write_ret_t fakeWriteResult;
  trans_ret_t fakeTransResult;

  FailedFollowerTest()
      : transBuilder(std::make_shared<Builder>()),
        baseStructure(createRootNode()),
        fakeWriteResult(true, "", std::vector<apply_ret_t>{APPLIED},
                        std::vector<index_t>{1}),
        fakeTransResult(true, "", 1, 0, transBuilder) {
    RandomGenerator::seed(3);
    VPackArrayBuilder a(transBuilder.get());
    transBuilder->add(VPackValue((uint64_t)1));
  }
};

TEST_F(FailedFollowerTest, creating_a_job_should_create_a_job_in_todo) {
  Mock<AgentInterface> mockAgent;

  std::string jobId = "1";
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    auto expectedJobKey = "/arango/Target/ToDo/" + jobId;
    EXPECT_EQ(std::string(q->slice().typeName()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(std::string(q->slice()[0].typeName()), "array");
    EXPECT_EQ(q->slice()[0].length(), 1);  // we always simply override! no preconditions...
    EXPECT_EQ(std::string(q->slice()[0][0].typeName()), "object");
    EXPECT_EQ(q->slice()[0][0].length(), 1);  // should ONLY do an entry in todo
    EXPECT_TRUE(std::string(q->slice()[0][0].get(expectedJobKey).typeName()) ==
                "object");

    auto job = q->slice()[0][0].get(expectedJobKey);
    EXPECT_EQ(std::string(job.get("creator").typeName()), "string");
    EXPECT_EQ(std::string(job.get("type").typeName()), "string");
    EXPECT_EQ(job.get("type").copyString(), "failedFollower");
    EXPECT_EQ(std::string(job.get("database").typeName()), "string");
    EXPECT_EQ(job.get("database").copyString(), DATABASE);
    EXPECT_EQ(std::string(job.get("collection").typeName()), "string");
    EXPECT_EQ(job.get("collection").copyString(), COLLECTION);
    EXPECT_EQ(std::string(job.get("shard").typeName()), "string");
    EXPECT_EQ(job.get("shard").copyString(), SHARD);
    EXPECT_EQ(std::string(job.get("fromServer").typeName()), "string");
    EXPECT_EQ(job.get("fromServer").copyString(), SHARD_FOLLOWER1);
    EXPECT_EQ(std::string(job.get("jobId").typeName()), "string");
    EXPECT_EQ(std::string(job.get("timeCreated").typeName()), "string");

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  auto failedFollower = FailedFollower(baseStructure, &agent, jobId, "unittest",
                                       DATABASE, COLLECTION, SHARD, SHARD_FOLLOWER1);
  failedFollower.create();
  Verify(Method(mockAgent, write));
}

TEST_F(FailedFollowerTest, if_collection_is_missing_job_should_just_finish) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](VPackSlice const& s, std::string const& path) {
    std::unique_ptr<VPackBuilder> builder;
    if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION) {
      return builder;
    }

    builder.reset(new VPackBuilder());
    if (s.isObject()) {
      builder->add(VPackValue(VPackValueType::Object));
      for (auto it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }

      if (path == "/arango/Target/ToDo") {
        builder->add("1", createJob().slice());
      }
      builder->close();
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
    EXPECT_EQ(std::string(q->slice().typeName()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(std::string(q->slice()[0].typeName()), "array");
    EXPECT_EQ(q->slice()[0].length(), 1);  // we always simply override! no preconditions...
    EXPECT_EQ(std::string(q->slice()[0][0].typeName()), "object");

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
  auto failedFollower = FailedFollower(agency(PREFIX), &agent, JOB_STATUS::TODO, jobId);
  failedFollower.start(aborts);
}

TEST_F(FailedFollowerTest, distributeshardslike_should_fail_immediately) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](VPackSlice const& s, std::string const& path) {
    std::unique_ptr<VPackBuilder> builder;
    builder.reset(new VPackBuilder());
    if (s.isObject()) {
      builder->add(VPackValue(VPackValueType::Object));
      for (auto it : VPackObjectIterator(s)) {
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
        builder->add("1", createJob().slice());
      }
      builder->close();
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
    EXPECT_EQ(std::string(q->slice().typeName()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(std::string(q->slice()[0].typeName()), "array");
    EXPECT_EQ(q->slice()[0].length(), 1);  // we always simply override! no preconditions...
    EXPECT_EQ(std::string(q->slice()[0][0].typeName()), "object");

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
  auto failedFollower = FailedFollower(agency(PREFIX), &agent, JOB_STATUS::TODO, jobId);
  failedFollower.start(aborts);
}

TEST_F(FailedFollowerTest, if_follower_is_healthy_again_we_fail_the_job) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](VPackSlice const& s, std::string const& path) {
    std::unique_ptr<VPackBuilder> builder;
    builder.reset(new VPackBuilder());
    if (s.isObject()) {
      builder->add(VPackValue(VPackValueType::Object));
      for (auto it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }

      if (path == "/arango/Supervision/Health/" + SHARD_FOLLOWER1) {
        builder->add("Status", VPackValue("GOOD"));
      }

      if (path == "/arango/Target/ToDo") {
        builder->add("1", createJob().slice());
      }
      builder->close();
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
                    .get("/arango/Supervision/Health/" + SHARD_FOLLOWER1 + "/Status")
                    .get("old")
                    .copyString() == "FAILED");

    char const* json = R"=([{"arango":{"Supervision":{"Health":{"follower1":{"Status":"GOOD"}}}}}])=";
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
  auto failedFollower = FailedFollower(agency(PREFIX), &agent, JOB_STATUS::TODO, jobId);
  ASSERT_FALSE(failedFollower.start(aborts));
  Verify(Method(mockAgent, transact));
  Verify(Method(mockAgent, write));
}

TEST_F(FailedFollowerTest, if_there_is_no_healthy_free_server_at_start_just_finish) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](VPackSlice const& s, std::string const& path) {
    std::unique_ptr<VPackBuilder> builder;
    builder.reset(new VPackBuilder());
    if (s.isObject()) {
      builder->add(VPackValue(VPackValueType::Object));
      for (auto it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }

      if (path == "/arango/Target/ToDo") {
        builder->add("1", createJob().slice());
      }
      builder->close();
    } else {
      if (path == "/arango/Supervision/Health/free/Status") {
        builder->add(VPackValue("FAILED"));
      } else if (path == "/arango/Supervision/Health/free2/Status") {
        builder->add(VPackValue("FAILED"));
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
    EXPECT_EQ(std::string(q->slice().typeName()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(std::string(q->slice()[0].typeName()), "array");
    EXPECT_EQ(std::string(q->slice()[0][0].typeName()), "object");
    EXPECT_TRUE(std::string(q->slice()[0][0].get("/arango/Target/Failed/1").typeName()) ==
                "object");
    return fakeWriteResult;
  });

  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();
  auto failedFollower = FailedFollower(agency(PREFIX), &agent, JOB_STATUS::TODO, jobId);
  ASSERT_FALSE(failedFollower.start(aborts));
}

TEST_F(FailedFollowerTest, abort_any_moveshard_job_blocking) {
  Mock<AgentInterface> moveShardMockAgent;
  Builder moveShardBuilder;
  When(Method(moveShardMockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_EQ(std::string(q->slice().typeName()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(std::string(q->slice()[0].typeName()), "array");
    EXPECT_TRUE(q->slice()[0].length() > 0);  // preconditions!
    EXPECT_EQ(std::string(q->slice()[0][0].typeName()), "object");
    EXPECT_TRUE(std::string(q->slice()[0][0].get("/arango/Target/ToDo/2").typeName()) ==
                "object");
    moveShardBuilder.add(q->slice()[0][0].get("/arango/Target/ToDo/2"));
    return fakeWriteResult;
  });
  When(Method(moveShardMockAgent, waitFor)).Return();
  AgentInterface& moveShardAgent = moveShardMockAgent.get();
  auto moveShard =
      MoveShard(baseStructure(PREFIX), &moveShardAgent, "2", "strunz", DATABASE,
                COLLECTION, SHARD, SHARD_LEADER, FREE_SERVER, true, true);
  moveShard.create();
  std::string jobId = "1";
  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder(new Builder());
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Supervision/Shards") {
        builder->add(SHARD, VPackValue("2"));

      } else if (path == "/arango/Target/ToDo") {
        builder->add("1", createJob().slice());

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
  // nothing should happen
  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode const& w) -> write_ret_t {
    // check that moveshard is being moved to failed
    EXPECT_EQ(std::string(q->slice().typeName()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(std::string(q->slice()[0].typeName()), "array");
    EXPECT_EQ(std::string(q->slice()[0][0].typeName()), "object");
    EXPECT_TRUE(std::string(q->slice()[0][0].get("/arango/Target/Failed/2").typeName()) ==
                "object");
    return fakeWriteResult;
  });

  AgentInterface& agent = mockAgent.get();
  auto failedFollower = FailedFollower(agency(PREFIX), &agent, JOB_STATUS::TODO, jobId);
  ASSERT_FALSE(failedFollower.start(aborts));
}

TEST_F(FailedFollowerTest, successfully_started_jbo_should_finish_immediately) {
  std::string jobId = "1";
  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder(new Builder());
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }

      if (path == "/arango/Target/ToDo") {
        builder->add("1", createJob().slice());
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
  When(Method(mockAgent, transact)).Do([&](query_t const& q) -> trans_ret_t {
    // check that the job is now pending
    auto writes = q->slice()[0][0];
    auto planEntry = "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION +
                     "/shards/" + SHARD;
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Finished/1").typeName()) ==
                "object");
    EXPECT_EQ(std::string(writes.get(planEntry).typeName()), "array");
    EXPECT_EQ(writes.get(planEntry).length(), 4);
    EXPECT_EQ(writes.get(planEntry)[0].copyString(), "leader");
    auto freeEntry = writes.get(planEntry)[1].copyString();
    EXPECT_EQ(freeEntry.compare(0, 4, FREE_SERVER), 0);
    EXPECT_EQ(writes.get(planEntry)[2].copyString(), "follower2");
    EXPECT_EQ(writes.get(planEntry)[3].copyString(), "follower1");

    EXPECT_TRUE(writes.get("/arango/Plan/Version").get("op").copyString() ==
                "increment");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Finished/1").typeName()) ==
                "object");

    auto preconditions = q->slice()[0][1];
    EXPECT_EQ(std::string(preconditions.typeName()), "object");
    auto healthStat =
        std::string("/arango/Supervision/Health/") + freeEntry + "/Status";
    EXPECT_TRUE(preconditions.get(healthStat).get("old").copyString() ==
                "GOOD");
    EXPECT_TRUE(std::string(preconditions.get(planEntry).get("old").typeName()) ==
                "array");
    EXPECT_TRUE(preconditions.get(planEntry).get("old")[0].copyString() ==
                "leader");
    EXPECT_TRUE(preconditions.get(planEntry).get("old")[1].copyString() ==
                "follower1");
    EXPECT_TRUE(preconditions.get(planEntry).get("old")[2].copyString() ==
                "follower2");
    EXPECT_TRUE(preconditions
                    .get(std::string("/arango/Supervision/DBServers/") + freeEntry)
                    .get("oldEmpty")
                    .getBool() == true);
    EXPECT_TRUE(
        preconditions.get("/arango/Supervision/Shards/s99").get("oldEmpty").getBool() == true);

    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();
  auto failedFollower = FailedFollower(agency(PREFIX), &agent, JOB_STATUS::TODO, jobId);
  failedFollower.start(aborts);
  Verify(Method(mockAgent, transact));
}

TEST_F(FailedFollowerTest, job_should_handle_distributeshardslike) {
  std::string jobId = "1";
  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder(new Builder());
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto it : VPackObjectIterator(s)) {
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
        builder->add("1", createJob().slice());
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
  When(Method(mockAgent, transact)).Do([&](query_t const& q) -> trans_ret_t {
    // check that the job is now pending
    auto writes = q->slice()[0][0];
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Finished/1").typeName()) ==
                "object");
    auto entry = std::string("/arango/Plan/Collections/") + DATABASE + "/" +
                 COLLECTION + "/shards/" + SHARD;
    EXPECT_EQ(std::string(writes.get(entry).typeName()), "array");
    EXPECT_EQ(writes.get(entry).length(), 4);
    EXPECT_EQ(writes.get(entry)[0].copyString(), "leader");
    auto freeEntry = writes.get(entry)[1].copyString();
    EXPECT_EQ(writes.get(entry)[1].copyString().compare(0, 4, FREE_SERVER), 0);
    EXPECT_EQ(writes.get(entry)[2].copyString(), "follower2");
    EXPECT_EQ(writes.get(entry)[3].copyString(), "follower1");
    EXPECT_TRUE(std::string(writes
                                .get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection1/shards/s100")
                                .typeName()) == "array");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection1/shards/s100")
                    .length() == 4);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE +
                         "/linkedcollection1/shards/s100")[0]
                    .copyString() == "leader");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE +
                         "/linkedcollection1/shards/s100")[1]
                    .copyString()
                    .compare(0, 4, FREE_SERVER) == 0);

    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE +
                         "/linkedcollection1/shards/s100")[2]
                    .copyString() == "follower2");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE +
                         "/linkedcollection1/shards/s100")[3]
                    .copyString() == "follower1");
    EXPECT_TRUE(std::string(writes
                                .get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection2/shards/s101")
                                .typeName()) == "array");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection2/shards/s101")
                    .length() == 4);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE +
                         "/linkedcollection2/shards/s101")[0]
                    .copyString() == "leader");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE +
                         "/linkedcollection2/shards/s101")[1]
                    .copyString()
                    .compare(0, 4, FREE_SERVER) == 0);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE +
                         "/linkedcollection2/shards/s101")[2]
                    .copyString() == "follower2");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE +
                         "/linkedcollection2/shards/s101")[3]
                    .copyString() == "follower1");

    EXPECT_TRUE(writes.get("/arango/Plan/Version").get("op").copyString() ==
                "increment");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Finished/1").typeName()) ==
                "object");

    auto preconditions = q->slice()[0][1];
    EXPECT_EQ(std::string(preconditions.typeName()), "object");
    auto healthStat =
        std::string("/arango/Supervision/Health/") + freeEntry + "/Status";
    EXPECT_TRUE(preconditions.get(healthStat).get("old").copyString() ==
                "GOOD");
    EXPECT_TRUE(std::string(preconditions.get(entry).get("old").typeName()) ==
                "array");
    EXPECT_TRUE(preconditions.get(entry).get("old")[0].copyString() ==
                "leader");
    EXPECT_TRUE(preconditions.get(entry).get("old")[1].copyString() ==
                "follower1");
    EXPECT_TRUE(preconditions.get(entry).get("old")[2].copyString() ==
                "follower2");
    EXPECT_TRUE(preconditions
                    .get(std::string("/arango/Supervision/DBServers/") + freeEntry)
                    .get("oldEmpty")
                    .getBool() == true);
    EXPECT_TRUE(
        preconditions.get("/arango/Supervision/Shards/s99").get("oldEmpty").getBool() == true);

    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();
  auto failedFollower = FailedFollower(agency(PREFIX), &agent, JOB_STATUS::TODO, jobId);
  failedFollower.start(aborts);
  Verify(Method(mockAgent, transact));
}

TEST_F(FailedFollowerTest, job_should_timeout_after_a_while) {
  std::string jobId = "1";
  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder(new Builder());
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }

      if (path == "/arango/Target/ToDo") {
        VPackBuilder todoJob = createJob();
        VPackBuilder timedOutJob;
        {
          VPackObjectBuilder a(&timedOutJob);
          for (auto it : VPackObjectIterator(todoJob.slice())) {
            if (it.key.copyString() == "timeCreated") {
              timedOutJob.add("timeCreated",
                              VPackValue("2015-01-01T00:00:00Z"));
            } else {
              timedOutJob.add(it.key.copyString(), it.value);
            }
          }
        }
        builder->add("1", timedOutJob.slice());
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
    // check that the job is now pending
    auto writes = q->slice()[0][0];
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Failed/1").typeName()) ==
                "object");

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();
  auto failedFollower = FailedFollower(agency(PREFIX), &agent, JOB_STATUS::TODO, jobId);
  failedFollower.start(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(FailedFollowerTest, job_should_be_abortable_in_todo) {
  std::string jobId = "1";
  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder(new Builder());
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }

      if (path == "/arango/Target/ToDo") {
        builder->add("1", createJob().slice());
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
    // check that the job is now pending
    auto writes = q->slice()[0][0];
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Failed/1").typeName()) ==
                "object");

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();
  auto failedFollower = FailedFollower(agency(PREFIX), &agent, JOB_STATUS::TODO, jobId);
  failedFollower.abort("test abort");
  Verify(Method(mockAgent, write));
}

}  // namespace failed_follower_test
}  // namespace tests
}  // namespace arangodb
