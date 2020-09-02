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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include <iostream>

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include <velocypack/Collection.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Mocks/LogLevels.h"

#include "Agency/ActiveFailoverJob.h"
#include "Agency/AgentInterface.h"
#include "Agency/Node.h"
#include "Basics/StringUtils.h"
#include "Random/RandomGenerator.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::consensus;
using namespace fakeit;

namespace arangodb {
namespace tests {
namespace active_failover_test {

const std::string PREFIX = "arango";
const std::string LEADER = "SNGL-leader";
const std::string FOLLOWER1 = "SNGL-follower1";   // tick 10, STATE GOOD
const std::string FOLLOWER2 = "SNGL-follower2";   // tick 1, STATE GOOD
const std::string FOLLOWER3 = "SNGL-follower23";  // tick 9, STATE GOOD
const std::string FOLLOWER4 = "SNGL-follower4";   // tick 100, STATE BAD
const std::string FOLLOWER5 = "SNGL-follower5";  // tick 1000, STATE GOOD wrong leader

bool aborts = false;

const char* agency =
#include "ActiveFailoverTest.json"
    ;

const char* transient =
#include "ActiveFailoverTestTransient.json"
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
  Options options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(c);

  Builder builder;
  builder.add(parser.steal()->slice());
  return builder;
}

typedef std::function<std::unique_ptr<Builder>(Slice const&, std::string const&)> TestStructType;

inline static std::string typeName(Slice const& slice) {
  return std::string(slice.typeName());
}

class ActiveFailover : public ::testing::Test,
                       public LogSuppressor<Logger::SUPERVISION, LogLevel::FATAL> {
 protected:
  Builder base;
  std::string jobId;
  write_ret_t fakeWriteResult;

  ActiveFailover()
      : jobId{"1"},
        fakeWriteResult{true, "", std::vector<apply_ret_t>{APPLIED},
                        std::vector<index_t>{1}} {
    arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
    base = createBuilder(agency);
    jobId = "1";
  }
};

TEST_F(ActiveFailover, creating_a_job_should_create_a_job_in_todo) {
  Mock<AgentInterface> mockAgent;

  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    auto expectedJobKey = "/arango/Target/ToDo/" + jobId;
    EXPECT_EQ(typeName(q->slice()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(typeName(q->slice()[0]), "array");
    // operations + preconditions
    EXPECT_EQ(q->slice()[0].length(), 2);
    EXPECT_EQ(typeName(q->slice()[0][0]), "object");
    EXPECT_EQ(q->slice()[0][0].length(), 2);  // should do an entry in todo and failedservers
    EXPECT_EQ(typeName(q->slice()[0][0].get(expectedJobKey)), "object");

    auto job = q->slice()[0][0].get(expectedJobKey);
    EXPECT_EQ(typeName(job.get("creator")), "string");
    EXPECT_EQ(typeName(job.get("type")), "string");
    EXPECT_EQ(job.get("type").copyString(), "activeFailover");
    EXPECT_EQ(typeName(job.get("server")), "string");
    EXPECT_EQ(job.get("server").copyString(), LEADER);
    EXPECT_EQ(typeName(job.get("jobId")), "string");
    EXPECT_EQ(job.get("jobId").copyString(), jobId);
    EXPECT_EQ(typeName(job.get("timeCreated")), "string");

    return fakeWriteResult;
  });

  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);

  auto& agent = mockAgent.get();
  Node snapshot = createNodeFromBuilder(base);
  ActiveFailoverJob job(snapshot(PREFIX), &agent, jobId, "tests", LEADER);

  ASSERT_TRUE(job.create());
  Verify(Method(mockAgent, write));
}

TEST_F(ActiveFailover, the_state_is_already_good_and_failservers_is_still_in_the_snapshot) {
  const char* tt = R"=({"arango":{"Supervision":{"Health":{"SNGL-leader":{"Status":"GOOD"}}}}})=";
  VPackBuilder overw = createBuilder(tt);
  VPackBuilder mod = VPackCollection::merge(base.slice(), overw.slice(), true);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_EQ(typeName(q->slice()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(typeName(q->slice()[0]), "array");
    // operations + preconditions
    EXPECT_EQ(q->slice()[0].length(), 2);
    EXPECT_EQ(typeName(q->slice()[0][0]), "object");
    EXPECT_EQ(typeName(q->slice()[0][1]), "object");

    auto writes = q->slice()[0][0];
    EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
    EXPECT_EQ(writes.get("/arango/Target/ToDo/1").get("server").copyString(), LEADER);

    auto precond = q->slice()[0][1];
    EXPECT_TRUE(typeName(precond.get(
                    "/arango/Supervision/Health/SNGL-leader/Status")) ==
                "object");
    EXPECT_TRUE(
        precond.get("/arango/Supervision/Health/SNGL-leader/Status").get("old").copyString() ==
        "BAD");
    EXPECT_TRUE(typeName(precond.get("/arango/Target/FailedServers").get("old")) ==
                "object");

    return write_ret_t{false, "", std::vector<apply_ret_t>{APPLIED},
                       std::vector<index_t>{0}};
  });

  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  Node snapshot = createNodeFromBuilder(mod);
  ActiveFailoverJob job(snapshot(PREFIX), &agent, jobId, "unittest", LEADER);

  ASSERT_FALSE(job.create());
  ASSERT_EQ(job.status(), JOB_STATUS::NOTFOUND);
  Verify(Method(mockAgent, write));
}

TEST_F(ActiveFailover, server_is_healthy_again_job_finishes) {
  const char* health = R"=({"arango":{"Supervision":{"Health":{"SNGL-leader":{"Status":"GOOD"}}},
                                        "Target":{"ToDo":{"1":{"jobId":"1","type":"activeFailover"}}}}})=";
  VPackBuilder mod =
      VPackCollection::merge(base.slice(), createBuilder(health).slice(), true);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_EQ(typeName(q->slice()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(typeName(q->slice()[0]), "array");
    // operations + preconditions
    EXPECT_EQ(q->slice()[0].length(), 2);
    EXPECT_EQ(typeName(q->slice()[0][0]), "object");
    EXPECT_EQ(typeName(q->slice()[0][1]), "object");

    auto writes = q->slice()[0][0];
    EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
    EXPECT_EQ(writes.get("/arango/Target/ToDo/1").get("server").copyString(), LEADER);

    auto precond = q->slice()[0][1];
    EXPECT_TRUE(typeName(precond.get(
                    "/arango/Supervision/Health/SNGL-leader/Status")) ==
                "object");
    EXPECT_TRUE(
        precond.get("/arango/Supervision/Health/SNGL-leader/Status").get("old").copyString() ==
        "BAD");
    EXPECT_TRUE(typeName(precond.get("/arango/Target/FailedServers").get("old")) ==
                "object");

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  Node snapshot = createNodeFromBuilder(mod);  // snapshort contains GOOD leader

  ActiveFailoverJob job(snapshot(PREFIX), &agent, jobId, "unittest", LEADER);
  ASSERT_TRUE(job.create());  // we already put the TODO entry in the snapshot for finish
  ASSERT_EQ(job.status(), JOB_STATUS::TODO);
  Verify(Method(mockAgent, write)).Exactly(1);

  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    // check that the job finishes now, without changing leader
    VPackSlice writes = q->slice()[0][0];
    EXPECT_TRUE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) ==
                "string");
    EXPECT_EQ(typeName(writes.get("/arango/Target/Finished/1")), "object");
    EXPECT_FALSE(writes.hasKey("/arango" + asyncReplLeader));  // no change to leader
    return fakeWriteResult;
  });

  ASSERT_TRUE(job.start(aborts));
  ASSERT_EQ(job.status(), JOB_STATUS::FINISHED);
  Verify(Method(mockAgent, write)).Exactly(2);
}

TEST_F(ActiveFailover, current_leader_is_different_from_server_in_job) {
  const char* health = R"=({"arango":{"Plan":{"AsyncReplication":{"Leader":"SNGL-follower1"}},
    "Target":{"ToDo":{"1":{"jobId":"1","type":"activeFailover"}}}}})=";
  VPackBuilder mod =
      VPackCollection::merge(base.slice(), createBuilder(health).slice(), true);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_EQ(typeName(q->slice()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(typeName(q->slice()[0]), "array");
    // operations + preconditions
    EXPECT_EQ(q->slice()[0].length(), 2);
    EXPECT_EQ(typeName(q->slice()[0][0]), "object");
    EXPECT_EQ(typeName(q->slice()[0][1]), "object");

    auto writes = q->slice()[0][0];
    EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
    EXPECT_EQ(writes.get("/arango/Target/ToDo/1").get("server").copyString(), LEADER);

    auto precond = q->slice()[0][1];
    EXPECT_TRUE(typeName(precond.get(
                    "/arango/Supervision/Health/SNGL-leader/Status")) ==
                "object");
    EXPECT_TRUE(
        precond.get("/arango/Supervision/Health/SNGL-leader/Status").get("old").copyString() ==
        "BAD");
    EXPECT_TRUE(typeName(precond.get("/arango/Target/FailedServers").get("old")) ==
                "object");

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  Node snapshot = createNodeFromBuilder(mod);  // snapshort contains different leader

  ActiveFailoverJob job(snapshot(PREFIX), &agent, jobId, "unittest", LEADER);
  ASSERT_TRUE(job.create());  // we already put the TODO entry in the snapshot for finish
  ASSERT_EQ(job.status(), JOB_STATUS::TODO);
  Verify(Method(mockAgent, write)).Exactly(1);

  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    // check that the job finishes now, without changing leader
    VPackSlice writes = q->slice()[0][0];
    EXPECT_TRUE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) ==
                "string");
    EXPECT_EQ(typeName(writes.get("/arango/Target/Finished/1")), "object");
    EXPECT_FALSE(writes.hasKey("/arango" + asyncReplLeader));  // no change to leader
    return fakeWriteResult;
  });

  ASSERT_TRUE(job.start(aborts));
  ASSERT_EQ(job.status(), JOB_STATUS::FINISHED);
  Verify(Method(mockAgent, write)).Exactly(2);

}  // SECTION

TEST_F(ActiveFailover, no_in_sync_follower_found_job_retries) {
  // follower follows wrong leader
  const char* noInSync =
      R"=({"arango":{"AsyncReplication":{"SNGL-follower1":{"leader":"abc","lastTick":9}}}})=";
  trans_ret_t fakeTransient{true, "", 1, 0,
                            std::make_shared<Builder>(createBuilder(noInSync))};

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_EQ(typeName(q->slice()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(typeName(q->slice()[0]), "array");
    // operations + preconditions
    EXPECT_EQ(q->slice()[0].length(), 2);
    EXPECT_EQ(typeName(q->slice()[0][0]), "object");
    EXPECT_EQ(typeName(q->slice()[0][1]), "object");

    auto writes = q->slice()[0][0];
    EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
    EXPECT_EQ(writes.get("/arango/Target/ToDo/1").get("server").copyString(), LEADER);

    auto precond = q->slice()[0][1];
    EXPECT_TRUE(typeName(precond.get(
                    "/arango/Supervision/Health/SNGL-leader/Status")) ==
                "object");
    EXPECT_TRUE(
        precond.get("/arango/Supervision/Health/SNGL-leader/Status").get("old").copyString() ==
        "BAD");
    EXPECT_TRUE(typeName(precond.get("/arango/Target/FailedServers").get("old")) ==
                "object");

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  Node snapshot = createNodeFromBuilder(base);

  ActiveFailoverJob job(snapshot(PREFIX), &agent, jobId, "unittest", LEADER);
  ASSERT_TRUE(job.create());  // we already put the TODO entry in the snapshot for finish
  ASSERT_EQ(job.status(), JOB_STATUS::TODO);
  Verify(Method(mockAgent, write)).Exactly(1);

  When(Method(mockAgent, transient)).Return(fakeTransient);
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    // check that the job fails now
    auto writes = q->slice()[0][0];
    EXPECT_TRUE(std::string(writes.get("/arango/Target/ToDo/1").get("op").typeName()) ==
                "string");
    EXPECT_TRUE(std::string(writes.get("/arango/Target/Failed/1").typeName()) ==
                "object");
    return fakeWriteResult;
  });

  ASSERT_FALSE(job.start(aborts));
  // job status stays on TODO and can retry later
  ASSERT_EQ(job.status(), JOB_STATUS::TODO);
  Verify(Method(mockAgent, transient)).Exactly(Once);
  Verify(Method(mockAgent, write)).Exactly(Once);  // finish is not called
}

TEST_F(ActiveFailover, follower_with_best_tick_value_used) {
  // 2 in-sync followers, follower1 should be used
  trans_ret_t fakeTransient{true, "", 1, 0,
                            std::make_shared<Builder>(createBuilder(transient))};

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_EQ(typeName(q->slice()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(typeName(q->slice()[0]), "array");
    // we always simply override! no preconditions...
    EXPECT_EQ(q->slice()[0].length(), 2);
    EXPECT_EQ(typeName(q->slice()[0][0]), "object");
    EXPECT_EQ(typeName(q->slice()[0][1]), "object");

    auto writes = q->slice()[0][0];
    EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
    EXPECT_EQ(writes.get("/arango/Target/ToDo/1").get("server").copyString(), LEADER);

    auto precond = q->slice()[0][1];
    EXPECT_TRUE(typeName(precond.get(
                    "/arango/Supervision/Health/SNGL-leader/Status")) ==
                "object");
    EXPECT_TRUE(
        precond.get("/arango/Supervision/Health/SNGL-leader/Status").get("old").copyString() ==
        "BAD");
    EXPECT_TRUE(typeName(precond.get("/arango/Target/FailedServers").get("old")) ==
                "object");

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  Node snapshot = createNodeFromBuilder(base);

  ActiveFailoverJob job(snapshot(PREFIX), &agent, jobId, "unittest", LEADER);
  ASSERT_TRUE(job.create());  // we already put the TODO entry in the snapshot for finish
  ASSERT_EQ(job.status(), JOB_STATUS::TODO);
  Verify(Method(mockAgent, write)).Exactly(1);

  When(Method(mockAgent, transient)).Return(fakeTransient);
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_EQ(typeName(q->slice()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(typeName(q->slice()[0]), "array");
    // we always simply override! no preconditions...
    EXPECT_EQ(q->slice()[0].length(), 2);
    EXPECT_EQ(typeName(q->slice()[0][0]), "object");
    EXPECT_EQ(typeName(q->slice()[0][1]), "object");

    // check that the job succeeds now
    auto writes = q->slice()[0][0];
    EXPECT_TRUE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) ==
                "string");
    EXPECT_EQ(typeName(writes.get("/arango/Target/Finished/1")), "object");
    EXPECT_TRUE(typeName(writes.get("/arango/Plan/AsyncReplication/Leader")) ==
                "string");
    EXPECT_EQ(writes.get("/arango/Plan/AsyncReplication/Leader").copyString(), FOLLOWER1);

    auto precond = q->slice()[0][1];
    EXPECT_TRUE(typeName(precond.get(
                    "/arango/Supervision/Health/SNGL-leader/Status")) ==
                "object");
    EXPECT_TRUE(
        precond.get("/arango/Supervision/Health/SNGL-leader/Status").get("old").copyString() ==
        "FAILED");
    EXPECT_TRUE(precond.get("/arango/Supervision/Health/SNGL-follower1/Status")
                    .get("old")
                    .copyString() == "GOOD");
    EXPECT_TRUE(precond.get("/arango/Plan/AsyncReplication/Leader").get("old").copyString() ==
                LEADER);

    return fakeWriteResult;
  });

  ASSERT_TRUE(job.start(aborts));
  // job status stays on TODO and can retry later
  ASSERT_EQ(job.status(), JOB_STATUS::FINISHED);
  Verify(Method(mockAgent, transient)).Exactly(1);
  Verify(Method(mockAgent, write)).Exactly(2);
}

}  // namespace active_failover_test
}  // namespace tests
}  // namespace arangodb
