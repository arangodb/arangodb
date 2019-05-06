////////////////////////////////////////////////////////////////////////////////
/// @brief test case for ActiverFailover job
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
#include "catch.hpp"
#include "fakeit.hpp"

#include "Agency/ActiveFailoverJob.h"
#include "Agency/AgentInterface.h"
#include "Agency/Node.h"
#include "lib/Basics/StringUtils.h"
#include "lib/Random/RandomGenerator.h"
#include <iostream>
#include <velocypack/Collection.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::consensus;
using namespace fakeit;

namespace arangodb {
namespace tests {
namespace active_failover_test {

const std::string PREFIX = "arango";
const std::string LEADER = "SNGL-leader";
const std::string FOLLOWER1 = "SNGL-follower1"; // tick 10, STATE GOOD
const std::string FOLLOWER2 = "SNGL-follower2"; // tick 1, STATE GOOD
const std::string FOLLOWER3 = "SNGL-follower23"; // tick 9, STATE GOOD
const std::string FOLLOWER4 = "SNGL-follower4"; // tick 100, STATE BAD
const std::string FOLLOWER5 = "SNGL-follower5"; // tick 1000, STATE GOOD wrong leader

bool aborts = false;

const char *agency =
#include "ActiveFailoverTest.json"
  ;

const char *transient =
#include "ActiveFailoverTestTransient.json"
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

  Options options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(c);
  
  Builder builder;
  builder.add(parser.steal()->slice());
  return builder;
  
}

typedef std::function<std::unique_ptr<Builder>(
  Slice const&, std::string const&)> TestStructType;

inline static std::string typeName (Slice const& slice) {
  return std::string(slice.typeName());
}

TEST_CASE("ActiveFailover", "[agency][supervision]") {
  
  arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);

  Builder base = createBuilder(agency);
  //baseStructure.toBuilder(builder);*/

  std::string jobId = "1";

  write_ret_t fakeWriteResult {true, "", std::vector<apply_ret_t> {APPLIED}, std::vector<index_t> {1}};
  
  SECTION("creating a job should create a job in todo") {
    Mock<AgentInterface> mockAgent;

    write_ret_t fakeWriteResult {true, "", std::vector<apply_ret_t> {APPLIED}, std::vector<index_t> {1}};
    When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
        INFO(q->slice().toJson());
        auto expectedJobKey = "/arango/Target/ToDo/" + jobId;
        REQUIRE(typeName(q->slice()) == "array" );
        REQUIRE(q->slice().length() == 1);
        REQUIRE(typeName(q->slice()[0]) == "array");
        // operations + preconditions
        REQUIRE(q->slice()[0].length() == 2);
        REQUIRE(typeName(q->slice()[0][0]) == "object");
        REQUIRE(q->slice()[0][0].length() == 2); // should do an entry in todo and failedservers
        REQUIRE(typeName(q->slice()[0][0].get(expectedJobKey)) == "object");

        auto job = q->slice()[0][0].get(expectedJobKey);
        REQUIRE(typeName(job.get("creator")) == "string");
        REQUIRE(typeName(job.get("type")) == "string");
        CHECK(job.get("type").copyString() == "activeFailover");
        REQUIRE(typeName(job.get("server")) == "string");
        CHECK(job.get("server").copyString() == LEADER);
        CHECK(typeName(job.get("jobId")) == "string");
        CHECK(job.get("jobId").copyString() == jobId);
        CHECK(typeName(job.get("timeCreated")) == "string");
        
        return fakeWriteResult;
      });

    When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
    
    auto& agent = mockAgent.get();
    Node snapshot = createNodeFromBuilder(base);
    ActiveFailoverJob job(snapshot(PREFIX), &agent, jobId, "tests", LEADER);
    
    REQUIRE(job.create());
    Verify(Method(mockAgent, write));
  }
  
  SECTION("The state is already 'GOOD' and 'Target/FailedServers' is still as in the snapshot. Violate: GOOD") {
    
    const char* tt = R"=({"arango":{"Supervision":{"Health":{"SNGL-leader":{"Status":"GOOD"}}}}})=";
    VPackBuilder overw = createBuilder(tt);
    VPackBuilder mod = VPackCollection::merge(base.slice(), overw.slice(), true);
    
    Mock<AgentInterface> mockAgent;
    When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
      INFO(q->slice().toJson());
      REQUIRE(typeName(q->slice()) == "array" );
      REQUIRE(q->slice().length() == 1);
      REQUIRE(typeName(q->slice()[0]) == "array");
      // operations + preconditions
      REQUIRE(q->slice()[0].length() == 2);
      REQUIRE(typeName(q->slice()[0][0]) == "object");
      REQUIRE(typeName(q->slice()[0][1]) == "object");
      
      auto writes = q->slice()[0][0];
      REQUIRE(typeName(writes.get("/arango/Target/ToDo/1")) == "object");
      REQUIRE(writes.get("/arango/Target/ToDo/1").get("server").copyString() == LEADER);
      
      auto precond = q->slice()[0][1];
      REQUIRE(typeName(precond.get("/arango/Supervision/Health/SNGL-leader/Status")) == "object");
      REQUIRE(precond.get("/arango/Supervision/Health/SNGL-leader/Status").get("old").copyString() == "BAD");
      REQUIRE(typeName(precond.get("/arango/Target/FailedServers").get("old")) == "object");
      
      return write_ret_t{false, "", std::vector<apply_ret_t> {APPLIED}, std::vector<index_t> {0}};
    });
    
    When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
    auto& agent = mockAgent.get();
    Node snapshot = createNodeFromBuilder(mod);
    ActiveFailoverJob job(snapshot(PREFIX), &agent, jobId, "unittest", LEADER);
    
    REQUIRE_FALSE(job.create());
    REQUIRE(job.status() == JOB_STATUS::NOTFOUND);
    Verify(Method(mockAgent,write));
  } // SECTION
  
  SECTION("Server is healthy again, job finishes") {
    
    const char* health = R"=({"arango":{"Supervision":{"Health":{"SNGL-leader":{"Status":"GOOD"}}},
                                        "Target":{"ToDo":{"1":{"jobId":"1","type":"activeFailover"}}}}})=";
    VPackBuilder mod = VPackCollection::merge(base.slice(), createBuilder(health).slice(), true);
    
    Mock<AgentInterface> mockAgent;
    When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
      REQUIRE(typeName(q->slice()) == "array" );
      REQUIRE(q->slice().length() == 1);
      REQUIRE(typeName(q->slice()[0]) == "array");
      // operations + preconditions
      REQUIRE(q->slice()[0].length() == 2);
      REQUIRE(typeName(q->slice()[0][0]) == "object");
      REQUIRE(typeName(q->slice()[0][1]) == "object");
      
      auto writes = q->slice()[0][0];
      REQUIRE(typeName(writes.get("/arango/Target/ToDo/1")) == "object");
      REQUIRE(writes.get("/arango/Target/ToDo/1").get("server").copyString() == LEADER);
      
      auto precond = q->slice()[0][1];
      REQUIRE(typeName(precond.get("/arango/Supervision/Health/SNGL-leader/Status")) == "object");
      REQUIRE(precond.get("/arango/Supervision/Health/SNGL-leader/Status").get("old").copyString() == "BAD");
      REQUIRE(typeName(precond.get("/arango/Target/FailedServers").get("old")) == "object");
      
      return fakeWriteResult;
    });
    When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
    auto& agent = mockAgent.get();
    Node snapshot = createNodeFromBuilder(mod); //snapshort contains GOOD leader
    
    ActiveFailoverJob job(snapshot(PREFIX), &agent, jobId, "unittest", LEADER);
    REQUIRE(job.create()); // we already put the TODO entry in the snapshot for finish
    REQUIRE(job.status() == JOB_STATUS::TODO);
    Verify(Method(mockAgent,write)).Exactly(1);
    
    When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
      // check that the job finishes now, without changing leader
      VPackSlice writes = q->slice()[0][0];
      REQUIRE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) == "string");
      REQUIRE(typeName(writes.get("/arango/Target/Finished/1")) == "object");
      REQUIRE_FALSE(writes.hasKey("/arango" + asyncReplLeader)); // no change to leader
      return fakeWriteResult;
    });
    
    REQUIRE(job.start(aborts));
    REQUIRE(job.status() == JOB_STATUS::FINISHED);
    Verify(Method(mockAgent,write)).Exactly(2);
    
  } // SECTION

  SECTION("Current leader is different from server in job, job finishes") {
    
    const char* health = R"=({"arango":{"Plan":{"AsyncReplication":{"Leader":"SNGL-follower1"}},
    "Target":{"ToDo":{"1":{"jobId":"1","type":"activeFailover"}}}}})=";
    VPackBuilder mod = VPackCollection::merge(base.slice(), createBuilder(health).slice(), true);
  
    Mock<AgentInterface> mockAgent;
    When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
      REQUIRE(typeName(q->slice()) == "array" );
      REQUIRE(q->slice().length() == 1);
      REQUIRE(typeName(q->slice()[0]) == "array");
      // operations + preconditions
      REQUIRE(q->slice()[0].length() == 2);
      REQUIRE(typeName(q->slice()[0][0]) == "object");
      REQUIRE(typeName(q->slice()[0][1]) == "object");
      
      auto writes = q->slice()[0][0];
      REQUIRE(typeName(writes.get("/arango/Target/ToDo/1")) == "object");
      REQUIRE(writes.get("/arango/Target/ToDo/1").get("server").copyString() == LEADER);
      
      auto precond = q->slice()[0][1];
      REQUIRE(typeName(precond.get("/arango/Supervision/Health/SNGL-leader/Status")) == "object");
      REQUIRE(precond.get("/arango/Supervision/Health/SNGL-leader/Status").get("old").copyString() == "BAD");
      REQUIRE(typeName(precond.get("/arango/Target/FailedServers").get("old")) == "object");
      
      return fakeWriteResult;
    });
    When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
    auto& agent = mockAgent.get();
    Node snapshot = createNodeFromBuilder(mod); //snapshort contains different leader
  
    ActiveFailoverJob job(snapshot(PREFIX), &agent, jobId, "unittest", LEADER);
    REQUIRE(job.create()); // we already put the TODO entry in the snapshot for finish
    REQUIRE(job.status() == JOB_STATUS::TODO);
    Verify(Method(mockAgent,write)).Exactly(1);
  
    When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
      // check that the job finishes now, without changing leader
      VPackSlice writes = q->slice()[0][0];
      REQUIRE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) == "string");
      REQUIRE(typeName(writes.get("/arango/Target/Finished/1")) == "object");
      REQUIRE_FALSE(writes.hasKey("/arango" + asyncReplLeader)); // no change to leader
      return fakeWriteResult;
    });
  
    REQUIRE(job.start(aborts));
    REQUIRE(job.status() == JOB_STATUS::FINISHED);
    Verify(Method(mockAgent,write)).Exactly(2);
  
  } // SECTION

  SECTION("no in-sync follower found, job retries") {
        
    // follower follows wrong leader
    const char* noInSync = R"=({"arango":{"AsyncReplication":{"SNGL-follower1":{"leader":"abc","lastTick":9}}}})=";
    trans_ret_t fakeTransient {true, "", 1, 0, std::make_shared<Builder>(createBuilder(noInSync))};
    
    Mock<AgentInterface> mockAgent;
    When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
      REQUIRE(typeName(q->slice()) == "array" );
      REQUIRE(q->slice().length() == 1);
      REQUIRE(typeName(q->slice()[0]) == "array");
      // operations + preconditions
      REQUIRE(q->slice()[0].length() == 2);
      REQUIRE(typeName(q->slice()[0][0]) == "object");
      REQUIRE(typeName(q->slice()[0][1]) == "object");
      
      auto writes = q->slice()[0][0];
      REQUIRE(typeName(writes.get("/arango/Target/ToDo/1")) == "object");
      REQUIRE(writes.get("/arango/Target/ToDo/1").get("server").copyString() == LEADER);
      
      auto precond = q->slice()[0][1];
      REQUIRE(typeName(precond.get("/arango/Supervision/Health/SNGL-leader/Status")) == "object");
      REQUIRE(precond.get("/arango/Supervision/Health/SNGL-leader/Status").get("old").copyString() == "BAD");
      REQUIRE(typeName(precond.get("/arango/Target/FailedServers").get("old")) == "object");
      
      return fakeWriteResult;
    });
    When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
    auto& agent = mockAgent.get();
    Node snapshot = createNodeFromBuilder(base);

    ActiveFailoverJob job(snapshot(PREFIX), &agent, jobId, "unittest", LEADER);
    REQUIRE(job.create()); // we already put the TODO entry in the snapshot for finish
    REQUIRE(job.status() == JOB_STATUS::TODO);
    Verify(Method(mockAgent,write)).Exactly(1);
    
    When(Method(mockAgent, transient)).Return(fakeTransient);
    When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
      // check that the job fails now
      auto writes = q->slice()[0][0];
      REQUIRE(std::string(writes.get("/arango/Target/ToDo/1").get("op").typeName()) == "string"); \
      CHECK(std::string(writes.get("/arango/Target/Failed/1").typeName()) == "object");
      return fakeWriteResult;
    });

    REQUIRE_FALSE(job.start(aborts));
    // job status stays on TODO and can retry later
    REQUIRE(job.status() == JOB_STATUS::TODO);
    Verify(Method(mockAgent,transient)).Exactly(Once);
    Verify(Method(mockAgent,write)).Exactly(Once); // finish is not called

  } // SECTION

  SECTION("follower with best tick value used, job succeeds") {
    
    // 2 in-sync followers, follower1 should be used
    trans_ret_t fakeTransient {true, "", 1, 0, std::make_shared<Builder>(createBuilder(transient))};
    
    Mock<AgentInterface> mockAgent;
    When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
      REQUIRE(typeName(q->slice()) == "array" );
      REQUIRE(q->slice().length() == 1);
      REQUIRE(typeName(q->slice()[0]) == "array");
      // we always simply override! no preconditions...
      REQUIRE(q->slice()[0].length() == 2);
      REQUIRE(typeName(q->slice()[0][0]) == "object");
      REQUIRE(typeName(q->slice()[0][1]) == "object");
      
      auto writes = q->slice()[0][0];
      REQUIRE(typeName(writes.get("/arango/Target/ToDo/1")) == "object");
      REQUIRE(writes.get("/arango/Target/ToDo/1").get("server").copyString() == LEADER);
      
      auto precond = q->slice()[0][1];
      REQUIRE(typeName(precond.get("/arango/Supervision/Health/SNGL-leader/Status")) == "object");
      REQUIRE(precond.get("/arango/Supervision/Health/SNGL-leader/Status").get("old").copyString() == "BAD");
      REQUIRE(typeName(precond.get("/arango/Target/FailedServers").get("old")) == "object");
      
      return fakeWriteResult;
    });
    When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
    auto& agent = mockAgent.get();
    Node snapshot = createNodeFromBuilder(base);
    
    ActiveFailoverJob job(snapshot(PREFIX), &agent, jobId, "unittest", LEADER);
    REQUIRE(job.create()); // we already put the TODO entry in the snapshot for finish
    REQUIRE(job.status() == JOB_STATUS::TODO);
    Verify(Method(mockAgent,write)).Exactly(1);
    
    When(Method(mockAgent, transient)).Return(fakeTransient);
    When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
      REQUIRE(typeName(q->slice()) == "array" );
      REQUIRE(q->slice().length() == 1);
      REQUIRE(typeName(q->slice()[0]) == "array");
      // we always simply override! no preconditions...
      REQUIRE(q->slice()[0].length() == 2);
      REQUIRE(typeName(q->slice()[0][0]) == "object");
      REQUIRE(typeName(q->slice()[0][1]) == "object");
      
      INFO(q->toString());
      
      // check that the job succeeds now
      auto writes = q->slice()[0][0];
      REQUIRE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) == "string"); \
      REQUIRE(typeName(writes.get("/arango/Target/Finished/1")) == "object");
      REQUIRE(typeName(writes.get("/arango/Plan/AsyncReplication/Leader")) == "string");
      REQUIRE(writes.get("/arango/Plan/AsyncReplication/Leader").copyString() == FOLLOWER1);
      
      auto precond = q->slice()[0][1];
      REQUIRE(typeName(precond.get("/arango/Supervision/Health/SNGL-leader/Status")) == "object");
      REQUIRE(precond.get("/arango/Supervision/Health/SNGL-leader/Status").get("old").copyString() == "FAILED");
      REQUIRE(precond.get("/arango/Supervision/Health/SNGL-follower1/Status").get("old").copyString() == "GOOD");
      REQUIRE(precond.get("/arango/Plan/AsyncReplication/Leader").get("old").copyString() == LEADER);
      
      return fakeWriteResult;
    });
        
    REQUIRE(job.start(aborts));
    // job status stays on TODO and can retry later
    REQUIRE(job.status() == JOB_STATUS::FINISHED);
    Verify(Method(mockAgent,transient)).Exactly(1);
    Verify(Method(mockAgent,write)).Exactly(2);
    
  } // SECTION

};

}}}
