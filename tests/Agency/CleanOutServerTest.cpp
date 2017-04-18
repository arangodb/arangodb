////////////////////////////////////////////////////////////////////////////////
/// @brief test case for CleanOutServer job
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

#include "Agency/CleanOutServer.h"
#include "Agency/AgentInterface.h"
#include "Agency/Node.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::consensus;
using namespace fakeit;

namespace arangodb {
namespace tests {
namespace cleanout_server_test {

const std::string PREFIX = "arango";
const std::string SERVER = "server";
const std::string JOBID = "1";

typedef std::function<std::unique_ptr<Builder>(
  Slice const&, std::string const&)>TestStructureType;

const char *agency =
#include "CleanOutServerTest.json"
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

Node createAgency() {
  return createNode(agency)("arango");
}

Node createAgency(TestStructureType const& createTestStructure) {
  auto node = createNode(agency);
  auto finalAgency = createTestStructure(node.toBuilder().slice(), "");

  auto finalAgencyNode = createNodeFromBuilder(*finalAgency);
  return finalAgencyNode("arango");
}

VPackBuilder createJob(std::string const& server) {
  VPackBuilder builder;
  VPackObjectBuilder a(&builder);
  {
    builder.add("creator", VPackValue("unittest"));
    builder.add("type", VPackValue("cleanOutServer"));
    builder.add("server", VPackValue(server));
    builder.add("jobId", VPackValue(JOBID));
    builder.add("timeCreated", VPackValue(
                           timepointToString(std::chrono::system_clock::now())));
  }
  return builder;
}

TEST_CASE("CleanOutServer", "[agency][supervision]") {
  auto baseStructure = createRootNode();
    
  write_ret_t fakeWriteResult {true, "", std::vector<bool> {true}, std::vector<index_t> {1}};
  auto transBuilder = std::make_shared<Builder>();
  { VPackArrayBuilder a(transBuilder.get());
    transBuilder->add(VPackValue((uint64_t)1)); }

  trans_ret_t fakeTransResult {true, "", 1, 0, transBuilder};

SECTION("cleanout server should not throw when doing bullshit instanciating") {
  Mock<AgentInterface> mockAgent;
  AgentInterface &agent = mockAgent.get();

  Node agency = createAgency();
  // should not throw
  REQUIRE_NOTHROW(CleanOutServer(
    agency,
    &agent,
    JOBID,
    "unittest",
    "wurstserver"
  ));
}

SECTION("cleanout server should fail if the server does not exist") {
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
        builder->add(JOBID, createJob("bogus").slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q) -> write_ret_t {
    INFO("WRITE: " << q->toJson());
    
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

  Node agency = createAgency(createTestStructure);
  INFO("AGENCY: " << agency.toJson());
  // should not throw
  auto cleanOutServer = CleanOutServer(
    agency,
    &agent,
    JOB_STATUS::TODO,
    JOBID
  );
  cleanOutServer.start();
  Verify(Method(mockAgent, write));
  Verify(Method(mockAgent, waitFor));
}

};

}
}
}