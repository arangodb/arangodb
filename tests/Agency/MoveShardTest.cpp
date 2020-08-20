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

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Mocks/LogLevels.h"

#include "Agency/AgentInterface.h"
#include "Agency/MoveShard.h"
#include "Agency/Node.h"

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
const std::string SHARD_FOLLOWER2 = "follower2";
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

const char* agency =
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

#define CHECK_FAILURE(source, query)                                           \
  std::string sourceKey = "/arango/Target/";                                   \
  sourceKey += source;                                                         \
  sourceKey += "/1";                                                           \
  EXPECT_EQ(std::string(q->slice().typeName()), "array");                  \
  EXPECT_EQ(q->slice().length(), 1);                                       \
  EXPECT_EQ(std::string(q->slice()[0].typeName()), "array");               \
  EXPECT_EQ(q->slice()[0].length(), 1);                                    \
  EXPECT_EQ(std::string(q->slice()[0][0].typeName()), "object");           \
  auto writes = q->slice()[0][0];                                              \
  EXPECT_EQ(std::string(writes.get(sourceKey).typeName()), "object");      \
  EXPECT_TRUE(std::string(writes.get(sourceKey).get("op").typeName()) ==       \
              "string");                                                       \
  EXPECT_EQ(writes.get(sourceKey).get("op").copyString(), "delete");       \
  EXPECT_TRUE(std::string(writes.get("/arango/Target/Failed/1").typeName()) == \
              "object");

Node createRootNode() {
  VPackOptions options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(agency);

  VPackBuilder builder;
  {
    VPackObjectBuilder a(&builder);
    builder.add("new", parser.steal()->slice());
  }

  Node root("ROOT");
  root.handle<SET>(builder.slice());
  return root;
}

VPackBuilder createJob(std::string const& collection, std::string const& from,
                       std::string const& to) {
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

class MoveShardTest : public ::testing::Test,
                      public LogSuppressor<Logger::SUPERVISION, LogLevel::FATAL> {
 protected:
  Node baseStructure;
  write_ret_t fakeWriteResult;
  std::string const jobId;

  MoveShardTest()
      : baseStructure(createRootNode()),
        fakeWriteResult(true, "", std::vector<apply_ret_t>{APPLIED},
                        std::vector<index_t>{1}),
        jobId("1") {}
};

TEST_F(MoveShardTest, the_job_should_fail_if_toserver_does_not_exist) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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
    CHECK_FAILURE("ToDo", q);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, the_job_should_fail_if_servers_are_planned_followers) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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
            builder->add(jobId,
                         createJob(COLLECTION, SHARD_FOLLOWER1, SHARD_LEADER).slice());
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
    CHECK_FAILURE("ToDo", q);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, the_job_should_fail_if_fromserver_does_not_exist) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  Mock<Job> spy(moveShard);
  Fake(Method(spy, finish));

  Job& spyMoveShard = spy.get();
  spyMoveShard.start(aborts);

  Verify(Method(spy, finish)
             .Matching([](std::string const& server, std::string const& shard,
                          bool success, std::string const& reason,
                          query_t const payload) -> bool { return !success; }));
}

TEST_F(MoveShardTest, the_job_should_fail_if_fromserver_is_not_in_plan) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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
    CHECK_FAILURE("ToDo", q);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, the_job_should_fail_if_fromserver_does_not_exist_2) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, the_job_should_remain_in_todo_if_shard_is_locked) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
}

TEST_F(MoveShardTest, the_job_should_remain_in_todo_if_server_is_locked) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
}

TEST_F(MoveShardTest, the_job_should_fail_if_target_server_was_cleaned_out) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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
    CHECK_FAILURE("ToDo", q);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, the_job_should_fail_if_the_target_server_is_failed) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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
    CHECK_FAILURE("ToDo", q);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, the_job_should_wait_until_the_target_server_is_good) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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
  CHECK_FAILURE("ToDo", q);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
}

TEST_F(MoveShardTest, the_job_should_fail_if_the_shard_distributes_its_shards_like_some_other) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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
    CHECK_FAILURE("ToDo", q);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, the_job_should_be_moved_to_pending_when_everything_is_ok) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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
    std::string sourceKey = "/arango/Target/ToDo/1";
    EXPECT_EQ(std::string(q->slice().typeName()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(std::string(q->slice()[0].typeName()), "array");
    EXPECT_EQ(q->slice()[0].length(), 2);
    EXPECT_EQ(std::string(q->slice()[0][0].typeName()), "object");
    EXPECT_EQ(std::string(q->slice()[0][1].typeName()), "object");

    auto writes = q->slice()[0][0];
    EXPECT_EQ(std::string(writes.get(sourceKey).typeName()), "object");
    EXPECT_TRUE(std::string(writes.get(sourceKey).get("op").typeName()) ==
                "string");
    EXPECT_EQ(writes.get(sourceKey).get("op").copyString(), "delete");
    EXPECT_TRUE(writes.get("/arango/Supervision/Shards/" + SHARD).copyString() ==
                "1");
    EXPECT_TRUE(writes.get("/arango/Supervision/DBServers/" + FREE_SERVER).get("op").isEqualString("read-lock"));
    EXPECT_TRUE(writes.get("/arango/Supervision/DBServers/" + FREE_SERVER).get("by").isEqualString("1"));
    EXPECT_TRUE(writes.get("/arango/Plan/Version").get("op").copyString() ==
                "increment");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Pending/1").typeName()) ==
                "object");
    EXPECT_TRUE(
        std::string(writes.get("/arango/Target/Pending/1").get("timeStarted").typeName()) ==
        "string");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .length() == 3);  // leader, oldFollower, newLeader
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[0]
                    .copyString() == SHARD_LEADER);

    // order not really relevant ... assume it might appear anyway
    auto followers = writes.get("/arango/Plan/Collections/" + DATABASE + "/" +
                                COLLECTION + "/shards/" + SHARD);
    bool found = false;
    for (auto const& server : VPackArrayIterator(followers)) {
      if (server.copyString() == FREE_SERVER) {
        found = true;
      }
    }
    EXPECT_TRUE(found);

    auto preconditions = q->slice()[0][1];
    EXPECT_TRUE(preconditions.get("/arango/Target/CleanedServers").get("old").toJson() ==
                "[]");
    EXPECT_TRUE(preconditions.get("/arango/Target/FailedServers").get("old").toJson() ==
                "{}");
    EXPECT_TRUE(preconditions
                    .get("/arango/Supervision/Health/" + FREE_SERVER + "/Status")
                    .get("old")
                    .copyString() == "GOOD");
    EXPECT_TRUE(
        preconditions.get("/arango/Supervision/DBServers/" + FREE_SERVER)
            .get("can-read-lock")
            .isEqualString("1"));
    EXPECT_TRUE(
        preconditions.get("/arango/Supervision/Shards/" + SHARD).get("oldEmpty").getBool() == true);
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .get("old")
                    .toJson() ==
                "[\"" + SHARD_LEADER + "\",\"" + SHARD_FOLLOWER1 + "\"]");

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, moving_from_a_follower_should_be_possible) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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
            builder->add(jobId,
                         createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER).slice());
          }
          builder->close();
        } else {
          builder->add(s);
        }
        return builder;
      };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    auto writes = q->slice()[0][0];
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .length() == 3);  // leader, oldFollower, newLeader
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[0]
                    .copyString() == SHARD_LEADER);

    // order not really relevant ... assume it might appear anyway
    auto followers = writes.get("/arango/Plan/Collections/" + DATABASE + "/" +
                                COLLECTION + "/shards/" + SHARD);
    bool found = false;
    for (auto const& server : VPackArrayIterator(followers)) {
      if (server.copyString() == FREE_SERVER) {
        found = true;
      }
    }
    EXPECT_TRUE(found);
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, when_moving_a_shard_that_is_a_distributeshardslike_leader_move_the_rest_as_well) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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
    auto writes = q->slice()[0][0];
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .length() == 3);  // leader, oldFollower, newLeader

    auto json = writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)
                    .toJson();
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection1/shards/s100")
                    .toJson() == json);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection2/shards/s101")
                    .toJson() == json);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/unrelatedcollection/shards/unrelatedshard")
                    .isNone());
    EXPECT_TRUE(writes.get("/arango/Supervision/Shards/" + SHARD).copyString() ==
                "1");
    EXPECT_TRUE(writes.get("/arango/Supervision/Shards/unrelatedshard").isNone());

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn();

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.start(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, if_the_job_is_too_old_it_should_be_aborted_to_prevent_a_deadloop) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue("2015-01-03T20:00:00Z"));
            }
            builder->add(jobId, pendingJob.slice());
          }
          builder->close();
        } else {
          if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                          COLLECTION + "/shards/" + SHARD) {
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

TEST_F(MoveShardTest, if_the_to_server_no_longer_replica_we_should_abort) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob(COLLECTION, SHARD_LEADER, SHARD_FOLLOWER1);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue("2015-01-03T20:00:00Z"));
            }
            builder->add(jobId, pendingJob.slice());
          }
          builder->close();
        } else {
          if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                          COLLECTION + "/shards/" + SHARD) {
            builder->add(VPackValue(VPackValueType::Array));
            builder->add(VPackValue("_" + SHARD_LEADER));
            builder->add(VPackValue(SHARD_FOLLOWER1));
            builder->add(VPackValue(SHARD_FOLLOWER2));
            builder->close();
          } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                          COLLECTION + "/" + SHARD + "/servers") {
            builder->add(VPackValue(VPackValueType::Array));
            builder->add(VPackValue("_" + SHARD_LEADER));
            builder->add(VPackValue(SHARD_FOLLOWER2));
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

TEST_F(MoveShardTest, if_the_job_is_too_old_leader_case_it_should_be_aborted_to_prevent_deadloop) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue("2015-01-03T20:00:00Z"));
            }
            builder->add(jobId, pendingJob.slice());
          }
          builder->close();
        } else {
          if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                          COLLECTION + "/shards/" + SHARD) {
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

TEST_F(MoveShardTest, if_the_collection_was_dropped_while_moving_finish_the_job) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob("BOGUS", SHARD_FOLLOWER1, FREE_SERVER);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue(timepointToString(
                                                std::chrono::system_clock::now())));
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

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  Mock<Job> spy(moveShard);
  Fake(Method(spy, finish));

  Job& spyMoveShard = spy.get();
  spyMoveShard.run(aborts);

  Verify(Method(spy, finish)
             .Matching([](std::string const& server, std::string const& shard,
                          bool success, std::string const& reason,
                          query_t const payload) -> bool { return success; }));
}

TEST_F(MoveShardTest, if_the_collection_was_dropped_before_the_job_could_be_started_just_finish_the_job) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
        std::unique_ptr<VPackBuilder> builder;
        builder.reset(new VPackBuilder());
        if (s.isObject()) {
          builder->add(VPackValue(VPackValueType::Object));
          for (auto it : VPackObjectIterator(s)) {
            auto childBuilder =
                createTestStructure(it.value, path + "/" + it.key.copyString());
            if (childBuilder && !(path == "/arango/Plan/Collections/" + DATABASE &&
                                  it.key.copyString() == COLLECTION)) {
              builder->add(it.key.copyString(), childBuilder->slice());
            }
          }

          if (path == "/arango/Target/ToDo") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob("ANUNKNOWNCOLLECTION", SHARD_FOLLOWER1, FREE_SERVER);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue(timepointToString(
                                                std::chrono::system_clock::now())));
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

  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  Mock<Job> spy(moveShard);
  Fake(Method(spy, finish));

  Job& spyMoveShard = spy.get();
  spyMoveShard.start(aborts);

  Verify(Method(spy, finish)
             .Matching([](std::string const& server, std::string const& shard,
                          bool success, std::string const& reason,
                          query_t const payload) -> bool { return success; }));
}

TEST_F(MoveShardTest, the_job_should_wait_until_the_planned_shard_situation_has_been_created_in_current) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue(timepointToString(
                                                std::chrono::system_clock::now())));
            }
            builder->add(jobId, pendingJob.slice());
          }
          builder->close();
        } else {
          if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                          COLLECTION + "/shards/" + SHARD) {
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

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
}

TEST_F(MoveShardTest, if_the_job_is_done_it_should_properly_finish_itself) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue(timepointToString(
                                                std::chrono::system_clock::now())));
            }
            builder->add(jobId, pendingJob.slice());
          }
          builder->close();
        } else {
          if (path == "/arango/Current/Collections/" + DATABASE + "/" +
                          COLLECTION + "/" + SHARD + "/servers") {
            builder->add(VPackValue(VPackValueType::Array));
            builder->add(VPackValue(SHARD_LEADER));
            builder->add(VPackValue(SHARD_FOLLOWER1));
            builder->add(VPackValue(FREE_SERVER));
            builder->close();
          } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                                 COLLECTION + "/shards/" + SHARD) {
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
    auto writes = q->slice()[0][0];
    EXPECT_TRUE(writes.get("/arango/Target/Pending/1").get("op").copyString() ==
                "delete");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Finished/1").typeName()) ==
                "object");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .toJson() == "[\"leader\",\"free\"]");
    EXPECT_TRUE(writes.get("/arango/Supervision/Shards/" + SHARD).get("op").copyString() ==
                "delete");
    EXPECT_TRUE(
        writes.get("/arango/Supervision/DBServers/" + FREE_SERVER).get("op").isEqualString("read-unlock"));

    auto preconditions = q->slice()[0][1];
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .get("old")
                    .length() == 3);

    return fakeWriteResult;
  });
  AgentInterface& agent = mockAgent.get();

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, the_job_should_not_finish_itself_when_only_parts_of_distributeshardslike_have_been_adopted) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue(timepointToString(
                                                std::chrono::system_clock::now())));
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
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
}

TEST_F(MoveShardTest, the_job_should_finish_when_all_distributeshardslike_shards_have_adapted) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
        std::unique_ptr<VPackBuilder> builder;
        builder.reset(new VPackBuilder());
        if (s.isObject()) {
          builder->add(VPackValue(VPackValueType::Object));
          for (auto it : VPackObjectIterator(s)) {
            auto childBuilder =
                createTestStructure(it.value, path + "/" + it.key.copyString());
            if (childBuilder && it.key.copyString() != COLLECTION &&
                path != "/arango/Current/Collections/" + DATABASE) {
              builder->add(it.key.copyString(), childBuilder->slice());
            }
          }

          if (path == "/arango/Target/Pending") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue(timepointToString(
                                                std::chrono::system_clock::now())));
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
    auto writes = q->slice()[0][0];
    EXPECT_TRUE(writes.get("/arango/Target/Pending/1").get("op").copyString() ==
                "delete");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Finished/1").typeName()) ==
                "object");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .toJson() == "[\"leader\",\"free\"]");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection1/shards/s100")
                    .toJson() == "[\"leader\",\"free\"]");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection2/shards/s101")
                    .toJson() == "[\"leader\",\"free\"]");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/unrelatedcollection/shards/unrelatedshard")
                    .isNone());
    EXPECT_TRUE(writes.get("/arango/Supervision/Shards/s100").isNone());

    auto preconditions = q->slice()[0][1];
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .get("old")
                    .length() == 3);
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection1/shards/s100")
                    .get("old")
                    .length() == 3);
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/linkedcollection2/shards/s101")
                    .get("old")
                    .length() == 3);
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/unrelatedcollection/shards/unrelatedshard")
                    .isNone());

    return fakeWriteResult;
  });
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, a_moveshard_job_that_just_made_it_to_todo_can_simply_be_aborted) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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
    EXPECT_EQ(q->slice()[0].length(), 2);  // we always simply override! no preconditions...
    auto writes = q->slice()[0][0];
    EXPECT_TRUE(writes.get("/arango/Target/ToDo/1").get("op").copyString() ==
                "delete");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Finished/1").typeName()) ==
                "object");
    auto precond = q->slice()[0][1];
    EXPECT_TRUE(precond.get("/arango/Target/ToDo/1").get("oldEmpty").isFalse());

    return fakeWriteResult;
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, TODO, jobId);
  moveShard.abort("test abort");
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, a_pending_moveshard_job_should_also_put_the_original_server_back_into_place_when_aborted) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            builder->add(jobId, createJob(COLLECTION, SHARD_LEADER, FREE_SERVER).slice());
          } else if (path == "/arango/Supervision/DBServers") {
            builder->add(FREE_SERVER, VPackValue("1"));
          } else if (path == "/arango/Supervision/Shards") {
            builder->add(SHARD, VPackValue("1"));
          }
          builder->close();
        } else {
          if (path == "/arango/Current/Collections/" + DATABASE + "/" +
                          COLLECTION + "/" + SHARD + "/servers") {
            builder->add(VPackValue(VPackValueType::Array));
            builder->add(VPackValue(SHARD_LEADER));
            builder->add(VPackValue(SHARD_FOLLOWER1));
            builder->close();
          } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                                 COLLECTION + "/shards/" + SHARD) {
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
    auto writes = q->slice()[0][0];
    EXPECT_TRUE(writes.get("/arango/Target/Pending/1").get("op").copyString() ==
                "delete");
    EXPECT_EQ(q->slice()[0].length(), 2);  // Precondition: to Server not leader yet
    EXPECT_TRUE(
        writes.get("/arango/Supervision/DBServers/" + FREE_SERVER).get("op").isEqualString("read-unlock"));
    EXPECT_TRUE(writes.get("/arango/Supervision/Shards/" + SHARD).get("op").copyString() ==
                "delete");
    EXPECT_TRUE(std::string(writes
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)
                                .typeName()) == "array");
    // apparently we are not cleaning up our mess. this is done somewhere else :S (>=2)
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .length() >= 2);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[0]
                    .copyString() == SHARD_LEADER);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[1]
                    .copyString() == SHARD_FOLLOWER1);
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Failed/1").typeName()) ==
                "object");

    return fakeWriteResult;
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.abort("test abort");
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, after_the_new_leader_has_synchronized_the_new_leader_should_resign) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue(timepointToString(
                                                std::chrono::system_clock::now())));
            }
            builder->add(jobId, pendingJob.slice());
          } else if (path == "/arango/Supervision/DBServers") {
            builder->add(FREE_SERVER, VPackValue("1"));
          } else if (path == "/arango/Supervision/Shards") {
            builder->add(SHARD, VPackValue("1"));
          }
          builder->close();
        } else {
          if (path == "/arango/Current/Collections/" + DATABASE + "/" +
                          COLLECTION + "/" + SHARD + "/servers") {
            builder->add(VPackValue(VPackValueType::Array));
            builder->add(VPackValue(SHARD_LEADER));
            builder->add(VPackValue(SHARD_FOLLOWER1));
            builder->add(VPackValue(FREE_SERVER));
            builder->close();
          } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                                 COLLECTION + "/shards/" + SHARD) {
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
    auto writes = q->slice()[0][0];
    EXPECT_TRUE(std::string(writes
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)
                                .typeName()) == "array");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .length() == 3);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[0]
                    .copyString() == "_" + SHARD_LEADER);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[1]
                    .copyString() == SHARD_FOLLOWER1);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[2]
                    .copyString() == FREE_SERVER);

    EXPECT_EQ(q->slice()[0].length(), 2);
    auto preconditions = q->slice()[0][1];
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
                    .copyString() == FREE_SERVER);
    return fakeWriteResult;
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, when_the_old_leader_is_not_yet_ready_for_resign_nothing_should_happen) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue(timepointToString(
                                                std::chrono::system_clock::now())));
            }
            builder->add(jobId, pendingJob.slice());
          } else if (path == "/arango/Supervision/DBServers") {
            builder->add(FREE_SERVER, VPackValue("1"));
          } else if (path == "/arango/Supervision/Shards") {
            builder->add(SHARD, VPackValue("1"));
          }
          builder->close();
        } else {
          if (path == "/arango/Current/Collections/" + DATABASE + "/" +
                          COLLECTION + "/" + SHARD + "/servers") {
            builder->add(VPackValue(VPackValueType::Array));
            builder->add(VPackValue(SHARD_LEADER));
            builder->add(VPackValue(SHARD_FOLLOWER1));
            builder->add(VPackValue(FREE_SERVER));
            builder->close();
          } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                                 COLLECTION + "/shards/" + SHARD) {
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
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
}

TEST_F(MoveShardTest, aborting_the_job_while_a_leader_transition_is_in_progress_should_make_the_old_leader_leader_again) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue(timepointToString(
                                                std::chrono::system_clock::now())));
            }
            builder->add(jobId, pendingJob.slice());
          } else if (path == "/arango/Supervision/DBServers") {
            builder->add(FREE_SERVER, VPackValue("1"));
          } else if (path == "/arango/Supervision/Shards") {
            builder->add(SHARD, VPackValue("1"));
          }
          builder->close();
        } else {
          if (path == "/arango/Current/Collections/" + DATABASE + "/" +
                          COLLECTION + "/" + SHARD + "/servers") {
            builder->add(VPackValue(VPackValueType::Array));
            builder->add(VPackValue(SHARD_LEADER));
            builder->add(VPackValue(SHARD_FOLLOWER1));
            builder->add(VPackValue(FREE_SERVER));
            builder->close();
          } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                                 COLLECTION + "/shards/" + SHARD) {
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
    auto writes = q->slice()[0][0];
    EXPECT_TRUE(writes.get("/arango/Target/Pending/1").get("op").copyString() ==
                "delete");
    EXPECT_EQ(q->slice()[0].length(), 2);  // Precondition: to Server not leader yet
    EXPECT_TRUE(
        writes.get("/arango/Supervision/DBServers/" + FREE_SERVER).get("op").isEqualString("read-unlock"));
    EXPECT_TRUE(writes.get("/arango/Supervision/Shards/" + SHARD).get("op").copyString() ==
                "delete");
    EXPECT_TRUE(std::string(writes
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)
                                .typeName()) == "array");
    // well apparently this job is not responsible to cleanup its mess
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .length() >= 2);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[0]
                    .copyString() == SHARD_LEADER);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[1]
                    .copyString() == SHARD_FOLLOWER1);
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Failed/1").typeName()) ==
                "object");

    return fakeWriteResult;
  });
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.abort("test abort");
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, aborting_the_job_while_the_new_leader_is_already_in_place_should_not_break_plan) {
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
        builder->close();
      } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD) {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(FREE_SERVER));
        builder->add(VPackValue(SHARD_LEADER));
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

    auto writes = q->slice()[0][0];
    EXPECT_EQ(writes.get("/arango/Target/Pending/1").get("op").copyString(), "delete");
    EXPECT_EQ(q->slice()[0].length(), 2); // Precondition: to Server not leader yet
    EXPECT_EQ(writes.get("/arango/Supervision/Shards/" + SHARD).get("op").copyString(), "delete");
    EXPECT_TRUE(writes.get("/arango/Supervision/DBServers/" + FREE_SERVER).get("op").isEqualString("read-unlock"));
    // well apparently this job is not responsible to cleanup its mess
    EXPECT_EQ(std::string(writes.get("/arango/Target/Failed/1").typeName()), "object");
    return fakeWriteResult;
  });
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  EXPECT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.abort("test abort");
  Verify(Method(mockAgent,write));
}

TEST_F(MoveShardTest, if_we_are_ready_to_resign_the_old_server_then_finally_move_to_the_new_leader) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue(timepointToString(
                                                std::chrono::system_clock::now())));
            }
            builder->add(jobId, pendingJob.slice());
          } else if (path == "/arango/Supervision/DBServers") {
            builder->add(FREE_SERVER, VPackValue("1"));
          } else if (path == "/arango/Supervision/Shards") {
            builder->add(SHARD, VPackValue("1"));
          }
          builder->close();
        } else {
          if (path == "/arango/Current/Collections/" + DATABASE + "/" +
                          COLLECTION + "/" + SHARD + "/servers") {
            builder->add(VPackValue(VPackValueType::Array));
            builder->add(VPackValue("_" + SHARD_LEADER));
            builder->add(VPackValue(SHARD_FOLLOWER1));
            builder->add(VPackValue(FREE_SERVER));
            builder->close();
          } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                                 COLLECTION + "/shards/" + SHARD) {
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
    auto writes = q->slice()[0][0];
    EXPECT_TRUE(std::string(writes
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)
                                .typeName()) == "array");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .length() == 3);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[0]
                    .copyString() == FREE_SERVER);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[1]
                    .copyString() == SHARD_FOLLOWER1);

    EXPECT_EQ(q->slice()[0].length(), 2);
    auto preconditions = q->slice()[0][1];
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
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .get("old")[0]
                    .copyString() == "_" + SHARD_LEADER);
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .get("old")[1]
                    .copyString() == SHARD_FOLLOWER1);
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .get("old")[2]
                    .copyString() == FREE_SERVER);
    return fakeWriteResult;
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, if_the_new_leader_took_over_finish_the_job) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue(timepointToString(
                                                std::chrono::system_clock::now())));
            }
            builder->add(jobId, pendingJob.slice());
          } else if (path == "/arango/Supervision/DBServers") {
            builder->add(FREE_SERVER, VPackValue("1"));
          } else if (path == "/arango/Supervision/Shards") {
            builder->add(SHARD, VPackValue("1"));
          }
          builder->close();
        } else {
          if (path == "/arango/Current/Collections/" + DATABASE + "/" +
                          COLLECTION + "/" + SHARD + "/servers") {
            builder->add(VPackValue(VPackValueType::Array));
            builder->add(VPackValue(FREE_SERVER));
            builder->add(VPackValue(SHARD_FOLLOWER1));
            builder->close();
          } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                                 COLLECTION + "/shards/" + SHARD) {
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
    auto writes = q->slice()[0][0];

    EXPECT_EQ(writes.length(), 5);
    EXPECT_TRUE(writes.get("/arango/Target/Pending/1").get("op").copyString() ==
                "delete");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Finished/1").typeName()) ==
                "object");
    EXPECT_TRUE(
        writes.get("/arango/Supervision/DBServers/" + FREE_SERVER).get("op").isEqualString("read-unlock"));
    EXPECT_TRUE(writes.get("/arango/Supervision/Shards/" + SHARD).get("op").copyString() ==
                "delete");

    EXPECT_EQ(q->slice()[0].length(), 2);
    auto preconditions = q->slice()[0][1];
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
                    .length() == 2);
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .get("old")[0]
                    .copyString() == FREE_SERVER);
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .get("old")[1]
                    .copyString() == SHARD_FOLLOWER1);
    return fakeWriteResult;
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, calling_an_unknown_job_should_be_possible_without_throwing_exceptions) {
  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();
  Node agency = createAgencyFromBuilder(baseStructure.toBuilder());

  EXPECT_NO_THROW(MoveShard(agency, &agent, PENDING, "666"));
}

TEST_F(MoveShardTest, it_should_be_possible_to_create_a_new_moveshard_job) {
  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, waitFor)).AlwaysReturn();
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_EQ(q->slice()[0].length(), 1);

    auto writes = q->slice()[0][0];
    EXPECT_EQ(writes.length(), 1);
    EXPECT_TRUE(std::string(writes.get("/arango/Target/ToDo/1").typeName()) ==
                "object");
    EXPECT_EQ(writes.get("/arango/Target/ToDo/1").get("database").copyString(), DATABASE);
    EXPECT_EQ(writes.get("/arango/Target/ToDo/1").get("collection").copyString(), COLLECTION);
    EXPECT_EQ(writes.get("/arango/Target/ToDo/1").get("shard").copyString(), SHARD);
    EXPECT_EQ(writes.get("/arango/Target/ToDo/1").get("fromServer").copyString(), SHARD_LEADER);
    EXPECT_EQ(writes.get("/arango/Target/ToDo/1").get("toServer").copyString(), SHARD_FOLLOWER1);
    EXPECT_TRUE(
        std::string(writes.get("/arango/Target/ToDo/1").get("timeCreated").typeName()) ==
        "string");

    return fakeWriteResult;
  });
  AgentInterface& agent = mockAgent.get();

  Node agency = createAgencyFromBuilder(baseStructure.toBuilder());

  auto moveShard = MoveShard(agency, &agent, jobId, "hans", DATABASE, COLLECTION,
                             SHARD, SHARD_LEADER, SHARD_FOLLOWER1, true);
  moveShard.create(nullptr);
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, it_should_be_possible_to_create_a_new_moveshard_job_within_an_envelope) {
  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();

  Node agency = createAgencyFromBuilder(baseStructure.toBuilder());

  auto moveShard = MoveShard(agency, &agent, jobId, "hans", DATABASE, COLLECTION,
                             SHARD, SHARD_LEADER, SHARD_FOLLOWER1, true);

  auto builder = std::make_shared<VPackBuilder>();
  builder->openObject();
  moveShard.create(builder);
  builder->close();

  ASSERT_TRUE(std::string(builder->slice().get("/Target/ToDo/1").typeName()) ==
              "object");
}

TEST_F(MoveShardTest, whenever_someone_tries_to_create_a_useless_job_it_should_be_created_in_failed) {
  Mock<AgentInterface> mockAgent;
  AgentInterface& agent = mockAgent.get();

  Node agency = createAgencyFromBuilder(baseStructure.toBuilder());

  auto moveShard = MoveShard(agency, &agent, jobId, "hans", DATABASE, COLLECTION,
                             SHARD, SHARD_LEADER, SHARD_LEADER, true);
  auto builder = std::make_shared<VPackBuilder>();
  builder->openObject();
  moveShard.create(builder);
  builder->close();

  ASSERT_TRUE(std::string(builder->slice().get("/Target/Failed/1").typeName()) ==
              "object");
}

TEST_F(MoveShardTest, when_aborting_a_moveshard_job_that_is_moving_stuff_away_from_a_follower_move_back_everything_in_place) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            builder->add(jobId,
                         createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER).slice());
          } else if (path == "/arango/Supervision/DBServers") {
            builder->add(FREE_SERVER, VPackValue("1"));
          } else if (path == "/arango/Supervision/Shards") {
            builder->add(SHARD, VPackValue("1"));
          }
          builder->close();
        } else {
          if (path == "/arango/Current/Collections/" + DATABASE + "/" +
                          COLLECTION + "/" + SHARD + "/servers") {
            builder->add(VPackValue(VPackValueType::Array));
            builder->add(VPackValue(SHARD_LEADER));
            builder->add(VPackValue(SHARD_FOLLOWER1));
            builder->close();
          } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                                 COLLECTION + "/shards/" + SHARD) {
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
    auto writes = q->slice()[0][0];
    EXPECT_TRUE(writes.get("/arango/Target/Pending/1").get("op").copyString() ==
                "delete");
    EXPECT_EQ(q->slice()[0].length(), 2);
    auto preconditions = q->slice()[0][1];
    EXPECT_TRUE(preconditions.get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION).get("oldEmpty").isFalse());
    EXPECT_TRUE(
        writes.get("/arango/Supervision/DBServers/" + FREE_SERVER).get("op").isEqualString("read-unlock"));
    EXPECT_TRUE(writes.get("/arango/Supervision/Shards/" + SHARD).get("op").copyString() ==
                "delete");
    EXPECT_TRUE(std::string(writes
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)
                                .typeName()) == "array");
    // apparently we are not cleaning up our mess. this is done somewhere else :S (>=2)
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .length() >= 2);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[0]
                    .copyString() == SHARD_LEADER);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[1]
                    .copyString() == SHARD_FOLLOWER1);
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Failed/1").typeName()) ==
                "object");

    return fakeWriteResult;
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.abort("test abort");
  Verify(Method(mockAgent, write));
}

TEST_F(MoveShardTest, if_aborting_failed_report_it_back_properly) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            builder->add(jobId,
                         createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER).slice());
          } else if (path == "/arango/Supervision/DBServers") {
            builder->add(FREE_SERVER, VPackValue("1"));
          } else if (path == "/arango/Supervision/Shards") {
            builder->add(SHARD, VPackValue("1"));
          }
          builder->close();
        } else {
          if (path == "/arango/Current/Collections/" + DATABASE + "/" +
                          COLLECTION + "/" + SHARD + "/servers") {
            builder->add(VPackValue(VPackValueType::Array));
            builder->add(VPackValue(SHARD_LEADER));
            builder->add(VPackValue(SHARD_FOLLOWER1));
            builder->close();
          } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                                 COLLECTION + "/shards/" + SHARD) {
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
    return {true, "", std::vector<apply_ret_t>{APPLIED}, std::vector<index_t>{0}};
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  auto result = moveShard.abort("test abort");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.errorNumber(), TRI_ERROR_SUPERVISION_GENERAL_FAILURE);
}

TEST_F(MoveShardTest, if_aborting_failed_due_to_a_precondition_report_it_properly) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            builder->add(jobId,
                         createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER).slice());
          } else if (path == "/arango/Supervision/DBServers") {
            builder->add(FREE_SERVER, VPackValue("1"));
          } else if (path == "/arango/Supervision/Shards") {
            builder->add(SHARD, VPackValue("1"));
          }
          builder->close();
        } else {
          if (path == "/arango/Current/Collections/" + DATABASE + "/" +
                          COLLECTION + "/" + SHARD + "/servers") {
            builder->add(VPackValue(VPackValueType::Array));
            builder->add(VPackValue(SHARD_LEADER));
            builder->add(VPackValue(SHARD_FOLLOWER1));
            builder->close();
          } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                                 COLLECTION + "/shards/" + SHARD) {
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
    return {false, "", std::vector<apply_ret_t>{APPLIED}, std::vector<index_t>{1}};
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  auto result = moveShard.abort("test abort");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.errorNumber(), TRI_ERROR_SUPERVISION_GENERAL_FAILURE);
}

TEST_F(MoveShardTest, trying_to_abort_a_finished_should_result_in_failure) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Finished") {
            builder->add(jobId,
                         createJob(COLLECTION, SHARD_FOLLOWER1, FREE_SERVER).slice());
          } else if (path == "/arango/Supervision/DBServers") {
            builder->add(FREE_SERVER, VPackValue("1"));
          } else if (path == "/arango/Supervision/Shards") {
            builder->add(SHARD, VPackValue("1"));
          }
          builder->close();
        } else {
          if (path == "/arango/Current/Collections/" + DATABASE + "/" +
                          COLLECTION + "/" + SHARD + "/servers") {
            builder->add(VPackValue(VPackValueType::Array));
            builder->add(VPackValue(SHARD_LEADER));
            builder->add(VPackValue(SHARD_FOLLOWER1));
            builder->close();
          } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                                 COLLECTION + "/shards/" + SHARD) {
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
    return {false, "", std::vector<apply_ret_t>{APPLIED}, std::vector<index_t>{1}};
  });

  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, FINISHED, jobId);
  auto result = moveShard.abort("test abort");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.errorNumber(), TRI_ERROR_SUPERVISION_GENERAL_FAILURE);
}

TEST_F(MoveShardTest, if_the_job_fails_while_trying_to_switch_over_leadership_it_should_be_aborted) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue("2015-01-03T20:00:00Z"));
            }
            builder->add(jobId, pendingJob.slice());
          }
          builder->close();
        } else {
          if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                          COLLECTION + "/shards/" + SHARD) {
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

TEST_F(MoveShardTest, if_the_job_timeouts_while_the_new_leader_is_trying_to_take_over_the_job_should_be_aborted) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue("2015-01-03T20:00:00Z"));
            }
            builder->add(jobId, pendingJob.slice());
          }
          builder->close();
        } else {
          if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                          COLLECTION + "/shards/" + SHARD) {
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

TEST_F(MoveShardTest, when_promoting_the_new_leader_the_old_one_should_become_a_resigned_follower_so_we_can_fall_back_on_it_if_the_switch_didnt_work) {
  std::function<std::unique_ptr<VPackBuilder>(VPackSlice const&, std::string const&)> createTestStructure =
      [&](VPackSlice const& s, std::string const& path) {
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

          if (path == "/arango/Target/Pending") {
            VPackBuilder pendingJob;
            {
              VPackObjectBuilder b(&pendingJob);
              auto plainJob = createJob(COLLECTION, SHARD_LEADER, FREE_SERVER);
              for (auto it : VPackObjectIterator(plainJob.slice())) {
                pendingJob.add(it.key.copyString(), it.value);
              }
              pendingJob.add("timeCreated", VPackValue(timepointToString(
                                                std::chrono::system_clock::now())));
            }
            builder->add(jobId, pendingJob.slice());
          } else if (path == "/arango/Supervision/DBServers") {
            builder->add(FREE_SERVER, VPackValue("1"));
          } else if (path == "/arango/Supervision/Shards") {
            builder->add(SHARD, VPackValue("1"));
          }
          builder->close();
        } else {
          if (path == "/arango/Current/Collections/" + DATABASE + "/" +
                          COLLECTION + "/" + SHARD + "/servers") {
            builder->add(VPackValue(VPackValueType::Array));
            builder->add(VPackValue("_" + SHARD_LEADER));
            builder->add(VPackValue(SHARD_FOLLOWER1));
            builder->add(VPackValue(FREE_SERVER));
            builder->close();
          } else if (path == "/arango/Plan/Collections/" + DATABASE + "/" +
                                 COLLECTION + "/shards/" + SHARD) {
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
    EXPECT_EQ(q->slice()[0].length(), 2);

    auto writes = q->slice()[0][0];
    EXPECT_TRUE(std::string(writes
                                .get("/arango/Plan/Collections/" + DATABASE +
                                     "/" + COLLECTION + "/shards/" + SHARD)
                                .typeName()) == "array");
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .length() == 3);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[0]
                    .copyString() == FREE_SERVER);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[1]
                    .copyString() == SHARD_FOLLOWER1);
    EXPECT_TRUE(writes
                    .get("/arango/Plan/Collections/" + DATABASE + "/" +
                         COLLECTION + "/shards/" + SHARD)[2]
                    .copyString() == SHARD_LEADER);

    auto preconditions = q->slice()[0][1];
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
                    .get("old")[0]
                    .copyString() == "_" + SHARD_LEADER);
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .get("old")[1]
                    .copyString() == SHARD_FOLLOWER1);
    EXPECT_TRUE(preconditions
                    .get("/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION + "/shards/" + SHARD)
                    .get("old")[2]
                    .copyString() == FREE_SERVER);

    return fakeWriteResult;
  });
  // nothing should happen so nothing should be called
  AgentInterface& agent = mockAgent.get();

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createAgencyFromBuilder(*builder);

  auto moveShard = MoveShard(agency, &agent, PENDING, jobId);
  moveShard.run(aborts);
  Verify(Method(mockAgent, write));
}

}  // namespace move_shard_test
}  // namespace tests
}  // namespace arangodb
