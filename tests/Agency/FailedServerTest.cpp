////////////////////////////////////////////////////////////////////////////////
/// @brief test case for FailedServer job
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
/// @author Kaveh Vahedipour
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
#include "catch.hpp"
#include "fakeit.hpp"

#include "Agency/AddFollower.h"
#include "Agency/FailedServer.h"
#include "Agency/MoveShard.h"
#include "Agency/AgentInterface.h"
#include "Agency/Node.h"
#include "lib/Basics/StringUtils.h"

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
namespace failed_server_test {

const std::string PREFIX = "/arango";
const std::string DATABASE = "database";
const std::string COLLECTION = "collection";
const std::string SHARD = "shard";
const std::string SHARD_LEADER = "leader";
const std::string SHARD_FOLLOWER1 = "follower1";
const std::string SHARD_FOLLOWER2 = "follower2";
const std::string FREE_SERVER = "free";
const std::string FREE_SERVER2 = "free2";

bool aborts = false;

typedef std::function<std::unique_ptr<Builder>(
  Slice const&, std::string const&)>TestStructureType;

const char *agency =
#include "FailedServerTest.json"
;

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

inline std::string typeName(Slice const& slice) {
  return std::string(slice.typeName());
}

TEST_CASE("FailedServer", "[agency][supervision]") {

  std::string const jobId = "1";

  auto transBuilder = std::make_shared<Builder>();
  { VPackArrayBuilder a(transBuilder.get());
    transBuilder->add(VPackValue((uint64_t)1)); }
    
  auto agency = createRootNode();
  write_ret_t fakeWriteResult {
    true, "", std::vector<apply_ret_t> {APPLIED}, std::vector<index_t> {1}};
  trans_ret_t fakeTransResult {true, "", 1, 0, transBuilder};
  
  SECTION("creating a job should create a job in todo") {
    Mock<AgentInterface> mockAgent;
    
    std::string jobId = "1";
    When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
        INFO(q->slice().toJson());
        auto expectedJobKey = PREFIX + toDoPrefix + jobId;
        REQUIRE(typeName(q->slice()) == "array");
        REQUIRE(q->slice().length() == 1);
        REQUIRE(typeName(q->slice()[0]) == "array");
        REQUIRE(q->slice()[0].length() == 2); 
        REQUIRE(typeName(q->slice()[0][0]) == "object");
        REQUIRE(q->slice()[0][0].length() == 2); 
        REQUIRE(typeName(q->slice()[0][0].get(expectedJobKey)) == "object");
        
        auto job = q->slice()[0][0].get(expectedJobKey);
        REQUIRE(typeName(job.get("creator")) == "string");
        REQUIRE(typeName(job.get("type")) == "string");
        CHECK(job.get("type").copyString() == "failedServer");
        REQUIRE(typeName(job.get("server")) == "string");
        CHECK(job.get("server").copyString() == SHARD_LEADER);
        CHECK(typeName(job.get("jobId")) == "string");
        CHECK(typeName(job.get("timeCreated")) == "string");
        
        return fakeWriteResult;
      });
    When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
    auto &agent = mockAgent.get();
    auto builder = agency.toBuilder();
    FailedServer(
      agency(PREFIX), &agent, jobId, "unittest", SHARD_LEADER).create();
    Verify(Method(mockAgent, write));
    
  } // SECTION
  
  SECTION("The state is still 'BAD' and 'Target/FailedServers' is still as in the snapshot. Violate: GOOD") {

    TestStructureType createTestStructure = [&](
      Slice const& s, std::string const& path) {

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
    When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
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
    FailedServer(
      agency(PREFIX), &agent, jobId, "unittest", SHARD_LEADER).create();

    Verify(Method(mockAgent,write));
    
  } // SECTION

  SECTION("The state is still 'BAD' and 'Target/FailedServers' is still as in the snapshot. Violate: FAILED") {

    TestStructureType createTestStructure = [&](
      Slice const& s, std::string const& path) {

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
          builder->add("Status", VPackValue("FAILED"));
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
    When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
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
    FailedServer(
      agency(PREFIX), &agent, jobId, "unittest", SHARD_LEADER).create();
    
  } // SECTION

  SECTION("The state is still 'FAILED' and 'Target/FailedServers' is still as in the snapshot") {

    TestStructureType createTestStructure = [&](
      Slice const& s, std::string const& path) {

      std::unique_ptr<Builder> builder;
      if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION) {
        return builder;
      }
      builder = std::make_unique<Builder>();
      
      if (s.isObject()) {
        VPackObjectBuilder b(builder.get());
        for (auto const& it: VPackObjectIterator(s)) {
          auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
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
        builder->close();
      } else {
        builder->add(s);
      }
      
      return builder;
    };
    
    auto builder = createTestStructure(agency.toBuilder().slice(), "");
    REQUIRE(builder);
    Node agency = createNodeFromBuilder(*builder);
    
    Mock<AgentInterface> mockAgent;
    When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
        INFO(q->slice().toJson());
        REQUIRE(typeName(q->slice()) == "array" );
        REQUIRE(q->slice().length() == 1);
        REQUIRE(typeName(q->slice()[0]) == "array");
        // we always simply override! no preconditions...
        REQUIRE(q->slice()[0].length() == 1); 
        REQUIRE(typeName(q->slice()[0][0]) == "object");
        
        auto writes = q->slice()[0][0];
        REQUIRE(typeName(writes.get("/arango/Target/ToDo/1")) == "object");
        REQUIRE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) == "string");
        CHECK(writes.get("/arango/Target/ToDo/1").get("op").copyString() == "delete");
        CHECK(typeName(writes.get("/arango/Target/Pending/1")) == "object");
        return fakeWriteResult;
      });
    
    When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
    auto& agent = mockAgent.get();
    FailedServer(agency("arango"), &agent, JOB_STATUS::TODO, jobId).start(aborts);

    Verify(Method(mockAgent,write));

    
  } // SECTION

  SECTION("The state is still 'FAILED' and 'Target/FailedServers' is PART 2") {

    TestStructureType createTestStructure = [&](
      Slice const& s, std::string const& path) {

      std::unique_ptr<Builder> builder;
      if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION) {
        return builder;
      }
      builder = std::make_unique<Builder>();
      
      if (s.isObject()) {
        VPackObjectBuilder b(builder.get());
        for (auto const& it: VPackObjectIterator(s)) {
          auto childBuilder = createTestStructure(it.value, path + "/" + it.key.copyString());
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
        builder->close();
      } else {
        if (path == "/arango/Supervision/Health/leader/Status") {
          builder->add("/arango/Supervision/Health/leader/Status", VPackValue("FAILED"));
        }
        builder->add(s);
      }
      
      return builder;
    };
    
    auto builder = createTestStructure(agency.toBuilder().slice(), "");
    REQUIRE(builder);
    Node agency = createNodeFromBuilder(*builder);
    
    Mock<AgentInterface> mockAgent;
    When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
        INFO(q->slice().toJson());
        REQUIRE(typeName(q->slice()) == "array" );
        REQUIRE(q->slice().length() == 1);
        REQUIRE(typeName(q->slice()[0]) == "array");
        // we always simply override! no preconditions...
        REQUIRE(q->slice()[0].length() == 1); 
        REQUIRE(typeName(q->slice()[0][0]) == "object");
        
        auto writes = q->slice()[0][0];
        REQUIRE(typeName(writes.get("/arango/Target/ToDo/1")) == "object");
        REQUIRE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) == "string");
        CHECK(writes.get("/arango/Target/ToDo/1").get("op").copyString() == "delete");
        CHECK(typeName(writes.get("/arango/Target/Pending/1")) == "object");
        return fakeWriteResult;
      });
    
    When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
    auto& agent = mockAgent.get();
    FailedServer(agency("arango"), &agent, JOB_STATUS::TODO, jobId).start(aborts);

    Verify(Method(mockAgent,write));

    
  } // SECTION

} // TEST_CASE

}}} // namespaces 
