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

#include <iostream>
#include <numeric>

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include <velocypack/Collection.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Mocks/LogLevels.h"

#include "Agency/AgentInterface.h"
#include "Agency/FailedLeader.h"
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

std::unordered_set<std::string> getKeySet(VPackSlice s) {
  std::unordered_set<std::string> keys;

  for (auto const& kv : VPackObjectIterator(s)) {
    keys.insert(kv.key.copyString());
  }

  return keys;
}

Node createRootNode() { return createNode(agency); }

char const* todo = R"=({
  "creator":"1", "type":"failedLeader", "database":"database",
  "collection":"collection", "shard":"s99", "fromServer":"leader",
  "jobId":"1", "timeCreated":"2017-01-01 00:00:00"
  })=";

typedef std::function<std::unique_ptr<Builder>(Slice const&, std::string const&)> TestStructureType;

class FailedLeaderTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::SUPERVISION, arangodb::LogLevel::ERR> {
 protected:

  struct ShardInfo {
    std::string database;
    std::string collection;
    std::string shard;
    bool isFollower = false;
  };


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

  auto resigned(std::string const& server) const -> std::string {
    // Note a server as resigned
    return "_" + server;
  }

  auto unresign(std::string const& server) const -> std::string {
    // Remove the "is resigned" notation
    if (server[0] == '_') {
      return server.substr(1);
    }
    return server;
  }

  void AssertTransactionFormat(query_t const& q) {
    auto slice = q->slice();
    ASSERT_TRUE(slice.isArray());
    // This test only does one transaction per envelope
    EXPECT_EQ(slice.length(), 1);
    slice = slice.at(0);
    ASSERT_TRUE(slice.isArray());
    // This test only does one transaction per envelope
    EXPECT_GE(slice.length(), 1); // At least [[write]]
    EXPECT_LE(slice.length(), 2); // At most [[write, precondition]]
  }

  void AssertVersionIncremented(query_t const& q) {
    auto w = getWritePartUnsafe(q);
    auto path = "/arango/Plan/Version";
    ASSERT_TRUE(w.hasKey(path));
    w = w.get(path);
    ASSERT_TRUE(w.isObject());
    EXPECT_TRUE(w.hasKey("op"));
    EXPECT_TRUE(w.get("op").isEqualString("increment"));
  }

  void AssertShardLocked(query_t const& q, ShardInfo const& si) {
    if (si.isFollower) {
      // Nothing to check for followers
      return;
    }
    auto w = getWritePartUnsafe(q);
    auto path = "/arango/Supervision/Shards/" + si.shard;
    ASSERT_TRUE(w.hasKey(path));
    w = w.get(path);
    EXPECT_TRUE(w.isEqualString("1"));
  }

  void AssertJobMovedToPending(query_t const& q, ShardInfo const& si, std::string const& jobId) {
    auto w = getWritePartUnsafe(q);
    {
      // added to pending
      auto path = "/arango/Target/Pending/" + jobId;
      ASSERT_TRUE(w.hasKey(path));

      auto pending = w.get(path);
      ASSERT_TRUE(pending.isObject());
      auto jobJobId = VelocyPackHelper::getStringValue(pending, "jobId", "INVALID");
      EXPECT_EQ(jobJobId, jobId);
      auto jobType = VelocyPackHelper::getStringValue(pending, "type", "INVALID");
      EXPECT_EQ(jobType, "failedLeader");
      if (!si.isFollower) {
        auto jobShard =
            VelocyPackHelper::getStringValue(pending, "shard", "INVALID");
        EXPECT_EQ(jobShard, si.shard);
      }
    }
    {
      // Removed from TODO
      auto path = "/arango/Target/ToDo/" + jobId;
      ASSERT_TRUE(w.hasKey(path));
      auto deleteOp = w.get(path);
      ASSERT_TRUE(deleteOp.isObject());
      EXPECT_TRUE(deleteOp.hasKey("op"));
      EXPECT_TRUE(deleteOp.get("op").isEqualString("delete"));
    }
  }

  void AssertNewServers(query_t const& q, ShardInfo const& si, std::vector<std::string> const& expectedServers) {
    auto w = getWritePartUnsafe(q);
    auto path = "/arango/Plan/Collections/" + si.database + "/" + si.collection + "/shards/" + si.shard;
    ASSERT_TRUE(w.hasKey(path));
    auto nextGenServers = w.get(path);
    ASSERT_TRUE(nextGenServers.isArray());
    size_t index = 0;
    ASSERT_EQ(expectedServers.size(), nextGenServers.length());
    for (auto const& it : VPackArrayIterator(nextGenServers)) {
      ASSERT_TRUE(it.isString());
      EXPECT_EQ(it.copyString(), expectedServers[index++]);
    }
  }

  void AssertPreconditions(query_t const& q, ShardInfo const& si, std::vector<std::string> const& expectedServers,
                           std::vector<std::string> const& lastGenPlan,
                           std::vector<std::string> const& lastGenFollowers,
                           std::vector<std::string> const& lastGenFailoverCandidates) {
    auto pre = getPreconditionPartUnsafe(q);

    {
      // Section: Locking, and server status.
      ASSERT_FALSE(lastGenPlan.empty());
      ASSERT_FALSE(expectedServers.empty());
      // Old leader value is used for lock-checking on the Server
      // This cannot be done with a resigend leader value
      auto const& oldLeader = unresign(lastGenPlan.front());
      auto const& newLeader = expectedServers.front();
      // Leader Shard is not locked
      if (!si.isFollower) {
        auto path = "/arango/Supervision/Shards/" + si.shard;
        ASSERT_TRUE(pre.hasKey(path));
        AssertOldEmptyObject(pre.get(path));
      }
      // New Leader is not locked (this lock is captured by failedServerJob)
      {
        auto path = "/arango/Supervision/DBServers/" + newLeader;
        ASSERT_TRUE(pre.hasKey(path));
        AssertOldEmptyObject(pre.get(path));
      }
      {
        // New leader is healty
        auto path = "/arango/Supervision/Health/" + newLeader + "/Status";
        ASSERT_TRUE(pre.hasKey(path));
        AssertOldIsString(pre.get(path), "GOOD");
      }
      {
        // Old leader is still failed
        auto path = "/arango/Supervision/Health/" + oldLeader + "/Status";
        ASSERT_TRUE(pre.hasKey(path)) << " testing: " << oldLeader;
        AssertOldIsString(pre.get(path), "FAILED");
      }
    }
    {
      // Section: Protection against lost plan updates:
      if (!si.isFollower) {
        // Plan, only the leader needs to be unmodified. The Followers can only get a new plan
        // version with the leader changing
        auto path = "/arango/Plan/Collections/" + si.database + "/" + si.collection + "/shards/" + si.shard;
        ASSERT_TRUE(pre.hasKey(path));
        AssertOldIsArray(pre.get(path), lastGenPlan);
      }
      {
        // Followers
        auto path =
            "/arango/Current/Collections/" + si.database + "/" + si.collection + "/" + si.shard + "/servers";
        ASSERT_TRUE(pre.hasKey(path));
        AssertOldIsArray(pre.get(path), lastGenFollowers);
      }
      {
        // Failover candidates
        auto path =
            "/arango/Current/Collections/" + si.database + "/" + si.collection + "/" + si.shard + "/failoverCandidates";
        if (lastGenFailoverCandidates.empty()) {
          // For old collections we should not assert failoverCandidates
          // Backwards compatibility, otherwise we could potentially not
          // failover
          ASSERT_FALSE(pre.hasKey(path));
        } else {
          ASSERT_TRUE(pre.hasKey(path));
          AssertOldIsArray(pre.get(path), lastGenFailoverCandidates);
        }
      }
    }
  }

  void AssertIsValidTransaction(query_t const& q, ShardInfo const& si,
                                std::string const& jobId,
                                std::vector<std::string> const& expectedServers,
                                std::vector<std::string> const& lastGenPlan,
                                std::vector<std::string> const& lastGenFollowers,
                                std::vector<std::string> const& lastGenFailoverCandidates) {
    AssertTransactionFormat(q);
    AssertVersionIncremented(q);
    AssertShardLocked(q, si);
    AssertJobMovedToPending(q, si, jobId);
    AssertNewServers(q, si, expectedServers);
    AssertPreconditions(q, si, expectedServers, lastGenPlan, lastGenFollowers,
                        lastGenFailoverCandidates);
  }

  class AgencyBuilder {

        // Inserts the given string as content of given collection in plan
    auto injectIntoPlan(ShardInfo const& si, std::string const& content) -> std::string {
      auto jsonString = std::string(R"({"arango": {"Plan": {"Collections": {")");
      jsonString += si.database;
      jsonString += std::string(R"(": {")");
      jsonString += si.collection;
      jsonString += std::string(R"(": )");
      jsonString += content;
      jsonString += std::string(R"( } } } } } )");
      return jsonString;
    }
    
    // Inserts the given string as content of given shard in Current
    // Requires content to be a JSON Object
    auto injectIntoCurrentEntry(ShardInfo const& si, std::string const& content) -> std::string {
      auto jsonString = std::string(R"({"arango": {"Current": {"Collections": {")");
      jsonString += si.database;
      jsonString += std::string(R"(": {")");
      jsonString += si.collection;
      jsonString += std::string(R"(": {")");
      jsonString += si.shard;
      jsonString += std::string(R"(": )");
      jsonString += content;
      jsonString += std::string(R"( } } } } } } )");
      return jsonString;
    }


   public:
    AgencyBuilder(VPackBuilder&& base) : _builder(std::move(base)) {}

    auto setPlannedServers(ShardInfo const& si, std::vector<std::string> const& servers) -> AgencyBuilder& {
      auto content = std::string(R"({"shards": {")");
      content += si.shard;
      content += std::string(R"(": )");
      content += vectorToArray(servers);
      content += std::string(R"(} })");
     
      auto jsonString = injectIntoPlan(si, std::move(content));
      return applyJson(std::move(jsonString));
    };

    auto setDistributeShardsLike(ShardInfo const& follower, ShardInfo const& leader) -> AgencyBuilder& {
      // Create a ShardInfo with "follower == true", otherwise the tests lader will be off.
      TRI_ASSERT(follower.isFollower);
      // A leader cannot be a follower at the same time.
      TRI_ASSERT(!leader.isFollower);
      auto content = std::string(R"({"distributeShardsLike": ")");
      content += leader.collection;
      content += std::string(R"(" })");
      auto jsonString = injectIntoPlan(follower, std::move(content));
      return applyJson(std::move(jsonString)); 
    }

    auto setFailoverCandidates(ShardInfo const& si, std::vector<std::string> const& servers) -> AgencyBuilder& {
      auto content = std::string(R"({"failoverCandidates": )");
      content += vectorToArray(servers);
      content += std::string(R"(})");
      auto jsonString = injectIntoCurrentEntry(si, std::move(content));
      return applyJson(std::move(jsonString));
    };

    auto setFollowers(ShardInfo const& si, std::vector<std::string> const& servers) -> AgencyBuilder& {
      auto content = std::string(R"({"servers": )");
      content += vectorToArray(servers);
      content += std::string(R"(})");
      auto jsonString = injectIntoCurrentEntry(si, std::move(content));
      return applyJson(std::move(jsonString));
    };

    auto setServerFailed(std::string const& server) -> AgencyBuilder& {
      auto jsonString = std::string(R"({"arango": {"Supervision": {"Health": {")") +
                        server + std::string(R"(": {"Status": "FAILED" } } } } })");
      return applyJson(std::move(jsonString));
    };

    auto setJobInTodo(std::string const& jobId) -> AgencyBuilder& {
      auto jsonString = std::string(R"({"arango": {"Target": {"ToDo": {"1": )");
      jsonString += std::string(todo);
      jsonString += std::string(R"(} } } } )");
      return applyJson(std::move(jsonString));
    }

    auto createNode() const -> Node {
      return createNodeFromBuilder(_builder);
    }

   private:
    auto vectorToArray(std::vector<std::string> servers) -> std::string {
      TRI_ASSERT(!servers.empty());
      // NOTE: The strings are surrounded by ""
      std::string jsonString = "[";
      bool isFirst = true;
      for (auto const& s : servers) {
        if (!isFirst) {
          jsonString += ",";
        }
        isFirst = false;
        jsonString += "\"" + s + "\"";
      }
      jsonString += "]";
      return jsonString;
    };

    auto applyJson(std::string&& jsonString) -> AgencyBuilder& {
      auto parsed = VPackParser::fromJson(std::move(jsonString));
      _builder = VPackCollection::merge(_builder.slice(), parsed->slice(), true);
      return *this;
    }

    VPackBuilder _builder;
  };

 private:
  // Internal helpers, do not call in test code.

  auto getWritePartUnsafe(query_t const& q) -> VPackSlice {
    // Call AssertTransactionFormat forst to ensure formatting
    return q->slice().at(0).at(0);
  }

  auto getPreconditionPartUnsafe(query_t const& q) -> VPackSlice {
    // Call AssertTransactionFormat forst to ensure formatting
    return q->slice().at(0).at(1);
  }

  void AssertOldEmptyObject(VPackSlice obj) {
    ASSERT_TRUE(obj.isObject());
    // Will be set to false if ommited, or actively se t to false
    bool oldEmpty = VelocyPackHelper::getBooleanValue(obj, "oldEmpty", false);
    // Required to be TRUE!
    EXPECT_TRUE(oldEmpty);
  }

  void AssertOldIsString(VPackSlice obj, std::string const& expected) {
    ASSERT_TRUE(obj.isObject());
    ASSERT_TRUE(obj.hasKey("old"));
    ASSERT_TRUE(obj.get("old").isEqualString(expected));
  }

  void AssertOldIsArray(VPackSlice obj, std::vector<std::string> const& expected) {
    ASSERT_TRUE(obj.isObject());
    ASSERT_TRUE(obj.hasKey("old"));
    obj = obj.get("old");
    ASSERT_TRUE(obj.isArray());
    size_t index = 0;
    ASSERT_EQ(expected.size(), obj.length());
    for (auto const& it : VPackArrayIterator(obj)) {
      ASSERT_TRUE(it.isString());
      EXPECT_EQ(it.copyString(), expected[index++]);
    }
  }
};

TEST_F(FailedLeaderTest, creating_a_job_should_create_a_job_in_todo) {
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
    EXPECT_EQ(job.get("type").copyString(), "failedLeader");
    EXPECT_EQ(std::string(job.get("database").typeName()), "string");
    EXPECT_EQ(job.get("database").copyString(), DATABASE);
    EXPECT_EQ(std::string(job.get("collection").typeName()), "string");
    EXPECT_EQ(job.get("collection").copyString(), COLLECTION);
    EXPECT_EQ(std::string(job.get("shard").typeName()), "string");
    EXPECT_EQ(job.get("shard").copyString(), SHARD);
    EXPECT_EQ(std::string(job.get("fromServer").typeName()), "string");
    EXPECT_EQ(job.get("fromServer").copyString(), SHARD_LEADER);
    EXPECT_EQ(std::string(job.get("jobId").typeName()), "string");
    EXPECT_EQ(std::string(job.get("timeCreated").typeName()), "string");

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
      for (auto it : VPackObjectIterator(s)) {
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
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, distributeshardslike_should_immediately_fail) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
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
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, if_leader_is_healthy_we_fail_the_job) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto it : VPackObjectIterator(s)) {
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
      for (auto it : VPackObjectIterator(s)) {
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
      MoveShard(baseStructure("arango"), &moveShardAgent, "2", "strunz", DATABASE,
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
    EXPECT_EQ(std::string(q->slice().typeName()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(std::string(q->slice()[0].typeName()), "array");
    EXPECT_EQ(std::string(q->slice()[0][0].typeName()), "object");
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
      for (auto it : VPackObjectIterator(s)) {
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
    EXPECT_EQ(std::string(q->slice().typeName()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(std::string(q->slice()[0].typeName()), "array");
    EXPECT_EQ(q->slice()[0].length(), 2);  // preconditions!
    EXPECT_EQ(std::string(q->slice()[0][0].typeName()), "object");
    EXPECT_EQ(std::string(q->slice()[0][1].typeName()), "object");

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
    EXPECT_EQ(std::string(job.get("toServer").typeName()), "string");
    EXPECT_EQ(job.get("toServer").copyString(), SHARD_FOLLOWER2);
    EXPECT_EQ(std::string(job.get("timeStarted").typeName()), "string");

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
      for (auto it : VPackObjectIterator(s)) {
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

  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::PENDING, jobId);
  failedLeader.run(aborts);
}

TEST_F(FailedLeaderTest, if_new_leader_doesnt_catch_up_we_wait) {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto it : VPackObjectIterator(s)) {
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
      for (auto it : VPackObjectIterator(s)) {
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
    try {
      EXPECT_EQ(std::string(q->slice().typeName()), "array");
      EXPECT_EQ(q->slice().length(), 1);
      EXPECT_EQ(std::string(q->slice()[0].typeName()), "array");
      EXPECT_EQ(q->slice()[0].length(), 1);  // we always simply override! no preconditions...
      EXPECT_EQ(std::string(q->slice()[0][0].typeName()), "object");

      auto writes = q->slice()[0][0];
      EXPECT_TRUE(std::string(writes.get("/arango/Target/Pending/1").get("op").typeName()) ==
                  "string");
      EXPECT_TRUE(std::string(writes.get("/arango/Target/ToDo/1").get("op").typeName()) ==
                  "string");
      EXPECT_TRUE(writes.get("/arango/Target/Pending/1").get("op").copyString() ==
                  "delete");
      EXPECT_TRUE(writes.get("/arango/Supervision/Shards/s99").get("op").copyString() ==
                  "delete");
      EXPECT_TRUE(std::string(writes.get("/arango/Target/Failed/1").typeName()) ==
                  "object");

      std::unordered_set<std::string> expectedKeys{
        "/arango/Target/ToDo/1",
        "/arango/Target/Pending/1",
        "/arango/Target/Failed/1",
        "/arango/Supervision/Shards/s99",
      };

      EXPECT_EQ(getKeySet(writes), expectedKeys);

      return fakeWriteResult;
    } catch(std::exception const& e) {
      EXPECT_TRUE(false);
      throw e;
    }
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
      for (auto it : VPackObjectIterator(s)) {
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
    EXPECT_EQ(std::string(q->slice().typeName()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(std::string(q->slice()[0].typeName()), "array");
    EXPECT_EQ(q->slice()[0].length(), 1);  // we always simply override! no preconditions...
    EXPECT_EQ(std::string(q->slice()[0][0].typeName()), "object");

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
  ShardInfo si{DATABASE, COLLECTION, SHARD};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    // follower2 in sync
                    .setFollowers(si, {SHARD_LEADER, SHARD_FOLLOWER2})
                    // but not part of the plan => will drop collection on next occasion
                    .setPlannedServers(si, {SHARD_LEADER, SHARD_FOLLOWER1})
                    .setJobInTodo(jobId)
                    .createNode();

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

TEST_F(FailedLeaderTest, failedleader_must_not_take_follower_into_account_that_is_not_in_plan) {
  std::string jobId = "1";
  ShardInfo si{DATABASE, COLLECTION, SHARD};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    // Follower 1 planned
                    .setPlannedServers(si, {SHARD_LEADER, SHARD_FOLLOWER1})
                    // Follower 2 in followers
                    .setFollowers(si, {SHARD_LEADER, SHARD_FOLLOWER2})
                    .setJobInTodo(jobId)
                    .createNode();

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

TEST_F(FailedLeaderTest, failedleader_must_not_take_a_candidate_into_account_that_is_not_in_plan) {
  std::string jobId = "1";
  ShardInfo si{DATABASE, COLLECTION, SHARD};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    // Follower 1 planned
                    .setPlannedServers(si, {SHARD_LEADER, SHARD_FOLLOWER1})
                    // Follower 2 in candidates
                    .setFailoverCandidates(si, {SHARD_LEADER, SHARD_FOLLOWER2})
                    // Follower 2 in followers
                    .setFollowers(si, {SHARD_LEADER})
                    .setJobInTodo(jobId)
                    .createNode();

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


TEST_F(FailedLeaderTest, failedleader_must_not_take_a_candidate_and_follower_into_account_that_is_not_in_plan) {
  std::string jobId = "1";
  ShardInfo si{DATABASE, COLLECTION, SHARD};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    // Follower 1 planned
                    .setPlannedServers(si, {SHARD_LEADER, SHARD_FOLLOWER1})
                    // Follower 2 in candidates
                    .setFailoverCandidates(si, {SHARD_LEADER, SHARD_FOLLOWER2})
                    // Follower 2 in followers
                    .setFollowers(si, {SHARD_LEADER, SHARD_FOLLOWER2})
                    .setJobInTodo(jobId)
                    .createNode();

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

TEST_F(FailedLeaderTest, failedleader_must_not_readd_servers_not_in_plan) {
  std::string jobId = "1";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  std::vector<std::string> expected = {SHARD_FOLLOWER1, SHARD_LEADER, FREE_SERVER};
  std::vector<std::string> planned = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1, SHARD_FOLLOWER2};
  std::vector<std::string> failovers = {SHARD_LEADER, SHARD_FOLLOWER1, SHARD_FOLLOWER2};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    // Follower 1 planned
                    .setPlannedServers(si, planned)
                    // Follower 2 in candidates
                    .setFailoverCandidates(si, failovers)
                    // Follower 2 in followers
                    .setFollowers(si, followers)
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    AssertIsValidTransaction(q, si, jobId, expected, planned, followers, failovers);
    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  // new server will randomly be selected...so seed the random number generator
  srand(1);
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, failedleader_must_not_add_a_follower_if_none_exists) {
  std::string jobId = "1";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  // We should have 3 servers, but there is no healthy one
  std::vector<std::string> expected = {SHARD_FOLLOWER1, SHARD_LEADER};
  std::vector<std::string> planned = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1, SHARD_FOLLOWER2};
  std::vector<std::string> failovers = {SHARD_LEADER, SHARD_FOLLOWER1, SHARD_FOLLOWER2};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    // Follower 1 planned
                    .setPlannedServers(si, planned)
                    // Follower 2 in candidates
                    .setFailoverCandidates(si, failovers)
                    // Follower 2 in followers
                    .setFollowers(si, followers)
                    .setServerFailed(FREE_SERVER)
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    AssertIsValidTransaction(q, si, jobId, expected, planned, followers, failovers);
    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  // new server will randomly be selected...so seed the random number generator
  srand(1);
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, failedleader_distribute_shard_like_good_case) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};
  // We only have Leader, F1, Free healthy
  std::vector<std::string> expected = {SHARD_FOLLOWER1, SHARD_LEADER, FREE_SERVER};

  std::vector<std::string> planned = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {SHARD_LEADER, SHARD_FOLLOWER1};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFailoverCandidates(si, failovers)
                    .setFollowers(si, followers)
                    .setPlannedServers(distLike1, planned)
                    .setFailoverCandidates(distLike1, failovers)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFailoverCandidates(distLike2, failovers)
                    .setFollowers(distLike2, followers)
                    .setDistributeShardsLike(distLike2, si)
                    .setServerFailed(SHARD_FOLLOWER2) // disable this as randomly picked follower
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Must be a valid transaction for the full group of distribute shards like
    AssertIsValidTransaction(q, si, jobId, expected, planned, followers, failovers);
    AssertIsValidTransaction(q, distLike1, jobId, expected, planned, followers, failovers);
    AssertIsValidTransaction(q, distLike2, jobId, expected, planned, followers, failovers);
    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  // new server will randomly be selected...so seed the random number generator
  srand(1);
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, failedleader_distribute_shard_like_pick_common_candidate_follower_not_sync) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};
  std::vector<std::string> expected = {SHARD_FOLLOWER1, SHARD_LEADER, FREE_SERVER};

  std::vector<std::string> planned = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {SHARD_LEADER, SHARD_FOLLOWER1};
  // DistShardLike 2 has not confirmed in-sync follower, but has failover candidate.
  // Can be picked
  std::vector<std::string> reducedFollowers = {SHARD_LEADER};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFailoverCandidates(si, failovers)
                    .setFollowers(si, followers)
                    .setPlannedServers(distLike1, planned)
                    .setFailoverCandidates(distLike1, failovers)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFailoverCandidates(distLike2, failovers)
                    .setFollowers(distLike2, reducedFollowers)
                    .setDistributeShardsLike(distLike2, si)
                    .setServerFailed(SHARD_FOLLOWER2) // disable this as randomly picked follower
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Must be a valid transaction for the full group of distribute shards like
    AssertIsValidTransaction(q, si, jobId, expected, planned, followers, failovers);
    AssertIsValidTransaction(q, distLike1, jobId, expected, planned, followers, failovers);
    AssertIsValidTransaction(q, distLike2, jobId, expected, planned, reducedFollowers, failovers);
    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  // new server will randomly be selected...so seed the random number generator
  srand(1);
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, failedleader_distribute_shard_like_pick_common_candidate_leader_not_sync) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};

  std::vector<std::string> expected = {SHARD_FOLLOWER1, SHARD_LEADER, FREE_SERVER};

  std::vector<std::string> planned = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {SHARD_LEADER, SHARD_FOLLOWER1};
  // Leader has not confirmed in-sync follower, but has failover candidate.
  // Can be picked
  std::vector<std::string> reducedFollowers = {SHARD_LEADER};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFailoverCandidates(si, failovers)
                    .setFollowers(si, reducedFollowers)
                    .setPlannedServers(distLike1, planned)
                    .setFailoverCandidates(distLike1, failovers)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFailoverCandidates(distLike2, failovers)
                    .setFollowers(distLike2, followers)
                    .setDistributeShardsLike(distLike2, si)
                    .setServerFailed(SHARD_FOLLOWER2) // disable this as randomly picked follower
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Must be a valid transaction for the full group of distribute shards like
    AssertIsValidTransaction(q, si, jobId, expected, planned, reducedFollowers, failovers);
    AssertIsValidTransaction(q, distLike1, jobId, expected, planned, followers, failovers);
    AssertIsValidTransaction(q, distLike2, jobId, expected, planned, followers, failovers);
    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  // new server will randomly be selected...so seed the random number generator
  srand(1);
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, failedleader_distribute_shard_like_no_common_candidate_follower_out_of_sync) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};

  std::vector<std::string> planned = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {SHARD_LEADER, SHARD_FOLLOWER1};
  // Follower2 has not enough candidates, we cannot transact
  std::vector<std::string> reducedFailovers = {SHARD_LEADER};
  std::vector<std::string> reducedFollowers = {SHARD_LEADER};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFailoverCandidates(si, failovers)
                    .setFollowers(si, followers)
                    .setPlannedServers(distLike1, planned)
                    .setFailoverCandidates(distLike1, failovers)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFailoverCandidates(distLike2, reducedFailovers)
                    .setFollowers(distLike2, reducedFollowers)
                    .setDistributeShardsLike(distLike2, si)
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Impossible to transact
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

TEST_F(FailedLeaderTest, failedleader_distribute_shard_like_no_common_candidate_leader_out_of_sync) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};

  std::vector<std::string> planned = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {SHARD_LEADER, SHARD_FOLLOWER1};
  // Follower2 has not enough candidates, we cannot transact
  std::vector<std::string> reducedFailovers = {SHARD_LEADER};
  std::vector<std::string> reducedFollowers = {SHARD_LEADER};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFailoverCandidates(si, reducedFailovers)
                    .setFollowers(si, reducedFollowers)
                    .setPlannedServers(distLike1, planned)
                    .setFailoverCandidates(distLike1, failovers)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFailoverCandidates(distLike2, failovers)
                    .setFollowers(distLike2, followers)
                    .setDistributeShardsLike(distLike2, si)
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Impossible to transact
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


// Section no one has set failover candidates

TEST_F(FailedLeaderTest, failedleader_distribute_shard_like_good_case_no_candidates) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};
  std::vector<std::string> expected = {SHARD_FOLLOWER1, SHARD_LEADER, FREE_SERVER};

  std::vector<std::string> planned = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFollowers(si, followers)
                    .setPlannedServers(distLike1, planned)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFollowers(distLike2, followers)
                    .setDistributeShardsLike(distLike2, si)
                    .setServerFailed(SHARD_FOLLOWER2) // disable this as randomly picked follower
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Must be a valid transaction for the full group of distribute shards like
    AssertIsValidTransaction(q, si, jobId, expected, planned, followers, failovers);
    AssertIsValidTransaction(q, distLike1, jobId, expected, planned, followers, failovers);
    AssertIsValidTransaction(q, distLike2, jobId, expected, planned, followers, failovers);
    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  // new server will randomly be selected...so seed the random number generator
  srand(1);
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, failedleader_distribute_shard_like_no_common_candidate_follower_out_of_sync_no_candidates) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};

  std::vector<std::string> planned = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {};
  // Follower2 has not enough followers, we cannot transact
  std::vector<std::string> reducedFollowers = {SHARD_LEADER};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFollowers(si, followers)
                    .setPlannedServers(distLike1, planned)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFollowers(distLike2, reducedFollowers)
                    .setDistributeShardsLike(distLike2, si)
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Impossible to transact
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

TEST_F(FailedLeaderTest, failedleader_distribute_shard_like_no_common_candidate_leader_out_of_sync_no_candidates) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};

  std::vector<std::string> planned = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {};
  // Leader has not enough followers, we cannot transact
  std::vector<std::string> reducedFollowers = {SHARD_LEADER};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFollowers(si, reducedFollowers)
                    .setPlannedServers(distLike1, planned)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFollowers(distLike2, followers)
                    .setDistributeShardsLike(distLike2, si)
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Impossible to transact
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

// Section one follower has not set failoverCandidates


TEST_F(FailedLeaderTest, failedleader_distribute_shard_like_good_case_one_has_no_candidates) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};
  std::vector<std::string> expected = {SHARD_FOLLOWER1, SHARD_LEADER, FREE_SERVER};

  std::vector<std::string> planned = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {SHARD_LEADER, SHARD_FOLLOWER1};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFailoverCandidates(si, failovers)
                    .setFollowers(si, followers)
                    .setPlannedServers(distLike1, planned)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFailoverCandidates(distLike2, failovers)
                    .setFollowers(distLike2, followers)
                    .setDistributeShardsLike(distLike2, si)
                    .setServerFailed(SHARD_FOLLOWER2) // disable this as randomly picked follower
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Must be a valid transaction for the full group of distribute shards like
    AssertIsValidTransaction(q, si, jobId, expected, planned, followers, failovers);
    AssertIsValidTransaction(q, distLike1, jobId, expected, planned, followers, {});
    AssertIsValidTransaction(q, distLike2, jobId, expected, planned, followers, failovers);
    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  // new server will randomly be selected...so seed the random number generator
  srand(1);
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, failedleader_distribute_shard_like_pick_common_candidate_follower_not_sync_one_has_no_candidates) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};
  std::vector<std::string> expected = {SHARD_FOLLOWER1, SHARD_LEADER, FREE_SERVER};

  std::vector<std::string> planned = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {SHARD_LEADER, SHARD_FOLLOWER1};
  // DistShardLike 2 has not confirmed in-sync follower, but has failover candidate.
  // Can be picked
  std::vector<std::string> reducedFollowers = {SHARD_LEADER};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFailoverCandidates(si, failovers)
                    .setFollowers(si, followers)
                    .setPlannedServers(distLike1, planned)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFailoverCandidates(distLike2, failovers)
                    .setFollowers(distLike2, reducedFollowers)
                    .setDistributeShardsLike(distLike2, si)
                    .setServerFailed(SHARD_FOLLOWER2) // disable this as randomly picked follower
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Must be a valid transaction for the full group of distribute shards like
    AssertIsValidTransaction(q, si, jobId, expected, planned, followers, failovers);
    AssertIsValidTransaction(q, distLike1, jobId, expected, planned, followers, {});
    AssertIsValidTransaction(q, distLike2, jobId, expected, planned, reducedFollowers, failovers);
    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  // new server will randomly be selected...so seed the random number generator
  srand(1);
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, failedleader_distribute_shard_like_pick_common_candidate_leader_not_sync_one_has_no_candidates) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};
  std::vector<std::string> expected = {SHARD_FOLLOWER1, SHARD_LEADER, FREE_SERVER};

  std::vector<std::string> planned = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {SHARD_LEADER, SHARD_FOLLOWER1};
  // Leader has not confirmed in-sync follower, but has failover candidate.
  // Can be picked
  std::vector<std::string> reducedFollowers = {SHARD_LEADER};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFailoverCandidates(si, failovers)
                    .setFollowers(si, reducedFollowers)
                    .setPlannedServers(distLike1, planned)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFailoverCandidates(distLike2, failovers)
                    .setFollowers(distLike2, followers)
                    .setDistributeShardsLike(distLike2, si)
                    .setServerFailed(SHARD_FOLLOWER2) // disable this as randomly picked follower
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Must be a valid transaction for the full group of distribute shards like
    AssertIsValidTransaction(q, si, jobId, expected, planned, reducedFollowers, failovers);
    AssertIsValidTransaction(q, distLike1, jobId, expected, planned, followers, {});
    AssertIsValidTransaction(q, distLike2, jobId, expected, planned, followers, failovers);
    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  // new server will randomly be selected...so seed the random number generator
  srand(1);
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, failedleader_distribute_shard_like_no_common_candidate_follower_out_of_sync_one_has_no_candidates) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};

  std::vector<std::string> planned = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {SHARD_LEADER, SHARD_FOLLOWER1};
  // Follower2 has not enough candidates, we cannot transact
  std::vector<std::string> reducedFailovers = {SHARD_LEADER};
  std::vector<std::string> reducedFollowers = {SHARD_LEADER};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFailoverCandidates(si, failovers)
                    .setFollowers(si, followers)
                    .setPlannedServers(distLike1, planned)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFailoverCandidates(distLike2, reducedFailovers)
                    .setFollowers(distLike2, reducedFollowers)
                    .setDistributeShardsLike(distLike2, si)
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Impossible to transact
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

TEST_F(FailedLeaderTest, failedleader_distribute_shard_like_no_common_candidate_leader_out_of_sync_one_has_no_candidates) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};

  std::vector<std::string> planned = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {SHARD_LEADER, SHARD_FOLLOWER1};
  // Follower2 has not enough candidates, we cannot transact
  std::vector<std::string> reducedFailovers = {SHARD_LEADER};
  std::vector<std::string> reducedFollowers = {SHARD_LEADER};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFailoverCandidates(si, reducedFailovers)
                    .setFollowers(si, reducedFollowers)
                    .setPlannedServers(distLike1, planned)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFailoverCandidates(distLike2, failovers)
                    .setFollowers(distLike2, followers)
                    .setDistributeShardsLike(distLike2, si)
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Impossible to transact
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

// Section Resigned Leader notation


TEST_F(FailedLeaderTest, failedleader_distribute_shards_like_resigned_leader_no_current_reports) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};
  // Important here: SHARD_LEADER is NOT resigned in new plan!
  std::vector<std::string> expected = {SHARD_FOLLOWER1, SHARD_LEADER, FREE_SERVER};

  // We have planned to resign the leader, but it has not yet confirmed anywhere
  std::vector<std::string> planned = {resigned(SHARD_LEADER), SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {SHARD_LEADER, SHARD_FOLLOWER1};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFailoverCandidates(si, failovers)
                    .setFollowers(si, followers)
                    .setPlannedServers(distLike1, planned)
                    .setFailoverCandidates(distLike1, failovers)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFailoverCandidates(distLike2, failovers)
                    .setServerFailed(SHARD_FOLLOWER2) // disable this as randomly picked follower
                    .setFollowers(distLike2, followers)
                    .setDistributeShardsLike(distLike2, si)
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Must be a valid transaction for the full group of distribute shards like
    AssertIsValidTransaction(q, si, jobId, expected, planned, followers, failovers);
    AssertIsValidTransaction(q, distLike1, jobId, expected, planned, followers, failovers);
    AssertIsValidTransaction(q, distLike2, jobId, expected, planned, followers, failovers);
    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  // new server will randomly be selected...so seed the random number generator
  srand(1);
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, failedleader_distribute_shards_like_resigned_leader_all_repoted_in_current) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};
  // Important here: SHARD_LEADER is NOT resigned in new plan!
  std::vector<std::string> expected = {SHARD_FOLLOWER1, SHARD_LEADER, FREE_SERVER};

  // We have planned to resign the leader, it was confirmed everywhere.
  std::vector<std::string> planned = {resigned(SHARD_LEADER), SHARD_FOLLOWER1};
  std::vector<std::string> followers = {resigned(SHARD_LEADER), SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {SHARD_LEADER, SHARD_FOLLOWER1};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFailoverCandidates(si, failovers)
                    .setFollowers(si, followers)
                    .setPlannedServers(distLike1, planned)
                    .setFailoverCandidates(distLike1, failovers)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFailoverCandidates(distLike2, failovers)
                    .setFollowers(distLike2, followers)
                    .setDistributeShardsLike(distLike2, si)
                    .setServerFailed(SHARD_FOLLOWER2) // disable this as randomly picked follower
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Must be a valid transaction for the full group of distribute shards like
    AssertIsValidTransaction(q, si, jobId, expected, planned, followers, failovers);
    AssertIsValidTransaction(q, distLike1, jobId, expected, planned, followers, failovers);
    AssertIsValidTransaction(q, distLike2, jobId, expected, planned, followers, failovers);
    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  // new server will randomly be selected...so seed the random number generator
  srand(1);
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}


TEST_F(FailedLeaderTest, failedleader_distribute_shards_like_resigned_leader_leader_repoted_in_current) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};
  // Important here: SHARD_LEADER is NOT resigned in new plan!
  std::vector<std::string> expected = {SHARD_FOLLOWER1, SHARD_LEADER, FREE_SERVER};

  // We have planned to resign the leader, it was confirmed everywhere.
  std::vector<std::string> planned = {resigned(SHARD_LEADER), SHARD_FOLLOWER1};
  std::vector<std::string> resignedFollowers = {resigned(SHARD_LEADER), SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {SHARD_LEADER, SHARD_FOLLOWER1};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFailoverCandidates(si, failovers)
                    .setFollowers(si, resignedFollowers)
                    .setPlannedServers(distLike1, planned)
                    .setFailoverCandidates(distLike1, failovers)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFailoverCandidates(distLike2, failovers)
                    .setFollowers(distLike2, followers)
                    .setDistributeShardsLike(distLike2, si)
                    .setServerFailed(SHARD_FOLLOWER2) // disable this as randomly picked follower
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Must be a valid transaction for the full group of distribute shards like
    AssertIsValidTransaction(q, si, jobId, expected, planned, resignedFollowers, failovers);
    AssertIsValidTransaction(q, distLike1, jobId, expected, planned, followers, failovers);
    AssertIsValidTransaction(q, distLike2, jobId, expected, planned, followers, failovers);
    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();

  // new server will randomly be selected...so seed the random number generator
  srand(1);
  auto failedLeader = FailedLeader(agency("arango"), &agent, JOB_STATUS::TODO, jobId);
  failedLeader.start(aborts);
}

TEST_F(FailedLeaderTest, failedleader_distribute_shards_like_resigned_leader_follower_repoted_in_current) {
  std::string jobId = "1";

  std::string col1 = "shardLike1";
  std::string shard1 = "s1001";
  std::string col2 = "shardLike2";
  std::string shard2 = "s2001";
  ShardInfo si{DATABASE, COLLECTION, SHARD};
  ShardInfo distLike1{DATABASE, col1, shard1, true};
  ShardInfo distLike2{DATABASE, col2, shard2, true};
  // Important here: SHARD_LEADER is NOT resigned in new plan!
  std::vector<std::string> expected = {SHARD_FOLLOWER1, SHARD_LEADER, FREE_SERVER};

  // We have planned to resign the leader, it was confirmed everywhere.
  std::vector<std::string> planned = {resigned(SHARD_LEADER), SHARD_FOLLOWER1};
  std::vector<std::string> resignedFollowers = {resigned(SHARD_LEADER), SHARD_FOLLOWER1};
  std::vector<std::string> followers = {SHARD_LEADER, SHARD_FOLLOWER1};
  std::vector<std::string> failovers = {SHARD_LEADER, SHARD_FOLLOWER1};

  Node agency = AgencyBuilder(baseStructure.toBuilder())
                    .setPlannedServers(si, planned)
                    .setFailoverCandidates(si, failovers)
                    .setFollowers(si, followers)
                    .setPlannedServers(distLike1, planned)
                    .setFailoverCandidates(distLike1, failovers)
                    .setFollowers(distLike1, followers)
                    .setDistributeShardsLike(distLike1, si)
                    .setPlannedServers(distLike2, planned)
                    .setFailoverCandidates(distLike2, failovers)
                    .setFollowers(distLike2, resignedFollowers)
                    .setDistributeShardsLike(distLike2, si)
                    .setServerFailed(SHARD_FOLLOWER2) // disable this as randomly picked follower
                    .setJobInTodo(jobId)
                    .createNode();

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    // Must be a valid transaction for the full group of distribute shards like
    AssertIsValidTransaction(q, si, jobId, expected, planned, followers, failovers);
    AssertIsValidTransaction(q, distLike1, jobId, expected, planned, followers, failovers);
    AssertIsValidTransaction(q, distLike2, jobId, expected, planned, resignedFollowers, failovers);
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
