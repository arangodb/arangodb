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
const std::string FOLLOWER = "SNGL-follower1";
const std::string FOLLOWER2 = "SNGL-follower2";

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

Node createNode(char const* c) {
  return createNodeFromBuilder(createBuilder(c));
}

Node createRootNode() {
  return createNode(agency);
}

typedef std::function<std::unique_ptr<Builder>(
  Slice const&, std::string const&)> TestStructType;

inline static std::string typeName (Slice const& slice) {
  return std::string(slice.typeName());
}

TEST_CASE("ActiveFailover", "[agency][supervision]") {
  
  auto baseStructure = createRootNode();
  arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
    
  Builder builder;
  baseStructure.toBuilder(builder);

  std::string jobId = "1";

  write_ret_t fakeWriteResult {true, "", std::vector<bool> {true}, std::vector<index_t> {1}};
  trans_ret_t fakeTransResult {true, "", 1, 0, std::make_shared<Builder>(createBuilder(transient))};
  
  SECTION("creating a job should create a job in todo") {
    Mock<AgentInterface> mockAgent;

    When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, bool d) -> write_ret_t {
        INFO(q->slice().toJson());
        auto expectedJobKey = "/arango/Target/ToDo/" + jobId;
        REQUIRE(typeName(q->slice()) == "array" );
        REQUIRE(q->slice().length() == 1);
        REQUIRE(typeName(q->slice()[0]) == "array");
        REQUIRE(q->slice()[0].length() == 2); // with preconditions...
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
    auto  activeFailover = ActiveFailoverJob(baseStructure(PREFIX), &agent, jobId, "tests", LEADER);
    
    activeFailover.create();
    Verify(Method(mockAgent, write));

  }
  
  //When(Method(mockAgent, transient)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  
  /*SECTION("The state is still 'BAD' and 'Target/FailedServers' is still as in the snapshot. Violate: GOOD") {
    
    auto createTestStructure = [&](Slice const& s, std::string const& path) -> std::unique_ptr<Builder> {
      
      std::unique_ptr<Builder> builder;
      if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION) {
        return builder;
      }
      builder = std::make_unique<Builder>();
      
      if (s.isObject()) {
        VPackObjectBuilder b(builder.get());
        for (auto const& it: VPackObjectIterator(s)) {
          auto childBuilder =
          createTestStructure(it.value, path + "/" + it.key.copyString());
          if (childBuilder) {
            builder->add(it.key.copyString(), childBuilder->slice());
          }
        }
        if (path == "/arango/Supervision/Health/leader") {
          builder->add("Status", VPackValue("GOOD"));
        }
      } else {
        builder->add(s);
      }
      
      return builder;
    };
    
    auto builder = createTestStructure(agency.toBuilder().slice(), "");
    REQUIRE(builder);
    Node agency = createNodeFromBuilder(*builder);
    
    Mock<AgentInterface> mockAgent;
    When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, bool d) -> write_ret_t {
      INFO(q->slice().toJson());
      REQUIRE(typeName(q->slice()) == "array" );
      REQUIRE(q->slice().length() == 1);
      REQUIRE(typeName(q->slice()[0]) == "array");
      // we always simply override! no preconditions...
      REQUIRE(q->slice()[0].length() == 2);
      REQUIRE(typeName(q->slice()[0][0]) == "object");
      REQUIRE(typeName(q->slice()[0][1]) == "object");
      
      auto writes = q->slice()[0][0];
      REQUIRE(typeName(writes.get("/arango/Target/ToDo/1")) == "object");
      REQUIRE(writes.get("/arango/Target/ToDo/1").get("server").copyString() == SHARD_LEADER);
      
      auto precond = q->slice()[0][1];
      REQUIRE(typeName(precond.get("/arango/Supervision/Health/leader/Status")) == "object");
      REQUIRE(precond.get("/arango/Supervision/Health/leader/Status").get("old").copyString() == "BAD");
      REQUIRE(typeName(precond.get("/arango/Target/FailedServers").get("old")) == "object");
      
      return fakeWriteResult;
    });
    
    When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
    auto& agent = mockAgent.get();
    FailedServer(agency(PREFIX), &agent, jobId, "unittest", SHARD_LEADER).create();
    
    Verify(Method(mockAgent,write));
  }*/ // SECTION

};

}}}
