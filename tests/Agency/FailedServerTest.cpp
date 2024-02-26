////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <iostream>

#include "Mocks/LogLevels.h"

#include "Agency/AddFollower.h"
#include "Agency/AgentInterface.h"
#include "Agency/FailedServer.h"
#include "Agency/MoveShard.h"
#include "Agency/Node.h"
#include "Basics/StringUtils.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::consensus;
using namespace fakeit;
using namespace arangodb::velocypack;

namespace arangodb {
namespace tests {
namespace failed_server_test {

[[maybe_unused]] const std::string PREFIX = "/arango";
[[maybe_unused]] const std::string DATABASE = "database";
[[maybe_unused]] const std::string COLLECTION = "collection";
[[maybe_unused]] const std::string SHARD = "shard";
[[maybe_unused]] const std::string SHARD_LEADER = "leader";
[[maybe_unused]] const std::string SHARD_FOLLOWER1 = "follower1";
[[maybe_unused]] const std::string SHARD_FOLLOWER2 = "follower2";
[[maybe_unused]] const std::string FREE_SERVER = "free";
[[maybe_unused]] const std::string FREE_SERVER2 = "free2";

bool aborts = false;

typedef std::function<std::unique_ptr<Builder>(Slice const&,
                                               std::string const&)>
    TestStructureType;

const char* agency =
#include "FailedServerTest.json"
    ;

NodePtr createNodeFromBuilder(VPackBuilder const& builder) {
  return Node::create(builder.slice());
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

NodePtr createNode(char const* c) {
  return createNodeFromBuilder(createBuilder(c));
}

NodePtr createRootNode() { return createNode(agency); }

inline std::string typeName(Slice const& slice) {
  return std::string(slice.typeName());
}

class FailedServerTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::SUPERVISION,
                                            arangodb::LogLevel::ERR> {
 protected:
  std::string const jobId = "1";
  std::shared_ptr<Builder> transBuilder;
  NodePtr agency;
  write_ret_t fakeWriteResult;
  trans_ret_t fakeTransResult;

  FailedServerTest()
      : jobId("1"),
        transBuilder(std::make_shared<Builder>()),
        agency(createRootNode()),
        fakeWriteResult(true, "", std::vector<apply_ret_t>{APPLIED},
                        std::vector<index_t>{1}),
        fakeTransResult(true, "", 1, 0, transBuilder) {
    VPackArrayBuilder a(transBuilder.get());
    transBuilder->add(VPackValue((uint64_t)1));
  }
};

TEST_F(FailedServerTest, creating_a_job_should_create_a_job_in_todo) {
  Mock<AgentInterface> mockAgent;

  std::string jobId = "1";
  When(Method(mockAgent, write))
      .AlwaysDo([&](velocypack::Slice q,
                    consensus::AgentInterface::WriteMode w) -> write_ret_t {
        auto expectedJobKey = PREFIX + toDoPrefix + jobId;
        EXPECT_EQ(typeName(q), "array");
        EXPECT_EQ(q.length(), 1);
        EXPECT_EQ(typeName(q[0]), "array");
        EXPECT_EQ(q[0].length(), 2);
        EXPECT_EQ(typeName(q[0][0]), "object");
        EXPECT_EQ(q[0][0].length(), 2);
        EXPECT_EQ(typeName(q[0][0].get(expectedJobKey)), "object");

        auto job = q[0][0].get(expectedJobKey);
        EXPECT_EQ(typeName(job.get("creator")), "string");
        EXPECT_EQ(typeName(job.get("type")), "string");
        EXPECT_EQ(job.get("type").copyString(), "failedServer");
        EXPECT_EQ(typeName(job.get("server")), "string");
        EXPECT_EQ(job.get("server").copyString(), SHARD_LEADER);
        EXPECT_EQ(typeName(job.get("jobId")), "string");
        EXPECT_EQ(typeName(job.get("timeCreated")), "string");
        EXPECT_EQ(typeName(job.get("notBefore")), "string");
        EXPECT_TRUE(job.get("failedLeaderAddsFollower").isTrue());

        return fakeWriteResult;
      });
  When(Method(mockAgent, waitFor))
      .AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  auto builder = agency->toBuilder();
  FailedServer(*agency->get(PREFIX), &agent, jobId, "unittest", SHARD_LEADER,
               "2022-01-01T00:00:00Z", true)
      .create();
  Verify(Method(mockAgent, write));
}

TEST_F(FailedServerTest,
       creating_a_job_should_create_a_job_in_todo_failed_leader_no_followers) {
  Mock<AgentInterface> mockAgent;

  std::string jobId = "1";
  When(Method(mockAgent, write))
      .AlwaysDo([&](velocypack::Slice q,
                    consensus::AgentInterface::WriteMode w) -> write_ret_t {
        auto expectedJobKey = PREFIX + toDoPrefix + jobId;
        EXPECT_EQ(typeName(q), "array");
        EXPECT_EQ(q.length(), 1);
        EXPECT_EQ(typeName(q[0]), "array");
        EXPECT_EQ(q[0].length(), 2);
        EXPECT_EQ(typeName(q[0][0]), "object");
        EXPECT_EQ(q[0][0].length(), 2);
        EXPECT_EQ(typeName(q[0][0].get(expectedJobKey)), "object");

        auto job = q[0][0].get(expectedJobKey);
        EXPECT_EQ(typeName(job.get("creator")), "string");
        EXPECT_EQ(typeName(job.get("type")), "string");
        EXPECT_EQ(job.get("type").copyString(), "failedServer");
        EXPECT_EQ(typeName(job.get("server")), "string");
        EXPECT_EQ(job.get("server").copyString(), SHARD_LEADER);
        EXPECT_EQ(typeName(job.get("jobId")), "string");
        EXPECT_EQ(typeName(job.get("timeCreated")), "string");
        EXPECT_EQ(typeName(job.get("notBefore")), "string");
        EXPECT_TRUE(job.get("failedLeaderAddsFollower").isFalse());

        return fakeWriteResult;
      });
  When(Method(mockAgent, waitFor))
      .AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  auto builder = agency->toBuilder();
  FailedServer(*agency->get(PREFIX), &agent, jobId, "unittest", SHARD_LEADER,
               "2022-01-01T00:00:00Z", false)
      .create();
  Verify(Method(mockAgent, write));
}

TEST_F(
    FailedServerTest,
    the_state_is_still_bad_and_failedservers_is_still_in_snapshot_violate_good) {
  TestStructureType createTestStructure = [&](Slice const& s,
                                              std::string const& path) {
    std::unique_ptr<Builder> builder;
    if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION) {
      return builder;
    }
    builder = std::make_unique<Builder>();

    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
    } else {
      builder->add(s);
    }

    return builder;
  };

  auto builder = createTestStructure(agency->toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  auto agency = createNodeFromBuilder(*builder);
  agency = agency->placeAt("/arango/Supervision/Health/leader/Status", "GOOD");

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write))
      .AlwaysDo([&](velocypack::Slice q,
                    consensus::AgentInterface::WriteMode w) -> write_ret_t {
        EXPECT_EQ(typeName(q), "array");
        EXPECT_EQ(q.length(), 1);
        EXPECT_EQ(typeName(q[0]), "array");
        // we always simply override! no preconditions...
        EXPECT_EQ(q[0].length(), 2);
        EXPECT_EQ(typeName(q[0][0]), "object");
        EXPECT_EQ(typeName(q[0][1]), "object");

        auto writes = q[0][0];
        EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
        EXPECT_EQ(
            writes.get("/arango/Target/ToDo/1").get("server").copyString(),
            SHARD_LEADER);

        auto precond = q[0][1];
        EXPECT_TRUE(
            typeName(precond.get("/arango/Supervision/Health/leader/Status")) ==
            "object");
        EXPECT_TRUE(precond.get("/arango/Supervision/Health/leader/Status")
                        .get("old")
                        .copyString() == "BAD");
        EXPECT_TRUE(
            typeName(precond.get("/arango/Target/FailedServers").get("old")) ==
            "object");

        return fakeWriteResult;
      });

  When(Method(mockAgent, waitFor))
      .AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  FailedServer(*agency->get(PREFIX), &agent, jobId, "unittest", SHARD_LEADER)
      .create();

  Verify(Method(mockAgent, write));
}

TEST_F(
    FailedServerTest,
    the_state_is_still_bad_and_failedservers_is_still_in_snapshot_violate_failed) {
  TestStructureType createTestStructure = [&](Slice const& s,
                                              std::string const& path) {
    std::unique_ptr<Builder> builder;
    if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION) {
      return builder;
    }
    builder = std::make_unique<Builder>();

    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
    } else {
      builder->add(s);
    }

    return builder;
  };

  auto builder = createTestStructure(agency->toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  auto agency = createNodeFromBuilder(*builder);
  agency =
      agency->placeAt("/arango/Supervision/Health/leader/Status", "FAILED");

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write))
      .AlwaysDo([&](velocypack::Slice q,
                    consensus::AgentInterface::WriteMode w) -> write_ret_t {
        EXPECT_EQ(typeName(q), "array");
        EXPECT_EQ(q.length(), 1);
        EXPECT_EQ(typeName(q[0]), "array");
        // we always simply override! no preconditions...
        EXPECT_EQ(q[0].length(), 2);
        EXPECT_EQ(typeName(q[0][0]), "object");
        EXPECT_EQ(typeName(q[0][1]), "object");

        auto writes = q[0][0];
        EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
        EXPECT_EQ(
            writes.get("/arango/Target/ToDo/1").get("server").copyString(),
            SHARD_LEADER);

        auto precond = q[0][1];
        EXPECT_TRUE(
            typeName(precond.get("/arango/Supervision/Health/leader/Status")) ==
            "object");
        EXPECT_TRUE(precond.get("/arango/Supervision/Health/leader/Status")
                        .get("old")
                        .copyString() == "BAD");
        EXPECT_TRUE(
            typeName(precond.get("/arango/Target/FailedServers").get("old")) ==
            "object");

        return fakeWriteResult;
      });

  When(Method(mockAgent, waitFor))
      .AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  FailedServer(*agency->get(PREFIX), &agent, jobId, "unittest", SHARD_LEADER)
      .create();
}

TEST_F(FailedServerTest,
       the_state_is_still_bad_and_failedservers_is_still_in_snapshot) {
  TestStructureType createTestStructure = [&](Slice const& s,
                                              std::string const& path) {
    std::unique_ptr<Builder> builder;
    if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION) {
      return builder;
    }
    builder = std::make_unique<Builder>();

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
        char const* todo =
            R"=({"creator":"unittest","jobId":"1","server":"leader",
               "timeCreated":"2017-04-10T11:40:09Z","type":"failedServer"})=";
        builder->add(jobId, createBuilder(todo).slice());
      }
    } else {
      builder->add(s);
    }

    return builder;
  };

  auto builder = createTestStructure(agency->toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  auto agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write))
      .AlwaysDo([&](velocypack::Slice q,
                    consensus::AgentInterface::WriteMode w) -> write_ret_t {
        EXPECT_EQ(typeName(q), "array");
        EXPECT_EQ(q.length(), 1);
        EXPECT_EQ(typeName(q[0]), "array");
        // we always simply override! no preconditions...
        EXPECT_EQ(q[0].length(), 1);
        EXPECT_EQ(typeName(q[0][0]), "object");

        auto writes = q[0][0];
        EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
        EXPECT_TRUE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) ==
                    "string");
        EXPECT_TRUE(
            writes.get("/arango/Target/ToDo/1").get("op").copyString() ==
            "delete");
        EXPECT_EQ(typeName(writes.get("/arango/Target/Pending/1")), "object");
        return fakeWriteResult;
      });

  When(Method(mockAgent, waitFor))
      .AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  FailedServer(*agency->get("arango"), &agent, JOB_STATUS::TODO, jobId)
      .start(aborts);

  Verify(Method(mockAgent, write));
}

TEST_F(FailedServerTest,
       the_state_is_still_bad_and_failedservers_is_still_in_snapshot_2) {
  TestStructureType createTestStructure = [&](Slice const& s,
                                              std::string const& path) {
    std::unique_ptr<Builder> builder;
    if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION) {
      return builder;
    }
    builder = std::make_unique<Builder>();

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
        char const* todo =
            R"=({"creator":"unittest","jobId":"1","server":"leader",
               "timeCreated":"2017-04-10T11:40:09Z","type":"failedServer",
               "failedLeaderAddsFollower":true})=";
        builder->add(jobId, createBuilder(todo).slice());
      }
    } else {
      if (path == "/arango/Supervision/Health/leader/Status") {
        builder->add(VPackValue("FAILED"));
      } else {
        builder->add(s);
      }
    }

    return builder;
  };

  auto builder = createTestStructure(agency->toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  auto agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write))
      .AlwaysDo([&](velocypack::Slice q,
                    consensus::AgentInterface::WriteMode w) -> write_ret_t {
        EXPECT_EQ(typeName(q), "array");
        EXPECT_EQ(q.length(), 1);
        EXPECT_EQ(typeName(q[0]), "array");
        EXPECT_EQ(q[0].length(), 2);  // with precondition
        EXPECT_EQ(typeName(q[0][0]), "object");

        auto writes = q[0][0];
        EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
        EXPECT_TRUE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) ==
                    "string");
        EXPECT_TRUE(
            writes.get("/arango/Target/ToDo/1").get("op").copyString() ==
            "delete");
        auto job = writes.get("/arango/Target/Pending/1");
        EXPECT_EQ(typeName(job), "object");
        return fakeWriteResult;
      });

  When(Method(mockAgent, waitFor))
      .AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  FailedServer(*agency->get("arango"), &agent, JOB_STATUS::TODO, jobId)
      .start(aborts);

  Verify(Method(mockAgent, write));
}

TEST_F(FailedServerTest, a_failed_server_test_starts_and_is_moved_to_pending) {
  TestStructureType createTestStructure = [&](Slice const& s,
                                              std::string const& path) {
    std::unique_ptr<Builder> builder;
    builder = std::make_unique<Builder>();

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
        char const* todo =
            R"=({"creator":"unittest","jobId":"1","server":"leader",
               "timeCreated":"2017-04-10T11:40:09Z","type":"failedServer",
               "failedLeaderAddsFollower":true})=";
        builder->add(jobId, createBuilder(todo).slice());
      }
    } else {
      if (path == "/arango/Supervision/Health/leader/Status") {
        builder->add(VPackValue("FAILED"));
      } else if (path == "/arango/Supervision/Health/follower1/Status") {
        builder->add(VPackValue("GOOD"));
      } else {
        builder->add(s);
      }
    }

    return builder;
  };

  auto builder = createTestStructure(agency->toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  auto agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write))
      .AlwaysDo([&](velocypack::Slice q,
                    consensus::AgentInterface::WriteMode w) -> write_ret_t {
        EXPECT_EQ(typeName(q), "array");
        EXPECT_EQ(q.length(), 1);
        EXPECT_EQ(typeName(q[0]), "array");
        EXPECT_EQ(q[0].length(), 2);  // with precondition
        EXPECT_EQ(typeName(q[0][0]), "object");
        EXPECT_EQ(typeName(q[0][1]), "object");
        velocypack::Slice writes = q[0][0];
        EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
        EXPECT_TRUE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) ==
                    "string");
        EXPECT_TRUE(
            writes.get("/arango/Target/ToDo/1").get("op").copyString() ==
            "delete");
        auto job = writes.get("/arango/Target/Pending/1");
        EXPECT_EQ(typeName(job), "object");
        EXPECT_TRUE(job.get("failedLeaderAddsFollower").isTrue());
        auto newJob = writes.get("/arango/Target/ToDo/1-0");
        EXPECT_EQ(typeName(newJob), "object");
        auto type = newJob.get("type");
        EXPECT_EQ(typeName(type), "string");
        EXPECT_EQ(type.copyString(), std::string("failedLeader"));
        EXPECT_TRUE(newJob.get("addsFollower").isTrue());
        return fakeWriteResult;
      });

  When(Method(mockAgent, waitFor))
      .AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  FailedServer(*agency->get("arango"), &agent, JOB_STATUS::TODO, jobId)
      .start(aborts);

  Verify(Method(mockAgent, write));
}

TEST_F(FailedServerTest,
       a_failed_server_test_starts_and_is_moved_to_pending_no_followers) {
  TestStructureType createTestStructure = [&](Slice const& s,
                                              std::string const& path) {
    std::unique_ptr<Builder> builder;
    builder = std::make_unique<Builder>();

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
        char const* todo =
            R"=({"creator":"unittest","jobId":"1","server":"leader",
               "timeCreated":"2017-04-10T11:40:09Z","type":"failedServer",
               "failedLeaderAddsFollower":false})=";
        builder->add(jobId, createBuilder(todo).slice());
      }
    } else {
      if (path == "/arango/Supervision/Health/leader/Status") {
        builder->add(VPackValue("FAILED"));
      } else if (path == "/arango/Supervision/Health/follower1/Status") {
        builder->add(VPackValue("GOOD"));
      } else {
        builder->add(s);
      }
    }

    return builder;
  };

  auto builder = createTestStructure(agency->toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  auto agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write))
      .AlwaysDo([&](velocypack::Slice q,
                    consensus::AgentInterface::WriteMode w) -> write_ret_t {
        EXPECT_EQ(typeName(q), "array");
        EXPECT_EQ(q.length(), 1);
        EXPECT_EQ(typeName(q[0]), "array");
        EXPECT_EQ(q[0].length(), 2);  // with precondition
        EXPECT_EQ(typeName(q[0][0]), "object");
        EXPECT_EQ(typeName(q[0][1]), "object");
        velocypack::Slice writes = q[0][0];
        EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
        EXPECT_TRUE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) ==
                    "string");
        EXPECT_TRUE(
            writes.get("/arango/Target/ToDo/1").get("op").copyString() ==
            "delete");
        auto job = writes.get("/arango/Target/Pending/1");
        EXPECT_EQ(typeName(job), "object");
        EXPECT_TRUE(job.get("failedLeaderAddsFollower").isFalse());
        auto newJob = writes.get("/arango/Target/ToDo/1-0");
        EXPECT_EQ(typeName(newJob), "object");
        auto type = newJob.get("type");
        EXPECT_EQ(typeName(type), "string");
        EXPECT_EQ(type.copyString(), std::string("failedLeader"));
        EXPECT_TRUE(newJob.get("addsFollower").isFalse());
        return fakeWriteResult;
      });

  When(Method(mockAgent, waitFor))
      .AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  FailedServer(*agency->get("arango"), &agent, JOB_STATUS::TODO, jobId)
      .start(aborts);

  Verify(Method(mockAgent, write));
}

TEST_F(FailedServerTest, a_failed_server_job_is_finished) {
  TestStructureType createTestStructure = [&](Slice const& s,
                                              std::string const& path) {
    std::unique_ptr<Builder> builder;
    builder = std::make_unique<Builder>();

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
        char const* pending =
            R"=({"creator":"unittest","jobId":"1","server":"leader",
               "timeCreated":"2017-04-10T11:40:09Z","type":"failedServer",
               "timeStarted":"2017-04-10T11:40:10Z",
               "failedLeaderAddsFollower":true})=";
        builder->add(jobId, createBuilder(pending).slice());
      }
    } else {
      if (path == "/arango/Supervision/Health/leader/Status") {
        builder->add(VPackValue("FAILED"));
      } else if (path == "/arango/Supervision/Health/follower1/Status") {
        builder->add(VPackValue("GOOD"));
      } else {
        builder->add(s);
      }
    }

    return builder;
  };

  auto builder = createTestStructure(agency->toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  auto agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write))
      .AlwaysDo([&](velocypack::Slice q,
                    consensus::AgentInterface::WriteMode w) -> write_ret_t {
        EXPECT_EQ(typeName(q), "array");
        EXPECT_EQ(q.length(), 1);
        EXPECT_EQ(typeName(q[0]), "array");
        EXPECT_EQ(q[0].length(), 1);  // with precondition
        EXPECT_EQ(typeName(q[0][0]), "object");
        velocypack::Slice writes = q[0][0];
        EXPECT_EQ(typeName(writes.get("/arango/Target/Pending/1")), "object");
        EXPECT_TRUE(
            typeName(writes.get("/arango/Target/Pending/1").get("op")) ==
            "string");
        EXPECT_TRUE(
            writes.get("/arango/Target/Pending/1").get("op").copyString() ==
            "delete");
        auto job = writes.get("/arango/Target/Finished/1");
        EXPECT_EQ(typeName(job), "object");
        EXPECT_TRUE(job.get("failedLeaderAddsFollower").isTrue());
        return fakeWriteResult;
      });

  When(Method(mockAgent, waitFor))
      .AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  FailedServer(*agency->get("arango"), &agent, JOB_STATUS::PENDING, jobId)
      .status();

  Verify(Method(mockAgent, write));
}

}  // namespace failed_server_test
}  // namespace tests
}  // namespace arangodb
