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
#include "catch.hpp"
#include "fakeit.hpp"

#include "Agency/AddFollower.h"
#include "Agency/FailedLeader.h"
#include "Agency/MoveShard.h"
#include "Agency/AgentInterface.h"
#include "Agency/Node.h"
#include "lib/Basics/StringUtils.h"
#include "lib/Random/RandomGenerator.h"

#include <iostream>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

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

const char *agency =
#include "FailedLeaderTest.json"
;

Node createNodeFromBuilder(Builder const& builder) {

  Builder opBuilder;
  { VPackObjectBuilder a(&opBuilder);
    opBuilder.add("new", builder.slice()); }
  
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

Node createRootNode() {
  return createNode(agency);
}

char const* todo = R"=({
  "creator":"1", "type":"failedLeader", "database":"database",
  "collection":"collection", "shard":"s99", "fromServer":"leader",
  "jobId":"1", "timeCreated":"2017-01-01 00:00:00"
  })=";

typedef std::function<std::unique_ptr<Builder>(
  Slice const&, std::string const&)>TestStructureType;

TEST_CASE("FailedLeader", "[agency][supervision]") {
  RandomGenerator::seed(3);

  auto baseStructure = createRootNode();

  Builder builder;
  baseStructure.toBuilder(builder);
    
  write_ret_t fakeWriteResult {true, "", std::vector<bool> {true}, std::vector<index_t> {1}};
  auto transBuilder = std::make_shared<Builder>();
  { VPackArrayBuilder a(transBuilder.get());
    transBuilder->add(VPackValue((uint64_t)1)); }

  trans_ret_t fakeTransResult {true, "", 1, 0, transBuilder};
  
SECTION("creating a job should create a job in todo") {
  Mock<AgentInterface> mockAgent;

  std::string jobId = "1";
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, bool d) -> write_ret_t {
    INFO(q->slice().toJson());
    auto expectedJobKey = "/arango/Target/ToDo/" + jobId;
    REQUIRE(std::string(q->slice().typeName()) == "array" );
    REQUIRE(q->slice().length() == 1);
    REQUIRE(std::string(q->slice()[0].typeName()) == "array");
    REQUIRE(q->slice()[0].length() == 1); // we always simply override! no preconditions...
    REQUIRE(std::string(q->slice()[0][0].typeName()) == "object");
    REQUIRE(q->slice()[0][0].length() == 1); // should ONLY do an entry in todo
    REQUIRE(std::string(q->slice()[0][0].get(expectedJobKey).typeName()) == "object");

    auto job = q->slice()[0][0].get(expectedJobKey);
    REQUIRE(std::string(job.get("creator").typeName()) == "string");
    REQUIRE(std::string(job.get("type").typeName()) == "string");
    CHECK(job.get("type").copyString() == "failedLeader");
    REQUIRE(std::string(job.get("database").typeName()) == "string");
    CHECK(job.get("database").copyString() == DATABASE);
    REQUIRE(std::string(job.get("collection").typeName()) == "string");
    CHECK(job.get("collection").copyString() == COLLECTION);
    REQUIRE(std::string(job.get("shard").typeName()) == "string");
    CHECK(job.get("shard").copyString() == SHARD);
    REQUIRE(std::string(job.get("fromServer").typeName()) == "string");
    CHECK(job.get("fromServer").copyString() == SHARD_LEADER);
    CHECK(std::string(job.get("jobId").typeName()) == "string");
    CHECK(std::string(job.get("timeCreated").typeName()) == "string");

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface &agent = mockAgent.get();

  auto failedLeader = FailedLeader(
    baseStructure,
    &agent,
    jobId,
    "unittest",
    DATABASE,
    COLLECTION,
    SHARD,
    SHARD_LEADER
  );
  failedLeader.create();
}

SECTION("if we want to start and the collection went missing from plan (our truth) the job should just finish") {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {

    std::unique_ptr<Builder> builder;
    if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION) {
      return builder;
    }
    builder.reset(new Builder());
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
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
  REQUIRE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, bool d) -> write_ret_t {
    INFO(q->slice().toJson());
    REQUIRE(std::string(q->slice().typeName()) == "array" );
    REQUIRE(q->slice().length() == 1);
    REQUIRE(std::string(q->slice()[0].typeName()) == "array");
    REQUIRE(q->slice()[0].length() == 1); // we always simply override! no preconditions...
    REQUIRE(std::string(q->slice()[0][0].typeName()) == "object");

    auto writes = q->slice()[0][0];
    REQUIRE(std::string(writes.get("/arango/Target/ToDo/1").typeName()) == "object");
    REQUIRE(std::string(writes.get("/arango/Target/ToDo/1").get("op").typeName()) == "string");
    CHECK(writes.get("/arango/Target/ToDo/1").get("op").copyString() == "delete");
    CHECK(std::string(writes.get("/arango/Target/Finished/1").typeName()) == "object");
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface &agent = mockAgent.get();
  auto failedLeader = FailedLeader(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedLeader.start();
}

SECTION("if we are supposed to fail a distributeShardsLike job we immediately fail because this should be done by a job running on the master shard") {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
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
  REQUIRE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, bool d) -> write_ret_t {
    INFO(q->slice().toJson());
    REQUIRE(std::string(q->slice().typeName()) == "array" );
    REQUIRE(q->slice().length() == 1);
    REQUIRE(std::string(q->slice()[0].typeName()) == "array");
    REQUIRE(q->slice()[0].length() == 1); // we always simply override! no preconditions...
    REQUIRE(std::string(q->slice()[0][0].typeName()) == "object");

    auto writes = q->slice()[0][0];
    REQUIRE(std::string(writes.get("/arango/Target/ToDo/1").typeName()) == "object");
    REQUIRE(std::string(writes.get("/arango/Target/ToDo/1").get("op").typeName()) == "string");
    CHECK(writes.get("/arango/Target/ToDo/1").get("op").copyString() == "delete");
    CHECK(std::string(writes.get("/arango/Target/Failed/1").typeName()) == "object");
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface &agent = mockAgent.get();
  auto failedLeader = FailedLeader(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedLeader.start();
}

SECTION("if the leader is healthy again we fail the job") {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
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
  REQUIRE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).Do([&](query_t const& q) -> trans_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    auto preconditions = q->slice()[0][1];
    REQUIRE(preconditions.get("/arango/Supervision/Health/" + SHARD_LEADER + "/Status").get("old").copyString() == "FAILED");

    char const* json = R"=([{"arango":{"Supervision":{"Health":{"leader":{"Status":"GOOD"}}}}}])=";
    auto transBuilder = std::make_shared<Builder>(createBuilder(json));
    return trans_ret_t(true, "", 0, 1, transBuilder);
  });
  When(Method(mockAgent, write)).Do([&](query_t const& q, bool d) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    auto writes = q->slice()[0][0]; \
    REQUIRE(std::string(writes.get("/arango/Target/ToDo/1").get("op").typeName()) == "string"); \
    CHECK(std::string(writes.get("/arango/Target/Failed/1").typeName()) == "object");

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  AgentInterface &agent = mockAgent.get();
  auto failedLeader = FailedLeader(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedLeader.start();
}

SECTION("the job must not be started if there is no server that is in sync for every shard") {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Target/ToDo") {
        builder->add("1", createBuilder(todo).slice());
      }
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        VPackArrayBuilder a(builder.get());
        builder->add(VPackValue(SHARD_LEADER));
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  INFO(builder->toJson());
  Node agency = createNodeFromBuilder(*builder);
  
  // nothing should happen
  Mock<AgentInterface> mockAgent;
  AgentInterface &agent = mockAgent.get();
  auto failedLeader = FailedLeader(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedLeader.start();
}

SECTION("the job must not be started if there if one of the linked shards (distributeShardsLike) is not in sync") {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }

      if (path == "/arango/Current/Collections/" + DATABASE) {
        // we fake that follower2 is in sync
        char const* json1 =
          R"=({"s100":{"servers":["leader","follower2"]}})=";
        builder->add("linkedcollection1", createBuilder(json1).slice());
        // for the other shard there is only follower1 in sync
        char const* json2 =
          R"=({"s101":{"servers":["leader","follower1"]}})=";
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
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };
  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  INFO("Agency: " << builder->toJson());
  Node agency = createNodeFromBuilder(*builder);

  // nothing should happen
  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    REQUIRE(false);
    return trans_ret_t();
  });
  AgentInterface &agent = mockAgent.get();
  auto failedLeader = FailedLeader(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedLeader.start();
}

SECTION("abort any moveShard job blocking the shard and start") {
  Mock<AgentInterface> moveShardMockAgent;

  Builder moveShardBuilder;
  When(Method(moveShardMockAgent, write)).Do([&](query_t const& q, bool d) -> write_ret_t {
    INFO("WriteTransaction(create): " << q->slice().toJson());
    REQUIRE(std::string(q->slice().typeName()) == "array" );
    REQUIRE(q->slice().length() == 1);
    REQUIRE(std::string(q->slice()[0].typeName()) == "array");
    REQUIRE(q->slice()[0].length() > 0); // preconditions!
    REQUIRE(std::string(q->slice()[0][0].typeName()) == "object");
    REQUIRE(std::string(q->slice()[0][0].get("/arango/Target/ToDo/2").typeName()) == "object");
    moveShardBuilder.add(q->slice()[0][0].get("/arango/Target/ToDo/2"));

    return fakeWriteResult;
  });
  When(Method(moveShardMockAgent, waitFor)).Return();
  AgentInterface &moveShardAgent = moveShardMockAgent.get();
  auto moveShard = MoveShard(
    baseStructure("arango"), &moveShardAgent, "2", "strunz", DATABASE,
    COLLECTION, SHARD, SHARD_LEADER, FREE_SERVER, true);
  moveShard.create();

  std::string jobId = "1";
  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder(new Builder());
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
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
      if (path == "/arango/Current/Collections/" + DATABASE + "/" +
          COLLECTION + "/" + SHARD + "/servers") {
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
  REQUIRE(builder);
  INFO("Teststructure: " << builder->toJson());
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q, bool d) -> write_ret_t {
    // check that moveshard is being moved to failed
    INFO("WriteTransaction: " << q->slice().toJson());
    REQUIRE(std::string(q->slice().typeName()) == "array");
    REQUIRE(q->slice().length() == 1);
    REQUIRE(std::string(q->slice()[0].typeName()) == "array");
    REQUIRE(std::string(q->slice()[0][0].typeName()) == "object");
    REQUIRE(std::string(q->slice()[0][0].get("/arango/Target/Failed/2").typeName()) == "object");
    return fakeWriteResult;
  });

  When(Method(mockAgent, transact)).Do([&](query_t const& q) -> trans_ret_t {
    // check that the job is now pending
    INFO("Transaction: " << q->slice().toJson());
    auto writes = q->slice()[0][0];
    REQUIRE(std::string(writes.get("/arango/Target/Pending/1").typeName()) == "object");
    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface &agent = mockAgent.get();
  auto failedLeader = FailedLeader(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedLeader.start();
  Verify(Method(mockAgent, transact));
  Verify(Method(mockAgent, write));
}

SECTION("if everything is fine than the job should be written to pending, adding the toServer") {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder(new Builder());
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Target/ToDo") {
        builder->add("1", createBuilder(todo).slice());
      }
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
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
  REQUIRE(builder);
  INFO("Teststructure: " << builder->toJson());
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    REQUIRE(std::string(q->slice().typeName()) == "array" );
    REQUIRE(q->slice().length() == 1);
    REQUIRE(std::string(q->slice()[0].typeName()) == "array");
    REQUIRE(q->slice()[0].length() == 2); // preconditions!
    REQUIRE(std::string(q->slice()[0][0].typeName()) == "object");
    REQUIRE(std::string(q->slice()[0][1].typeName()) == "object");

    auto writes = q->slice()[0][0];
    REQUIRE(std::string(writes.get("/arango/Target/ToDo/1").typeName()) == "object");
    REQUIRE(std::string(writes.get("/arango/Target/ToDo/1").get("op").typeName()) == "string");
    CHECK(writes.get("/arango/Target/ToDo/1").get("op").copyString() == "delete");
    CHECK(std::string(writes.get("/arango/Target/ToDo/1").typeName()) == "object");
    REQUIRE(std::string(writes.get("/arango/Target/Pending/1").typeName()) == "object");

    auto job = writes.get("/arango/Target/Pending/1");
    REQUIRE(std::string(job.get("toServer").typeName()) == "string");
    CHECK(job.get("toServer").copyString() == SHARD_FOLLOWER2);
    CHECK(std::string(job.get("timeStarted").typeName()) == "string");

    REQUIRE(std::string(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).typeName()) == "array");
    REQUIRE(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).length() == 4);
    REQUIRE(std::string(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[0].typeName()) == "string");
    REQUIRE(std::string(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[1].typeName()) == "string");
    REQUIRE(std::string(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[2].typeName()) == "string");
    REQUIRE(std::string(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[3].typeName()) == "string");
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[0].copyString() == SHARD_FOLLOWER2);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[1].copyString() == SHARD_LEADER);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[2].copyString() == SHARD_FOLLOWER1);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[3].copyString().compare(0,4,FREE_SERVER) == 0);

    auto preconditions = q->slice()[0][1];
    REQUIRE(std::string(preconditions.get("/arango/Supervision/Shards/" + SHARD).typeName()) == "object");
    REQUIRE(std::string(preconditions.get("/arango/Supervision/Shards/" + SHARD).get("oldEmpty").typeName()) == "bool");
    CHECK(preconditions.get("/arango/Supervision/Shards/" + SHARD).get("oldEmpty").getBool() == true);
    CHECK(preconditions.get("/arango/Supervision/Health/" + SHARD_LEADER + "/Status").get("old").copyString() == "FAILED");
    CHECK(preconditions.get("/arango/Supervision/Health/" + SHARD_FOLLOWER2 + "/Status").get("old").copyString() == "GOOD");

    REQUIRE(std::string(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).typeName()) == "object");
    REQUIRE(std::string(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old").typeName()) == "array");
    REQUIRE(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old").length() == 3);
    REQUIRE(std::string(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[0].typeName()) == "string");
    REQUIRE(std::string(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[1].typeName()) == "string");
    REQUIRE(std::string(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[2].typeName()) == "string");
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[0].copyString() == SHARD_LEADER);
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[1].copyString() == SHARD_FOLLOWER1);
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[2].copyString() == SHARD_FOLLOWER2);

    auto result = std::make_shared<Builder>();
    result->openArray();
    result->add(VPackValue((uint64_t)1));
    result->close();
    return trans_ret_t(true, "1", 1, 0, result);
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface &agent = mockAgent.get();

  // new server will randomly be selected...so seed the random number generator
  srand(1);
  auto failedLeader = FailedLeader(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedLeader.start();
}

SECTION("if we want are working and our collection went missing from plan the job should just finish") {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder;
    if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION) {
      return builder;
    }
    builder.reset(new Builder());
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Target/Pending") {
        Builder jobBuilder;
        { VPackObjectBuilder b(&jobBuilder);
          jobBuilder.add("creator", VPackValue("1"));
          jobBuilder.add("type", VPackValue("failedLeader"));
          jobBuilder.add("database", VPackValue(DATABASE));
          jobBuilder.add("collection", VPackValue(COLLECTION));
          jobBuilder.add("shard", VPackValue(SHARD));
          jobBuilder.add("fromServer", VPackValue(SHARD_LEADER));
          jobBuilder.add("toServer", VPackValue(SHARD_FOLLOWER1));
          jobBuilder.add("jobId", VPackValue(jobId));
          jobBuilder.add("timeCreated", VPackValue("2017-01-01 00:00:00"));}
        builder->add("1", jobBuilder.slice());
      }
    } else {
      builder->add(s);
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, bool d) -> write_ret_t {
    INFO(q->slice().toJson());
    REQUIRE(std::string(q->slice().typeName()) == "array" );
    REQUIRE(q->slice().length() == 1);
    REQUIRE(std::string(q->slice()[0].typeName()) == "array");
    REQUIRE(q->slice()[0].length() == 1); // we always simply override! no preconditions...
    REQUIRE(std::string(q->slice()[0][0].typeName()) == "object");

    auto writes = q->slice()[0][0];
    REQUIRE(std::string(writes.get("/arango/Target/ToDo/1").typeName()) == "object");
    REQUIRE(std::string(writes.get("/arango/Target/ToDo/1").get("op").typeName()) == "string");
    CHECK(writes.get("/arango/Target/ToDo/1").get("op").copyString() == "delete");
    CHECK(std::string(writes.get("/arango/Target/Finished/1").typeName()) == "object");
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface &agent = mockAgent.get();

  INFO("Agency: " << agency);
  auto failedLeader = FailedLeader(
    agency("arango"),
    &agent,
    JOB_STATUS::PENDING,
    jobId
  );
  failedLeader.run();
}

SECTION("if the newly supposed leader didn't catch up yet we wait") {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Target/Pending") {
        Builder jobBuilder;
        { VPackObjectBuilder b(&jobBuilder);
          jobBuilder.add("creator", VPackValue("1"));
          jobBuilder.add("type", VPackValue("failedLeader"));
          jobBuilder.add("database", VPackValue(DATABASE));
          jobBuilder.add("collection", VPackValue(COLLECTION));
          jobBuilder.add("shard", VPackValue(SHARD));
          jobBuilder.add("fromServer", VPackValue(SHARD_LEADER));
          jobBuilder.add("toServer", VPackValue(SHARD_FOLLOWER1));
          jobBuilder.add("jobId", VPackValue(jobId));
          jobBuilder.add("timeCreated", VPackValue(
                           timepointToString(std::chrono::system_clock::now())));}
        builder->add("1", jobBuilder.slice());
      }
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        VPackArrayBuilder a(builder.get());
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
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
  REQUIRE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  AgentInterface &agent = mockAgent.get();
  INFO("Agency: " << agency);
  auto failedLeader = FailedLeader(
    agency("arango"),
    &agent,
    JOB_STATUS::PENDING,
    jobId
  );
  failedLeader.run();
}

SECTION("in case of a timeout the job should be aborted") {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Target/Pending") {
        Builder jobBuilder;
        { VPackObjectBuilder b(&jobBuilder);
          jobBuilder.add("creator", VPackValue("1"));
          jobBuilder.add("type", VPackValue("failedLeader"));
          jobBuilder.add("database", VPackValue(DATABASE));
          jobBuilder.add("collection", VPackValue(COLLECTION));
          jobBuilder.add("shard", VPackValue(SHARD));
          jobBuilder.add("fromServer", VPackValue(SHARD_LEADER));
          jobBuilder.add("toServer", VPackValue(SHARD_FOLLOWER1));
          jobBuilder.add("jobId", VPackValue(jobId));
          jobBuilder.add("timeCreated", VPackValue("2015-01-03T20:00:00Z"));}
        builder->add("1", jobBuilder.slice());
      }
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        VPackArrayBuilder a(builder.get());
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
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
  REQUIRE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q, bool d) -> write_ret_t {
    INFO(q->slice().toJson());
    REQUIRE(std::string(q->slice().typeName()) == "array" );
    REQUIRE(q->slice().length() == 1);
    REQUIRE(std::string(q->slice()[0].typeName()) == "array");
    REQUIRE(q->slice()[0].length() == 1); // we always simply override! no preconditions...
    REQUIRE(std::string(q->slice()[0][0].typeName()) == "object");

    auto writes = q->slice()[0][0];
    REQUIRE(std::string(writes.get("/arango/Target/Pending/1").typeName()) == "object");
    REQUIRE(std::string(writes.get("/arango/Target/Pending/1").get("op").typeName()) == "string");
    CHECK(writes.get("/arango/Target/Pending/1").get("op").copyString() == "delete");
    CHECK(std::string(writes.get("/arango/Target/Failed/1").typeName()) == "object");
    REQUIRE(std::string(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).typeName()) == "array");
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[0].copyString() == SHARD_LEADER);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[1].copyString() == SHARD_FOLLOWER1);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface &agent = mockAgent.get();
  INFO("Agency: " << agency);
  auto failedLeader = FailedLeader(
    agency("arango"),
    &agent,
    JOB_STATUS::PENDING,
    jobId
  );
  failedLeader.run();
  Verify(Method(mockAgent, write));
}

SECTION("when everything is finished there should be proper cleanup") {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Target/Pending") {
        Builder jobBuilder;
        { VPackObjectBuilder b(&jobBuilder);
          jobBuilder.add("creator", VPackValue("1"));
          jobBuilder.add("type", VPackValue("failedLeader"));
          jobBuilder.add("database", VPackValue(DATABASE));
          jobBuilder.add("collection", VPackValue(COLLECTION));
          jobBuilder.add("shard", VPackValue(SHARD));
          jobBuilder.add("fromServer", VPackValue(SHARD_LEADER));
          jobBuilder.add("toServer", VPackValue(SHARD_FOLLOWER1));
          jobBuilder.add("jobId", VPackValue(jobId));
          jobBuilder.add("timeCreated", VPackValue(
                           timepointToString(std::chrono::system_clock::now())));}
        builder->add("1", jobBuilder.slice());
      }
      builder->close();
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        VPackArrayBuilder a(builder.get());
        builder->add(VPackValue(SHARD_FOLLOWER1));
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
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
  REQUIRE(builder);
  Node agency = createNodeFromBuilder(*builder);

  size_t numWrites = 0;
  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, bool d) -> write_ret_t {
    INFO("Write: " << q->slice().toJson());
    numWrites++;

    if (numWrites == 1) {
      REQUIRE(std::string(q->slice().typeName()) == "array" );
      REQUIRE(std::string(q->slice()[0][0].typeName()) == "object");
      REQUIRE(q->slice()[0][0].length() == 1);

      auto writes = q->slice()[0][0];
      REQUIRE(std::string(writes.get("/arango/Target/FailedServers/" + SHARD_LEADER).typeName()) == "object");
      REQUIRE(writes.get("/arango/Target/FailedServers/" + SHARD_LEADER).get("op").copyString() == "erase");
      REQUIRE(writes.get("/arango/Target/FailedServers/" + SHARD_LEADER).get("val").copyString() == SHARD);
      return fakeWriteResult;
    } else {
      REQUIRE(std::string(q->slice().typeName()) == "array" );
      REQUIRE(q->slice().length() == 1);
      REQUIRE(std::string(q->slice()[0].typeName()) == "array");
      REQUIRE(q->slice()[0].length() == 1); // we always simply override! no preconditions...
      REQUIRE(std::string(q->slice()[0][0].typeName()) == "object");

      auto writes = q->slice()[0][0];
      REQUIRE(std::string(writes.get("/arango/Supervision/Shards/" + SHARD).typeName()) == "object");
      REQUIRE(writes.get("/arango/Supervision/Shards/" + SHARD).get("op").copyString() == "delete");
      REQUIRE(std::string(writes.get("/arango/Target/Pending/1").get("op").typeName()) == "string");
      CHECK(writes.get("/arango/Target/Pending/1").get("op").copyString() == "delete");
      CHECK(std::string(writes.get("/arango/Target/Finished/1").typeName()) == "object");
      return fakeWriteResult;
    }
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface &agent = mockAgent.get();
  INFO("Agency: " << agency);
  auto failedLeader = FailedLeader(
    agency("arango"),
    &agent,
    JOB_STATUS::PENDING,
    jobId
  );
  failedLeader.run();
  Verify(Method(mockAgent, write));
}

SECTION("a failedleader must not take a follower into account that is in sync but has been dropped out of the plan") {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder(new Builder());
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Target/ToDo") {
        builder->add("1", createBuilder(todo).slice());
      }
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        // follower2 in sync
        VPackArrayBuilder a(builder.get());
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER2));
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
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
  REQUIRE(builder);
  INFO("Teststructure: " << builder->toJson());
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).AlwaysDo([&](query_t const& q) -> trans_ret_t {
    INFO("Transaction: " << q->slice().toJson());
    // must NOT be called!
    REQUIRE(false);
    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface &agent = mockAgent.get();

  // new server will randomly be selected...so seed the random number generator
  srand(1);
  auto failedLeader = FailedLeader(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedLeader.start();
}
}
}
}
}
