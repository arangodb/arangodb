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
#include "catch.hpp"
#include "fakeit.hpp"

#include "Agency/AddFollower.h"
#include "Agency/FailedFollower.h"
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

typedef std::function<std::unique_ptr<Builder>(
  Slice const&, std::string const&)>TestStructureType;

const char *agency =
#include "FailedFollowerTest.json"
;

VPackBuilder createJob() {
  VPackBuilder builder;
  VPackObjectBuilder a(&builder);
  {
    builder.add("creator", VPackValue("1"));
    builder.add("type", VPackValue("failedFollower"));
    builder.add("database", VPackValue(DATABASE));
    builder.add("collection", VPackValue(COLLECTION));
    builder.add("shard", VPackValue(SHARD));
    builder.add("fromServer", VPackValue(SHARD_FOLLOWER1));
    builder.add("jobId", VPackValue("1"));
    builder.add("timeCreated", VPackValue(
                           timepointToString(std::chrono::system_clock::now())));
  }
  return builder;
}

Node createNodeFromBuilder(VPackBuilder const& builder) {

  VPackBuilder opBuilder;
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
  
  VPackBuilder builder;
  builder.add(parser.steal()->slice());
  return builder;
  
}

Node createNode(char const* c) {
  return createNodeFromBuilder(createBuilder(c));
}

Node createRootNode() {
  return createNode(agency);
}


TEST_CASE("FailedFollower", "[agency][supervision]") {
  RandomGenerator::seed(3);
  auto transBuilder = std::make_shared<Builder>();
  { VPackArrayBuilder a(transBuilder.get());
    transBuilder->add(VPackValue((uint64_t)1)); }
  
  
  
  auto baseStructure = createRootNode();
  write_ret_t fakeWriteResult {true, "", std::vector<bool> {true}, std::vector<index_t> {1}};
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
    CHECK(job.get("type").copyString() == "failedFollower");
    REQUIRE(std::string(job.get("database").typeName()) == "string");
    CHECK(job.get("database").copyString() == DATABASE);
    REQUIRE(std::string(job.get("collection").typeName()) == "string");
    CHECK(job.get("collection").copyString() == COLLECTION);
    REQUIRE(std::string(job.get("shard").typeName()) == "string");
    CHECK(job.get("shard").copyString() == SHARD);
    REQUIRE(std::string(job.get("fromServer").typeName()) == "string");
    CHECK(job.get("fromServer").copyString() == SHARD_FOLLOWER1);
    CHECK(std::string(job.get("jobId").typeName()) == "string");
    CHECK(std::string(job.get("timeCreated").typeName()) == "string");

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface &agent = mockAgent.get();

  auto failedFollower = FailedFollower(
    baseStructure,
    &agent,
    jobId,
    "unittest",
    DATABASE,
    COLLECTION,
    SHARD,
    SHARD_FOLLOWER1
  );
  failedFollower.create();
  Verify(Method(mockAgent, write));
}

SECTION("if we want to start and the collection went missing from plan (our truth) the job should just finish") {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](VPackSlice const& s, std::string const& path) {
    std::unique_ptr<VPackBuilder> builder;
    if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION) {
      return builder;
    }

    builder.reset(new VPackBuilder());
    if (s.isObject()) {
      builder->add(VPackValue(VPackValueType::Object));
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
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
  auto failedFollower = FailedFollower(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedFollower.start();
}

SECTION("if we are supposed to fail a distributeShardsLike job we immediately fail because this should be done by a job running on the master shard") {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](VPackSlice const& s, std::string const& path) {
    std::unique_ptr<VPackBuilder> builder;
    builder.reset(new VPackBuilder());
    if (s.isObject()) {
      builder->add(VPackValue(VPackValueType::Object));
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
        builder->add("1", createJob().slice());
      }
      builder->close();
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
  auto failedFollower = FailedFollower(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedFollower.start();
}

SECTION("if the follower is healthy again we fail the job") {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](VPackSlice const& s, std::string const& path) {
    std::unique_ptr<VPackBuilder> builder;
    builder.reset(new VPackBuilder());
    if (s.isObject()) {
      builder->add(VPackValue(VPackValueType::Object));
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
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
  REQUIRE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, transact)).Do([&](query_t const& q) -> trans_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    auto preconditions = q->slice()[0][1];
    REQUIRE(preconditions.get("/arango/Supervision/Health/" + SHARD_FOLLOWER1 + "/Status").get("old").copyString() == "FAILED");

    char const* json = R"=([{"arango":{"Supervision":{"Health":{"follower1":{"Status":"GOOD"}}}}}])=";
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
  auto failedFollower = FailedFollower(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedFollower.start();
}

SECTION("if there is no healthy free server when trying to start just wait") {
  std::string jobId = "1";

  TestStructureType createTestStructure = [&](VPackSlice const& s, std::string const& path) {
    std::unique_ptr<VPackBuilder> builder;
    builder.reset(new VPackBuilder());
    if (s.isObject()) {
      builder->add(VPackValue(VPackValueType::Object));
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
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
  REQUIRE(builder);
  INFO("Agency: " << builder->toJson());
  Node agency = createNodeFromBuilder(*builder);
  
  // nothing should happen
  Mock<AgentInterface> mockAgent;
  AgentInterface &agent = mockAgent.get();
  auto failedFollower = FailedFollower(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedFollower.start();
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
        builder->add("1", createJob().slice());
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
    REQUIRE(std::string(writes.get("/arango/Target/Finished/1").typeName()) == "object");
    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface &agent = mockAgent.get();
  auto failedFollower = FailedFollower(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedFollower.start();
  Verify(Method(mockAgent, transact));
  Verify(Method(mockAgent, write));
}

SECTION("a successfully started job should finish immediately and set everything") {
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
        builder->add("1", createJob().slice());
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
  When(Method(mockAgent, transact)).Do([&](query_t const& q) -> trans_ret_t {
    // check that the job is now pending
    INFO("Transaction: " << q->slice().toJson());
    auto writes = q->slice()[0][0];
    auto planEntry = "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION
      + "/shards/" + SHARD;
    REQUIRE(std::string(writes.get("/arango/Target/Finished/1").typeName()) == "object");
    REQUIRE(std::string(writes.get(planEntry).typeName()) == "array");
    REQUIRE(writes.get(planEntry).length() == 3);
    REQUIRE(writes.get(planEntry)[0].copyString() == "leader");
    auto freeEntry = writes.get(planEntry)[1].copyString();
    REQUIRE(freeEntry.compare(0,4,FREE_SERVER) == 0);
    REQUIRE(writes.get(planEntry)[2].copyString() == "follower2");

    REQUIRE(writes.get("/arango/Plan/Version").get("op").copyString() == "increment");
    REQUIRE(std::string(writes.get("/arango/Target/Finished/1").typeName()) == "object");

    auto preconditions = q->slice()[0][1];
    REQUIRE(std::string(preconditions.typeName()) == "object");
    auto healthStat = std::string("/arango/Supervision/Health/") + freeEntry + "/Status";
    REQUIRE(preconditions.get(healthStat).get("old").copyString() == "GOOD");
    REQUIRE(std::string(preconditions.get(planEntry).get("old").typeName()) == "array");
    REQUIRE(preconditions.get(planEntry).get("old")[0].copyString() == "leader");
    REQUIRE(preconditions.get(planEntry).get("old")[1].copyString() == "follower1");
    REQUIRE(preconditions.get(planEntry).get("old")[2].copyString() == "follower2");
    REQUIRE(preconditions.get(std::string("/arango/Supervision/DBServers/")+freeEntry).get("oldEmpty").getBool() == true);
    REQUIRE(preconditions.get("/arango/Supervision/Shards/s99").get("oldEmpty").getBool() == true);

    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface &agent = mockAgent.get();
  auto failedFollower = FailedFollower(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedFollower.start();
  Verify(Method(mockAgent, transact));
}

SECTION("the job should handle distributeShardsLike") {
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
        builder->add("1", createJob().slice());
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
  When(Method(mockAgent, transact)).Do([&](query_t const& q) -> trans_ret_t {
    // check that the job is now pending
    INFO("Transaction: " << q->slice().toJson());
    auto writes = q->slice()[0][0];
    REQUIRE(std::string(writes.get("/arango/Target/Finished/1").typeName()) == "object");
    auto entry = std::string("/arango/Plan/Collections/") + DATABASE + "/" + COLLECTION + "/shards/" + SHARD;
    REQUIRE(std::string(writes.get(entry).typeName()) == "array");
    REQUIRE(writes.get(entry).length() == 3);
    REQUIRE(writes.get(entry)[0].copyString() == "leader");
    auto freeEntry = writes.get(entry)[1].copyString();
    REQUIRE(writes.get(entry)[1].copyString().compare(0,4,FREE_SERVER) == 0);
    REQUIRE(writes.get(entry)[2].copyString() == "follower2");
    REQUIRE(std::string(writes.get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection1/shards/s100").typeName()) == "array");
    REQUIRE(writes.get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection1/shards/s100").length() == 3);
    REQUIRE(writes.get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection1/shards/s100")[0].copyString() == "leader");
    REQUIRE(writes.get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection1/shards/s100")[1].copyString().compare(0,4,FREE_SERVER) == 0);

    REQUIRE(writes.get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection1/shards/s100")[2].copyString() == "follower2");
    REQUIRE(std::string(writes.get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection2/shards/s101").typeName()) == "array");
    REQUIRE(writes.get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection2/shards/s101").length() == 3);
    REQUIRE(writes.get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection2/shards/s101")[0].copyString() == "leader");
    REQUIRE(writes.get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection2/shards/s101")[1].copyString().compare(0,4,FREE_SERVER) == 0);
    REQUIRE(writes.get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection2/shards/s101")[2].copyString() == "follower2");

    REQUIRE(writes.get("/arango/Plan/Version").get("op").copyString() == "increment");
    REQUIRE(std::string(writes.get("/arango/Target/Finished/1").typeName()) == "object");

    auto preconditions = q->slice()[0][1];
    REQUIRE(std::string(preconditions.typeName()) == "object");
    auto healthStat = std::string("/arango/Supervision/Health/") + freeEntry + "/Status";
    REQUIRE(preconditions.get(healthStat).get("old").copyString() == "GOOD");
    REQUIRE(std::string(preconditions.get(entry).get("old").typeName()) == "array");
    REQUIRE(preconditions.get(entry).get("old")[0].copyString() == "leader");
    REQUIRE(preconditions.get(entry).get("old")[1].copyString() == "follower1");
    REQUIRE(preconditions.get(entry).get("old")[2].copyString() == "follower2");
    REQUIRE(preconditions.get(std::string("/arango/Supervision/DBServers/") + freeEntry).get("oldEmpty").getBool() == true);
    REQUIRE(preconditions.get("/arango/Supervision/Shards/s99").get("oldEmpty").getBool() == true);

    return fakeTransResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface &agent = mockAgent.get();
  auto failedFollower = FailedFollower(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedFollower.start();
  Verify(Method(mockAgent, transact));
}

SECTION("the job should timeout after a while") {
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
        VPackBuilder todoJob = createJob();
        VPackBuilder timedOutJob;
        {
          VPackObjectBuilder a(&timedOutJob);
          for (auto const& it: VPackObjectIterator(todoJob.slice())) {
            if (it.key.copyString() == "timeCreated") {
              timedOutJob.add("timeCreated", VPackValue("2015-01-01T00:00:00Z"));
            } else {
              timedOutJob.add(it.key.copyString(), it.value);
            }
          }
        }
        builder->add("1", timedOutJob.slice());
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
    // check that the job is now pending
    INFO("Write: " << q->slice().toJson());
    auto writes = q->slice()[0][0];
    REQUIRE(std::string(writes.get("/arango/Target/Failed/1").typeName()) == "object");

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface &agent = mockAgent.get();
  auto failedFollower = FailedFollower(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedFollower.start();
  Verify(Method(mockAgent, write));
}

SECTION("the job should be abortable when it is in todo") {
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
        builder->add("1", createJob().slice());
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
    // check that the job is now pending
    INFO("Write: " << q->slice().toJson());
    auto writes = q->slice()[0][0];
    REQUIRE(std::string(writes.get("/arango/Target/Failed/1").typeName()) == "object");

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface &agent = mockAgent.get();
  auto failedFollower = FailedFollower(
    agency("arango"),
    &agent,
    JOB_STATUS::TODO,
    jobId
  );
  failedFollower.abort();
  Verify(Method(mockAgent, write));
}



}
}
}
}
