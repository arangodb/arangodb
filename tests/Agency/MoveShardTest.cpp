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

#include "Agency/MoveShard.h"
#include "Agency/AgentInterface.h"
#include "Agency/Node.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::consensus;
using namespace fakeit;

const std::string PREFIX = "arango";
const std::string DATABASE = "database";
const std::string COLLECTION = "collection";
const std::string SHARD = "s99";
const std::string SHARD_LEADER = "leader";
const std::string SHARD_FOLLOWER1 = "follower1";
const std::string FREE_SERVER = "free";
const std::string FREE_SERVER2 = "free2";

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::consensus;
using namespace fakeit;

bool aborts = false;

namespace arangodb {
namespace tests {
namespace move_shard_test {

const char *agency =
#include "MoveShardTest.json"
;

Node createAgencyFromBuilder(VPackBuilder const& builder) {
  Node node("");

  VPackBuilder opBuilder;
  {
    VPackObjectBuilder a(&opBuilder);
    opBuilder.add("new", builder.slice());
  }

  node.handle<SET>(opBuilder.slice());
  return node(PREFIX);
}

#define CHECK_FAILURE(source, query) \
    std::string sourceKey = "/arango/Target/";\
    sourceKey += source;\
    sourceKey += "/1"; \
    REQUIRE(std::string(q->slice().typeName()) == "array"); \
    REQUIRE(q->slice().length() == 1); \
    REQUIRE(std::string(q->slice()[0].typeName()) == "array"); \
    REQUIRE(q->slice()[0].length() == 1); \
    REQUIRE(std::string(q->slice()[0][0].typeName()) == "object"); \
    auto writes = q->slice()[0][0]; \
    REQUIRE(std::string(writes.get(sourceKey).typeName()) == "object"); \
    REQUIRE(std::string(writes.get(sourceKey).get("op").typeName()) == "string"); \
    CHECK(writes.get(sourceKey).get("op").copyString() == "delete"); \
    CHECK(std::string(writes.get("/arango/Target/Failed/1").typeName()) == "object");

Node createRootNode() {

  VPackOptions options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(agency);

  VPackBuilder builder;
  { VPackObjectBuilder a(&builder);
    builder.add("new", parser.steal()->slice()); }

  Node root("ROOT");
  root.handle<SET>(builder.slice());
  return root;
}

VPackBuilder createJob(std::string const& collection, std::string const& from, std::string const& to) {
  VPackBuilder builder;
  {
    VPackObjectBuilder b(&builder);
    builder.add("jobId", VPackValue("1"));
    builder.add("creator", VPackValue("unittest"));
    builder.add("type", VPackValue("moveShard"));
    builder.add("database", VPackValue(DATABASE));
    builder.add("collection", VPackValue(collection));
    builder.add("shard", VPackValue(SHARD));
    builder.add("fromServer", VPackValue(from));
    builder.add("toServer", VPackValue(to));
    builder.add("isLeader", VPackValue(from == SHARD_LEADER));
  }
  return builder;
}

TEST_CASE("MoveShard", "[agency][supervision]") {
auto baseStructure = createRootNode();
write_ret_t fakeWriteResult {true, "", std::vector<apply_ret_t> {APPLIED}, std::vector<index_t> {1}};
std::string const jobId = "1";

SECTION("the job should fail if toServer does not exist") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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
        builder->add(jobId, createJob(COLLECTION, SHARD_LEADER, "unfug").slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    CHECK_FAILURE("ToDo", q);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent,write));
}

SECTION("the job should fail to start if fromServer and toServer are planned followers") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
    [&](VPackSlice const& s, std::string const& path) {
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
        builder->add(jobId, createJob(COLLECTION, SHARD_FOLLOWER1, SHARD_LEADER).slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    CHECK_FAILURE("ToDo", q);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent,write));
}

SECTION("the job should fail if fromServer does not exist") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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
        builder->add(jobId, createJob(COLLECTION, "unfug", FREE_SERVER).slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();
  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  Mock<Job> spy(moveShard);
  Fake(Method(spy, finish));

  Job& spyMoveShard = spy.get();
  spyMoveShard.start(aborts);

  Verify(Method(spy, finish).Matching([](std::string const& server, std::string const& shard, bool success, std::string const& reason, query_t const payload) -> bool {return !success;}));
}

SECTION("the job should fail if fromServer is not in plan of the shard") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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
        builder->add(jobId, createJob(COLLECTION, FREE_SERVER, FREE_SERVER2).slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    CHECK_FAILURE("ToDo", q);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent,write));
}

SECTION("the job should fail if fromServer does not exist") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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
        builder->add(jobId, createJob(COLLECTION, "unfug", FREE_SERVER).slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
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
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent,write));
}

SECTION("the job should remain in todo if the shard is currently locked") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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
        builder->add(jobId, createJob(COLLECTION, SHARD_LEADER, FREE_SERVER).slice());
      } else if (path == "/arango/Supervision/Shards") {
        builder->add(SHARD, VPackValue("2"));
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  // nothing should be called (job remains in ToDo)
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
}

SECTION("the job should remain in todo if the target server is currently locked") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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
        builder->add(jobId, createJob(COLLECTION, SHARD_LEADER, FREE_SERVER).slice());
      } else if (path == "/arango/Supervision/DBServers") {
        builder->add(FREE_SERVER, VPackValue("2"));
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  // nothing should be called (job remains in ToDo)
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
}

SECTION("the job should fail if the target server was cleaned out") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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
        builder->add(jobId, createJob(COLLECTION, SHARD_LEADER, FREE_SERVER).slice());
      }
      builder->close();
    } else {
      if (path == "/arango/Target/CleanedServers") {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    CHECK_FAILURE("ToDo", q);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent,write));
}

SECTION("the job should fail if the target server is failed") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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
        builder->add(jobId, createJob(COLLECTION, SHARD_LEADER, FREE_SERVER).slice());
      }

      if (path == "/arango/Target/FailedServers") {
        builder->add(FREE_SERVER, VPackValue(true));
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    CHECK_FAILURE("ToDo", q);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent,write));
}

SECTION("the job should fail if the target server is failed") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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
        builder->add(jobId, createJob(COLLECTION, SHARD_LEADER, FREE_SERVER).slice());
      }
      builder->close();
    } else {
      if (path == "/arango/Supervision/Health/" + FREE_SERVER + "/Status") {
        builder->add(VPackValue("FAILED"));
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    CHECK_FAILURE("ToDo", q);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent,write));
}

SECTION("the job should fail if the shard distributes its shards like some other") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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
        builder->add(jobId, createJob(COLLECTION, SHARD_LEADER, FREE_SERVER).slice());
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION) {
        builder->add("distributeShardsLike", VPackValue("PENG"));
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    CHECK_FAILURE("ToDo", q);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent,write));
}

SECTION("the job should be moved to pending when everything is ok") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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
        builder->add(jobId, createJob(COLLECTION, SHARD_LEADER, FREE_SERVER).slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    std::string sourceKey = "/arango/Target/ToDo/1";
    REQUIRE(std::string(q->slice().typeName()) == "array");
    REQUIRE(q->slice().length() == 1);
    REQUIRE(std::string(q->slice()[0].typeName()) == "array");
    REQUIRE(q->slice()[0].length() == 2);
    REQUIRE(std::string(q->slice()[0][0].typeName()) == "object");
    REQUIRE(std::string(q->slice()[0][1].typeName()) == "object");

    auto writes = q->slice()[0][0];
    REQUIRE(std::string(writes.get(sourceKey).typeName()) == "object");
    REQUIRE(std::string(writes.get(sourceKey).get("op").typeName()) == "string");
    CHECK(writes.get(sourceKey).get("op").copyString() == "delete");
    CHECK(writes.get("/arango/Supervision/Shards/" + SHARD).copyString() == "1");
    CHECK(writes.get("/arango/Supervision/DBServers/" + FREE_SERVER).copyString() == "1");
    CHECK(writes.get("/arango/Plan/Version").get("op").copyString() == "increment");
    CHECK(std::string(writes.get("/arango/Target/Pending/1").typeName()) == "object");
    CHECK(std::string(writes.get("/arango/Target/Pending/1").get("timeStarted").typeName()) == "string");
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).length() == 3); // leader, oldFollower, newLeader
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[0].copyString() == SHARD_LEADER);

    // order not really relevant ... assume it might appear anyway
    auto followers = writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD);
    bool found = false;
    for (auto const& server: VPackArrayIterator(followers)) {
      if (server.copyString() == FREE_SERVER) {
        found = true;
      }
    }
    CHECK(found == true);

    auto preconditions = q->slice()[0][1];
    CHECK(preconditions.get("/arango/Target/CleanedServers").get("old").toJson() == "[]");
    CHECK(preconditions.get("/arango/Target/FailedServers").get("old").toJson() == "{}");
    CHECK(preconditions.get("/arango/Supervision/Health/" + FREE_SERVER + "/Status").get("old").copyString() == "GOOD");
    CHECK(preconditions.get("/arango/Supervision/DBServers/" + FREE_SERVER).get("oldEmpty").getBool() == true);
    CHECK(preconditions.get("/arango/Supervision/Shards/" + SHARD).get("oldEmpty").getBool() == true);
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old").toJson() == "[\"" + SHARD_LEADER + "\",\"" + SHARD_FOLLOWER1 + "\"]");

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent,write));
}

SECTION("moving from a follower should be possible") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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
        builder->add(jobId, createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER).slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());

    auto writes = q->slice()[0][0];
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).length() == 3); // leader, oldFollower, newLeader
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[0].copyString() == SHARD_LEADER);

    // order not really relevant ... assume it might appear anyway
    auto followers = writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD);
    bool found = false;
    for (auto const& server: VPackArrayIterator(followers)) {
      if (server.copyString() == FREE_SERVER) {
        found = true;
      }
    }
    CHECK(found == true);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent,write));
}

SECTION("when moving a shard that is a distributeShardsLike leader move the rest as well") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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
        builder->add(jobId, createJob(COLLECTION, SHARD_LEADER, FREE_SERVER).slice());
      } else if (path == "/arango/Current/Collections/" + DATABASE) {
        // we fake that follower2 is in sync
        builder->add(VPackValue("linkedcollection1"));
        {
          VPackObjectBuilder f(builder.get());
          builder->add(VPackValue("s100"));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue("servers"));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
            }
          }
        }
        // for the other shard there is only follower1 in sync
        builder->add(VPackValue("linkedcollection2"));
        {
          VPackObjectBuilder f(builder.get());
          builder->add(VPackValue("s101"));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue("servers"));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
            }
          }
        }
      } else if (path == "/arango/Plan/Collections/" + DATABASE) {
        builder->add(VPackValue("linkedcollection1"));
        {
          VPackObjectBuilder f(builder.get());
          builder->add("distributeShardsLike", VPackValue(COLLECTION));
          builder->add(VPackValue("shards"));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue("s100"));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
            }
          }
        }
        builder->add(VPackValue("linkedcollection2"));
        {
          VPackObjectBuilder f(builder.get());
          builder->add("distributeShardsLike", VPackValue(COLLECTION));
          builder->add(VPackValue("shards"));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue("s101"));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
            }
          }
        }
        builder->add(VPackValue("unrelatedcollection"));
        {
          VPackObjectBuilder f(builder.get());
          builder->add(VPackValue("shards"));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue("unrelatedshard"));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
            }
          }
        }
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    auto writes = q->slice()[0][0];
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).length() == 3); // leader, oldFollower, newLeader

    auto json = writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).toJson();
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection1/shards/s100").toJson() == json);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection2/shards/s101").toJson() == json);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/unrelatedcollection/shards/unrelatedshard").isNone());
    CHECK(writes.get("/arango/Supervision/Shards/" + SHARD).copyString() == "1");
    CHECK(writes.get("/arango/Supervision/Shards/unrelatedshard").isNone());

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent,write));
}

SECTION("if the job is too old it should be aborted to prevent a deadloop") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        VPackBuilder pendingJob;
        {
          VPackObjectBuilder b(&pendingJob);
          auto plainJob = createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER);
          for (auto const& it: VPackObjectIterator(plainJob.slice())) {
            pendingJob.add(it.key.copyString(), it.value);
          }
          pendingJob.add("timeCreated", VPackValue("2015-01-03T20:00:00Z"));
        }
        builder->add(jobId, pendingJob.slice());
      }
      builder->close();
    } else {
      if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  Node agency = createAgencyFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  Mock<Job> spy(moveShard);
  Fake(Method(spy, abort));

  Job& spyMoveShard = spy.get();
  spyMoveShard.run(aborts);

  Verify(Method(spy, abort));
}

SECTION("if the job is too old (leader case) it should be aborted to prevent a deadloop") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        VPackBuilder pendingJob;
        {
          VPackObjectBuilder b(&pendingJob);
          auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
          for (auto const& it: VPackObjectIterator(plainJob.slice())) {
            pendingJob.add(it.key.copyString(), it.value);
          }
          pendingJob.add("timeCreated", VPackValue("2015-01-03T20:00:00Z"));
        }
        builder->add(jobId, pendingJob.slice());
      }
      builder->close();
    } else {
      if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  Node agency = createAgencyFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  Mock<Job> spy(moveShard);
  Fake(Method(spy, abort));

  Job& spyMoveShard = spy.get();
  spyMoveShard.run(aborts);

  Verify(Method(spy, abort));
}

SECTION("if the collection was dropped while moving finish the job") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        VPackBuilder pendingJob;
        {
          VPackObjectBuilder b(&pendingJob);
          auto plainJob = createJob("BOGUS", SHARD_FOLLOWER1, FREE_SERVER);
          for (auto const& it: VPackObjectIterator(plainJob.slice())) {
            pendingJob.add(it.key.copyString(), it.value);
          }
          pendingJob.add("timeCreated", VPackValue(timepointToString(std::chrono::system_clock::now())));
        }
        builder->add(jobId, pendingJob.slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  Node agency = createAgencyFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  Mock<Job> spy(moveShard);
  Fake(Method(spy, finish));

  Job& spyMoveShard = spy.get();
  spyMoveShard.run(aborts);

  Verify(Method(spy, finish).Matching([](std::string const& server, std::string const& shard, bool success, std::string const& reason, query_t const payload) -> bool {
    return success;
  }));
}

SECTION("if the collection was dropped before the job could be started just finish the job") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
    std::unique_ptr<VPackBuilder> builder;
    builder.reset(new VPackBuilder());
    if (s.isObject()) {
      builder->add(VPackValue(VPackValueType::Object));
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder && !(path == "/arango/Plan/Collections/" + DATABASE && it.key.copyString() == COLLECTION)) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }

      if (path == "/arango/Target/ToDo") {
        VPackBuilder pendingJob;
        {
          VPackObjectBuilder b(&pendingJob);
          auto plainJob = createJob("ANUNKNOWNCOLLECTION", SHARD_FOLLOWER1, FREE_SERVER);
          for (auto const& it: VPackObjectIterator(plainJob.slice())) {
            pendingJob.add(it.key.copyString(), it.value);
          }
          pendingJob.add("timeCreated", VPackValue(timepointToString(std::chrono::system_clock::now())));
        }
        builder->add(jobId, pendingJob.slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  Node agency = createAgencyFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  Mock<Job> spy(moveShard);
  Fake(Method(spy, finish));

  Job& spyMoveShard = spy.get();
  spyMoveShard.start(aborts);

  Verify(Method(spy, finish).Matching([](std::string const& server, std::string const& shard, bool success, std::string const& reason, query_t const payload) -> bool {return success;}));

}

SECTION("the job should wait until the planned shard situation has been created in Current") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        VPackBuilder pendingJob;
        {
          VPackObjectBuilder b(&pendingJob);
          auto plainJob = createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER);
          for (auto const& it: VPackObjectIterator(plainJob.slice())) {
            pendingJob.add(it.key.copyString(), it.value);
          }
          pendingJob.add("timeCreated", VPackValue(timepointToString(std::chrono::system_clock::now())));
        }
        builder->add(jobId, pendingJob.slice());
      }
      builder->close();
    } else {
      if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  Node agency = createAgencyFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  // should not write anything because we are not yet in sync
  AgentInterface& agent = mockAgent.get();

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
}

SECTION("if the job is done it should properly finish itself") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        VPackBuilder pendingJob;
        {
          VPackObjectBuilder b(&pendingJob);
          auto plainJob = createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER);
          for (auto const& it: VPackObjectIterator(plainJob.slice())) {
            pendingJob.add(it.key.copyString(), it.value);
          }
          pendingJob.add("timeCreated", VPackValue(timepointToString(std::chrono::system_clock::now())));
        }
        builder->add(jobId, pendingJob.slice());
      }
      builder->close();
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  Node agency = createAgencyFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    auto writes = q->slice()[0][0];
    CHECK(writes.get("/arango/Target/Pending/1").get("op").copyString() == "delete");
    CHECK(std::string(writes.get("/arango/Target/Finished/1").typeName()) == "object");
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).toJson() == "[\"leader\",\"free\"]");
    CHECK(writes.get("/arango/Supervision/Shards/" + SHARD).get("op").copyString() == "delete");
    CHECK(writes.get("/arango/Supervision/DBServers/" + FREE_SERVER).get("op").copyString() == "delete");

    auto preconditions = q->slice()[0][1];
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old").length() == 3);

    return fakeWriteResult;
  });
  AgentInterface& agent = mockAgent.get();

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
  Verify(Method(mockAgent,write));
}

SECTION("the job should not finish itself when only parts of distributeShardsLike have adapted") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        VPackBuilder pendingJob;
        {
          VPackObjectBuilder b(&pendingJob);
          auto plainJob = createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER);
          for (auto const& it: VPackObjectIterator(plainJob.slice())) {
            pendingJob.add(it.key.copyString(), it.value);
          }
          pendingJob.add("timeCreated", VPackValue(timepointToString(std::chrono::system_clock::now())));
        }
        builder->add(jobId, pendingJob.slice());
      } else if (path == "/arango/Current/Collections/" + DATABASE) {
        // we fake that follower2 is in sync
        builder->add(VPackValue("linkedcollection1"));
        {
          VPackObjectBuilder f(builder.get());
          builder->add(VPackValue("s100"));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue("servers"));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
              builder->add(VPackValue(FREE_SERVER));
            }
          }
        }
        // for the other shard there is only follower1 in sync
        builder->add(VPackValue("linkedcollection2"));
        {
          VPackObjectBuilder f(builder.get());
          builder->add(VPackValue("s101"));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue("servers"));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
            }
          }
        }
      } else if (path == "/arango/Plan/Collections/" + DATABASE) {
        builder->add(VPackValue("linkedcollection1"));
        {
          VPackObjectBuilder f(builder.get());
          builder->add("distributeShardsLike", VPackValue(COLLECTION));
          builder->add(VPackValue("shards"));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue("s100"));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
              builder->add(VPackValue(FREE_SERVER));
            }
          }
        }
        builder->add(VPackValue("linkedcollection2"));
        {
          VPackObjectBuilder f(builder.get());
          builder->add("distributeShardsLike", VPackValue(COLLECTION));
          builder->add(VPackValue("shards"));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue("s101"));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
              builder->add(VPackValue(FREE_SERVER));
            }
          }
        }
        builder->add(VPackValue("unrelatedcollection"));
        {
          VPackObjectBuilder f(builder.get());
          builder->add(VPackValue("shards"));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue("unrelatedshard"));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
            }
          }
        }
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  // nothing should happen...child shards not yet in sync
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
}

SECTION("the job should finish when all distributeShardsLike shards have adapted") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
    std::unique_ptr<VPackBuilder> builder;
    builder.reset(new VPackBuilder());
    if (s.isObject()) {
      builder->add(VPackValue(VPackValueType::Object));
      for (auto const& it: VPackObjectIterator(s)) {
        auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder && it.key.copyString() != COLLECTION && path != "/arango/Current/Collections/" + DATABASE) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }

      if (path == "/arango/Target/Pending") {
        VPackBuilder pendingJob;
        {
          VPackObjectBuilder b(&pendingJob);
          auto plainJob = createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER);
          for (auto const& it: VPackObjectIterator(plainJob.slice())) {
            pendingJob.add(it.key.copyString(), it.value);
          }
          pendingJob.add("timeCreated", VPackValue(timepointToString(std::chrono::system_clock::now())));
        }
        builder->add(jobId, pendingJob.slice());
      } else if (path == "/arango/Current/Collections/" + DATABASE) {
        builder->add(VPackValue(COLLECTION));
        {
          VPackObjectBuilder f(builder.get());
          builder->add(VPackValue(SHARD));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue("servers"));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
              builder->add(VPackValue(FREE_SERVER));
            }
          }
        }
        // we fake that follower2 is in sync
        builder->add(VPackValue("linkedcollection1"));
        {
          VPackObjectBuilder f(builder.get());
          builder->add(VPackValue("s100"));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue("servers"));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
              builder->add(VPackValue(FREE_SERVER));
            }
          }
        }
        // for the other shard there is only follower1 in sync
        builder->add(VPackValue("linkedcollection2"));
        {
          VPackObjectBuilder f(builder.get());
          builder->add(VPackValue("s101"));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue("servers"));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
              builder->add(VPackValue(FREE_SERVER));
            }
          }
        }
      } else if (path == "/arango/Plan/Collections/" + DATABASE) {
        builder->add(VPackValue(COLLECTION));
        {
          VPackObjectBuilder f(builder.get());
          builder->add(VPackValue("shards"));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue(SHARD));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
              builder->add(VPackValue(FREE_SERVER));
            }
          }
        }
        builder->add(VPackValue("linkedcollection1"));
        {
          VPackObjectBuilder f(builder.get());
          builder->add("distributeShardsLike", VPackValue(COLLECTION));
          builder->add(VPackValue("shards"));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue("s100"));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
              builder->add(VPackValue(FREE_SERVER));
            }
          }
        }
        builder->add(VPackValue("linkedcollection2"));
        {
          VPackObjectBuilder f(builder.get());
          builder->add("distributeShardsLike", VPackValue(COLLECTION));
          builder->add(VPackValue("shards"));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue("s101"));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
              builder->add(VPackValue(FREE_SERVER));
            }
          }
        }
        builder->add(VPackValue("unrelatedcollection"));
        {
          VPackObjectBuilder f(builder.get());
          builder->add(VPackValue("shards"));
          {
            VPackObjectBuilder f(builder.get());
            builder->add(VPackValue("unrelatedshard"));
            {
              VPackArrayBuilder g(builder.get());
              builder->add(VPackValue(SHARD_LEADER));
              builder->add(VPackValue(SHARD_FOLLOWER1));
            }
          }
        }
      } else if (path == "/arango/Supervision/Shards") {
        builder->add(SHARD, VPackValue(1));
      } else if (path == "/arango/Supervision/DBServers") {
        builder->add(FREE_SERVER, VPackValue(1));
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    auto writes = q->slice()[0][0];
    CHECK(writes.get("/arango/Target/Pending/1").get("op").copyString() == "delete");
    CHECK(std::string(writes.get("/arango/Target/Finished/1").typeName()) == "object");
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).toJson() == "[\"leader\",\"free\"]");
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection1/shards/s100").toJson() == "[\"leader\",\"free\"]");
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection2/shards/s101").toJson() == "[\"leader\",\"free\"]");
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/unrelatedcollection/shards/unrelatedshard").isNone());
    CHECK(writes.get("/arango/Supervision/Shards/s100").isNone());

    auto preconditions = q->slice()[0][1];
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old").length() == 3);
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection1/shards/s100").get("old").length() == 3);
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection2/shards/s101").get("old").length() == 3);
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/unrelatedcollection/shards/unrelatedshard").isNone());

    return fakeWriteResult;
  });
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
  Verify(Method(mockAgent,write));
}

SECTION("a moveshard job that just made it to ToDo can simply be aborted") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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
        builder->add(jobId, createJob(COLLECTION, SHARD_LEADER, FREE_SERVER).slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    REQUIRE(q->slice()[0].length() == 2); // we always simply override! no preconditions...
    auto writes = q->slice()[0][0];
    CHECK(writes.get("/arango/Target/ToDo/1").get("op").copyString() == "delete");
    CHECK(std::string(writes.get("/arango/Target/Finished/1").typeName()) == "object");
    auto precond = q->slice()[0][1];
    CHECK(precond.get("/arango/Target/ToDo/1").get("oldEmpty").isFalse());

    return fakeWriteResult;
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.abort("test abort");
  Verify(Method(mockAgent,write));
}

SECTION("a pending moveshard job should also put the original server back into place when aborted") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        builder->add(jobId, createJob(COLLECTION, SHARD_LEADER, FREE_SERVER).slice());
      } else if (path == "/arango/Supervision/DBServers") {
        builder->add(FREE_SERVER, VPackValue("1"));
      } else if (path == "/arango/Supervision/Shards") {
        builder->add(SHARD, VPackValue("1"));
      }
      builder->close();
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->close();
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    auto writes = q->slice()[0][0];
    CHECK(writes.get("/arango/Target/Pending/1").get("op").copyString() == "delete");
    REQUIRE(q->slice()[0].length() == 2); // Precondition: to Server not leader yet
    CHECK(writes.get("/arango/Supervision/DBServers/" + FREE_SERVER).get("op").copyString() == "delete");
    CHECK(writes.get("/arango/Supervision/Shards/" + SHARD).get("op").copyString() == "delete");
    CHECK(std::string(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).typeName()) == "array");
    // apparently we are not cleaning up our mess. this is done somewhere else :S (>=2)
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).length() >= 2);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[0].copyString() == SHARD_LEADER);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[1].copyString() == SHARD_FOLLOWER1);
    CHECK(std::string(writes.get("/arango/Target/Failed/1").typeName()) == "object");

    return fakeWriteResult;
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.abort("test abort");
  Verify(Method(mockAgent,write));
}

SECTION("after the new leader has synchronized the new leader should resign") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        VPackBuilder pendingJob;
        {
          VPackObjectBuilder b(&pendingJob);
          auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
          for (auto const& it: VPackObjectIterator(plainJob.slice())) {
            pendingJob.add(it.key.copyString(), it.value);
          }
          pendingJob.add("timeCreated", VPackValue(timepointToString(std::chrono::system_clock::now())));
        }
        builder->add(jobId, pendingJob.slice());
      } else if (path == "/arango/Supervision/DBServers") {
        builder->add(FREE_SERVER, VPackValue("1"));
      } else if (path == "/arango/Supervision/Shards") {
        builder->add(SHARD, VPackValue("1"));
      }
      builder->close();
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    auto writes = q->slice()[0][0];
    CHECK(std::string(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).typeName()) == "array");
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).length() == 3);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[0].copyString() == "_" + SHARD_LEADER);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[1].copyString() == SHARD_FOLLOWER1);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[2].copyString() == FREE_SERVER);

    REQUIRE(q->slice()[0].length() == 2);
    auto preconditions = q->slice()[0][1];
    CHECK(std::string(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).typeName()) == "object");
    CHECK(std::string(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old").typeName()) == "array");
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old").length() == 3);
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[0].copyString() == SHARD_LEADER);
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[1].copyString() == SHARD_FOLLOWER1);
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[2].copyString() == FREE_SERVER);
    return fakeWriteResult;
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
  Verify(Method(mockAgent,write));
}

SECTION("when the old leader is not yet ready for resign nothing should happen") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        VPackBuilder pendingJob;
        {
          VPackObjectBuilder b(&pendingJob);
          auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
          for (auto const& it: VPackObjectIterator(plainJob.slice())) {
            pendingJob.add(it.key.copyString(), it.value);
          }
          pendingJob.add("timeCreated", VPackValue(timepointToString(std::chrono::system_clock::now())));
        }
        builder->add(jobId, pendingJob.slice());
      } else if (path == "/arango/Supervision/DBServers") {
        builder->add(FREE_SERVER, VPackValue("1"));
      } else if (path == "/arango/Supervision/Shards") {
        builder->add(SHARD, VPackValue("1"));
      }
      builder->close();
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue("_" + SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  // nothing should happen so nothing should be called
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
}

SECTION("aborting the job while a leader transition is in progress (for example when job is timing out) should make the old leader leader again") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        VPackBuilder pendingJob;
        {
          VPackObjectBuilder b(&pendingJob);
          auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
          for (auto const& it: VPackObjectIterator(plainJob.slice())) {
            pendingJob.add(it.key.copyString(), it.value);
          }
          pendingJob.add("timeCreated", VPackValue(timepointToString(std::chrono::system_clock::now())));
        }
        builder->add(jobId, pendingJob.slice());
      } else if (path == "/arango/Supervision/DBServers") {
        builder->add(FREE_SERVER, VPackValue("1"));
      } else if (path == "/arango/Supervision/Shards") {
        builder->add(SHARD, VPackValue("1"));
      }
      builder->close();
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue("_" + SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());

    auto writes = q->slice()[0][0];
    CHECK(writes.get("/arango/Target/Pending/1").get("op").copyString() == "delete");
    REQUIRE(q->slice()[0].length() == 2); // Precondition: to Server not leader yet
    CHECK(writes.get("/arango/Supervision/DBServers/" + FREE_SERVER).get("op").copyString() == "delete");
    CHECK(writes.get("/arango/Supervision/Shards/" + SHARD).get("op").copyString() == "delete");
    CHECK(std::string(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).typeName()) == "array");
    // well apparently this job is not responsible to cleanup its mess
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).length() >= 2);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[0].copyString() == SHARD_LEADER);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[1].copyString() == SHARD_FOLLOWER1);
    CHECK(std::string(writes.get("/arango/Target/Failed/1").typeName()) == "object");

    return fakeWriteResult;
  });
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.abort("test abort");
  Verify(Method(mockAgent,write));
}

SECTION("if we are ready to resign the old server then finally move to the new leader") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        VPackBuilder pendingJob;
        {
          VPackObjectBuilder b(&pendingJob);
          auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
          for (auto const& it: VPackObjectIterator(plainJob.slice())) {
            pendingJob.add(it.key.copyString(), it.value);
          }
          pendingJob.add("timeCreated", VPackValue(timepointToString(std::chrono::system_clock::now())));
        }
        builder->add(jobId, pendingJob.slice());
      } else if (path == "/arango/Supervision/DBServers") {
        builder->add(FREE_SERVER, VPackValue("1"));
      } else if (path == "/arango/Supervision/Shards") {
        builder->add(SHARD, VPackValue("1"));
      }
      builder->close();
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue("_" + SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue("_" + SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    auto writes = q->slice()[0][0];
    CHECK(std::string(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).typeName()) == "array");
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).length() == 3);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[0].copyString() == FREE_SERVER);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[1].copyString() == SHARD_FOLLOWER1);

    REQUIRE(q->slice()[0].length() == 2);
    auto preconditions = q->slice()[0][1];
    CHECK(std::string(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).typeName()) == "object");
    CHECK(std::string(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old").typeName()) == "array");
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old").length() == 3);
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[0].copyString() == "_" + SHARD_LEADER);
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[1].copyString() == SHARD_FOLLOWER1);
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[2].copyString() == FREE_SERVER);
    return fakeWriteResult;
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
  Verify(Method(mockAgent,write));
}

SECTION("if the new leader took over finish the job") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        VPackBuilder pendingJob;
        {
          VPackObjectBuilder b(&pendingJob);
          auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
          for (auto const& it: VPackObjectIterator(plainJob.slice())) {
            pendingJob.add(it.key.copyString(), it.value);
          }
          pendingJob.add("timeCreated", VPackValue(timepointToString(std::chrono::system_clock::now())));
        }
        builder->add(jobId, pendingJob.slice());
      } else if (path == "/arango/Supervision/DBServers") {
        builder->add(FREE_SERVER, VPackValue("1"));
      } else if (path == "/arango/Supervision/Shards") {
        builder->add(SHARD, VPackValue("1"));
      }
      builder->close();
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(FREE_SERVER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->close();
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(FREE_SERVER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    auto writes = q->slice()[0][0];
    CHECK(writes.length() == 4);
    CHECK(writes.get("/arango/Target/Pending/1").get("op").copyString() == "delete");
    CHECK(std::string(writes.get("/arango/Target/Finished/1").typeName()) == "object");
    CHECK(writes.get("/arango/Supervision/DBServers/" + FREE_SERVER).get("op").copyString() == "delete");
    CHECK(writes.get("/arango/Supervision/Shards/" + SHARD).get("op").copyString() == "delete");

    REQUIRE(q->slice()[0].length() == 2);
    auto preconditions = q->slice()[0][1];
    CHECK(std::string(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).typeName()) == "object");
    CHECK(std::string(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old").typeName()) == "array");
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old").length() == 2);
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[0].copyString() == FREE_SERVER);
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[1].copyString() == SHARD_FOLLOWER1);
    return fakeWriteResult;
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
  Verify(Method(mockAgent,write));
}

SECTION("calling an unknown job should be possible without throwing exceptions or so") {
  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();
  Node agency = createAgencyFromBuilder(baseStructure.toBuilder());
  INFO("Agency: " << agency);

  CHECK_NOTHROW(MoveShard(agency, &agent, PENDING, "666"));
}

SECTION("it should be possible to create a new moveshard job") {
  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    REQUIRE(q->slice()[0].length() == 1);

    auto writes = q->slice()[0][0];
    CHECK(writes.length() == 1);
    REQUIRE(std::string(writes.get("/arango/Target/ToDo/1").typeName()) == "object");
    CHECK(writes.get("/arango/Target/ToDo/1").get("database").copyString() == DATABASE);
    CHECK(writes.get("/arango/Target/ToDo/1").get("collection").copyString() == COLLECTION);
    CHECK(writes.get("/arango/Target/ToDo/1").get("shard").copyString() == SHARD);
    CHECK(writes.get("/arango/Target/ToDo/1").get("fromServer").copyString() == SHARD_LEADER);
    CHECK(writes.get("/arango/Target/ToDo/1").get("toServer").copyString() == SHARD_FOLLOWER1);
    CHECK(std::string(writes.get("/arango/Target/ToDo/1").get("timeCreated").typeName()) == "string");

    return fakeWriteResult;
  });
  AgentInterface& agent = mockAgent.get();

  Node agency = createAgencyFromBuilder(baseStructure.toBuilder());
  INFO("Agency: " << agency);

  auto moveShard = MoveShard(agency, &agent, jobId, "hans", DATABASE, COLLECTION, SHARD, SHARD_LEADER, SHARD_FOLLOWER1, true);
  moveShard.create(nullptr);
  Verify(Method(mockAgent,write));
}

SECTION("it should be possible to create a new moveshard job within an envelope") {
  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();

  Node agency = createAgencyFromBuilder(baseStructure.toBuilder());
  INFO("Agency: " << agency);

  auto moveShard = MoveShard(agency, &agent, jobId, "hans", DATABASE, COLLECTION, SHARD, SHARD_LEADER, SHARD_FOLLOWER1, true);

  auto builder = std::make_shared<VPackBuilder>();
  builder->openObject();
  moveShard.create(builder);
  builder->close();

  REQUIRE(std::string(builder->slice().get("/Target/ToDo/1").typeName()) == "object");
}

SECTION("whenever someone tries to create a useless job it should be created in Failed") {
  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();

  Node agency = createAgencyFromBuilder(baseStructure.toBuilder());
  INFO("Agency: " << agency);

  auto moveShard = MoveShard(agency, &agent, jobId, "hans", DATABASE, COLLECTION, SHARD, SHARD_LEADER, SHARD_LEADER, true);
  auto builder = std::make_shared<VPackBuilder>();
  builder->openObject();
  moveShard.create(builder);
  builder->close();

  REQUIRE(std::string(builder->slice().get("/Target/Failed/1").typeName()) == "object");
}

SECTION("when aborting a moveshard job that is moving stuff away from a follower move back everything in place") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        builder->add(jobId, createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER).slice());
      } else if (path == "/arango/Supervision/DBServers") {
        builder->add(FREE_SERVER, VPackValue("1"));
      } else if (path == "/arango/Supervision/Shards") {
        builder->add(SHARD, VPackValue("1"));
      }
      builder->close();
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->close();
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    auto writes = q->slice()[0][0];
    CHECK(writes.get("/arango/Target/Pending/1").get("op").copyString() == "delete");
    REQUIRE(q->slice()[0].length() == 2);
    CHECK(writes.get("/arango/Supervision/DBServers/" + FREE_SERVER).get("op").copyString() == "delete");
    CHECK(writes.get("/arango/Supervision/Shards/" + SHARD).get("op").copyString() == "delete");
    CHECK(std::string(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).typeName()) == "array");
    auto preconditions = q->slice()[0][1];
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION).get("oldEmpty").isFalse());
    // apparently we are not cleaning up our mess. this is done somewhere else :S (>=2)
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).length() >= 2);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[0].copyString() == SHARD_LEADER);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[1].copyString() == SHARD_FOLLOWER1);
    CHECK(std::string(writes.get("/arango/Target/Failed/1").typeName()) == "object");

    return fakeWriteResult;
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.abort("test abort");
  Verify(Method(mockAgent,write));
}

SECTION("if aborting failed report it back properly") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        builder->add(jobId, createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER).slice());
      } else if (path == "/arango/Supervision/DBServers") {
        builder->add(FREE_SERVER, VPackValue("1"));
      } else if (path == "/arango/Supervision/Shards") {
        builder->add(SHARD, VPackValue("1"));
      }
      builder->close();
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->close();
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    return {true, "", std::vector<apply_ret_t> {APPLIED}, std::vector<index_t> {0}};
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  auto result = moveShard.abort("test abort");
  CHECK_FALSE(result.ok());
  CHECK(result.errorNumber() == TRI_ERROR_SUPERVISION_GENERAL_FAILURE);
}

SECTION("if aborting failed due to a precondition report it properly") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        builder->add(jobId, createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER).slice());
      } else if (path == "/arango/Supervision/DBServers") {
        builder->add(FREE_SERVER, VPackValue("1"));
      } else if (path == "/arango/Supervision/Shards") {
        builder->add(SHARD, VPackValue("1"));
      }
      builder->close();
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->close();
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    return {false, "", std::vector<apply_ret_t> {APPLIED}, std::vector<index_t> {1}};
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  auto result = moveShard.abort("test abort");
  CHECK_FALSE(result.ok());
  CHECK(result.errorNumber() == TRI_ERROR_SUPERVISION_GENERAL_FAILURE);
}

SECTION("trying to abort a finished should result in failure") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Finished") {
        builder->add(jobId, createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER).slice());
      } else if (path == "/arango/Supervision/DBServers") {
        builder->add(FREE_SERVER, VPackValue("1"));
      } else if (path == "/arango/Supervision/Shards") {
        builder->add(SHARD, VPackValue("1"));
      }
      builder->close();
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->close();
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    return {false, "", std::vector<apply_ret_t> {APPLIED}, std::vector<index_t> {1}};
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, FINISHED, jobId);
  auto result = moveShard.abort("test abort");
  CHECK_FALSE(result.ok());
  CHECK(result.errorNumber() == TRI_ERROR_SUPERVISION_GENERAL_FAILURE);
}

SECTION("if the job fails while trying to switch over leadership it should be aborted") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        VPackBuilder pendingJob;
        {
          VPackObjectBuilder b(&pendingJob);
          auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
          for (auto const& it: VPackObjectIterator(plainJob.slice())) {
            pendingJob.add(it.key.copyString(), it.value);
          }
          pendingJob.add("timeCreated", VPackValue("2015-01-03T20:00:00Z"));
        }
        builder->add(jobId, pendingJob.slice());
      }
      builder->close();
    } else {
      if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue("_" + SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  Node agency = createAgencyFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  Mock<Job> spy(moveShard);
  Fake(Method(spy, abort));

  Job& spyMoveShard = spy.get();
  spyMoveShard.run(aborts);

  Verify(Method(spy, abort));
}

SECTION("if the job timeouts while the new leader is trying to take over the job should be aborted") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        VPackBuilder pendingJob;
        {
          VPackObjectBuilder b(&pendingJob);
          auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
          for (auto const& it: VPackObjectIterator(plainJob.slice())) {
            pendingJob.add(it.key.copyString(), it.value);
          }
          pendingJob.add("timeCreated", VPackValue("2015-01-03T20:00:00Z"));
        }
        builder->add(jobId, pendingJob.slice());
      }
      builder->close();
    } else {
      if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(FREE_SERVER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  Node agency = createAgencyFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  Mock<Job> spy(moveShard);
  Fake(Method(spy, abort));

  Job& spyMoveShard = spy.get();
  spyMoveShard.run(aborts);

  Verify(Method(spy, abort));
}

SECTION("when promoting the new leader, the old one should become a resigned follower so we can fall back on it if the switch didn't work") {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure = [&](VPackSlice const& s, std::string const& path) {
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

      if (path == "/arango/Target/Pending") {
        VPackBuilder pendingJob;
        {
          VPackObjectBuilder b(&pendingJob);
          auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
          for (auto const& it: VPackObjectIterator(plainJob.slice())) {
            pendingJob.add(it.key.copyString(), it.value);
          }
          pendingJob.add("timeCreated", VPackValue(timepointToString(std::chrono::system_clock::now())));
        }
        builder->add(jobId, pendingJob.slice());
      } else if (path == "/arango/Supervision/DBServers") {
        builder->add(FREE_SERVER, VPackValue("1"));
      } else if (path == "/arango/Supervision/Shards") {
        builder->add(SHARD, VPackValue("1"));
      }
      builder->close();
    } else {
      if (path == "/arango/Current/Collections/" + DATABASE + "/" + COLLECTION + "/" + SHARD + "/servers") {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue("_" + SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue("_" + SHARD_LEADER));
        builder->add(VPackValue(SHARD_FOLLOWER1));
        builder->add(VPackValue(FREE_SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WriteTransaction: " << q->slice().toJson());
    REQUIRE(q->slice()[0].length() == 2);

    auto writes = q->slice()[0][0];
    REQUIRE(std::string(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).typeName()) == "array");
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).length() == 3);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[0].copyString() == FREE_SERVER);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[1].copyString() == SHARD_FOLLOWER1);
    CHECK(writes.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)[2].copyString() == SHARD_LEADER);

    auto preconditions = q->slice()[0][1];
    REQUIRE(std::string(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).typeName()) == "object");
    CHECK(std::string(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old").typeName()) == "array");
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[0].copyString() == "_" + SHARD_LEADER);
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[1].copyString() == SHARD_FOLLOWER1);
    CHECK(preconditions.get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD).get("old")[2].copyString() == FREE_SERVER);

    return fakeWriteResult;
  });
  // nothing should happen so nothing should be called
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  REQUIRE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  INFO("Agency: " << agency);
  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
  Verify(Method(mockAgent, write));
}

}
}
}
}
