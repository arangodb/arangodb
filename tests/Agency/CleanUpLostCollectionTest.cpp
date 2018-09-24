////////////////////////////////////////////////////////////////////////////////
/// @brief test case for cleanupLostCollection job
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
/// @author Lars Maier
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

namespace arangodb {
namespace tests {
namespace clean_up_lost_collection_test {

const char *agency =
#include "CleanUpLostCollectionTest.json"
;

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

TEST_CASE("CleanUpLostCollectionTest", "[agency][supervision]") {
auto baseStructure = createRootNode();
write_ret_t fakeWriteResult {true, "", std::vector<bool> {true}, std::vector<index_t> {1}};
std::string const jobId = "1";

SECTION("clean up a lost collection when the leader is failed") {

  Mock<AgentInterface> mockAgent;


  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, bool d) -> write_ret_t {
    INFO(q->slice().toJson());
    /*auto expectedJobKey = "/arango/Target/Finish/" + jobId;
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
    CHECK(std::string(job.get("timeCreated").typeName()) == "string");*/

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface &agent = mockAgent.get();

  Supervision::cleanupLostCollections(baseStructure("arango"), &agent, jobId);
  Verify(Method(mockAgent, write));
}

}
}
}
}
