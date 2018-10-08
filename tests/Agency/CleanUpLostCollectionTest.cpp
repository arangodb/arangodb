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
write_ret_t fakeWriteResult {true, "", std::vector<apply_ret_t> {APPLIED}, std::vector<index_t> {1}};
std::string const jobId = "1";

SECTION("clean up a lost collection when the leader is failed") {

  Mock<AgentInterface> mockAgent;


  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO(q->slice().toJson());

    // What do we expect here:
    // We expect two transactions:
    //  1. Transaction:
    //    - Operation:
    //        delete /arango/Current/Collections/database/collection/s99
    //        push {
    //            "creator": "supervision",
    //            "jobId": "1",
    //            "server": "s99",
    //            "timeCreated": "2018-09-26T09:25:33Z",
    //            "type": "cleanUpLostCollection"
    //            }
    //    - Preconditions:
    //        not empty: /arango/Current/Collections/database/collection/s99
    //        empty: /arango/Plan/Collections/database/collection/shards/s99
    //        old: /arango/Supervision/Health/leader/Status == "FAILED"

    auto const& trxs = q->slice();
    REQUIRE(trxs.length() == 1);

    auto const& trx1 = trxs[0];
    REQUIRE(trx1.length() == 2); // Operation and Precondition
    auto const& op1   = trx1[0];
    auto const& pre1  = trx1[1];
    REQUIRE(op1.isObject());
    REQUIRE(op1.hasKey("/arango/Current/Collections/database/collection/s99"));
    auto const& op1delete = op1.get("/arango/Current/Collections/database/collection/s99");

    REQUIRE(op1delete.isObject());
    REQUIRE(op1delete.hasKey("op"));
    REQUIRE(op1delete.get("op").isEqualString("delete"));

    REQUIRE(op1.hasKey("/arango/Target/Finished"));
    auto const& op1push = op1.get("/arango/Target/Finished");
    REQUIRE(op1push.hasKey("new"));
    auto const& op1new = op1push.get("new");
    REQUIRE(op1new.get("creator").isEqualString("supervision"));
    REQUIRE(op1new.get("jobId").isEqualString("1"));
    REQUIRE(op1new.get("server").isEqualString("s99"));
    REQUIRE(op1new.get("timeCreated").isString());
    REQUIRE(op1new.get("type").isEqualString("cleanUpLostCollection"));

    REQUIRE(op1push.hasKey("op"));
    REQUIRE(op1push.get("op").isEqualString("push"));

    REQUIRE(pre1.hasKey("/arango/Current/Collections/database/collection/s99"));
    REQUIRE(pre1.hasKey("/arango/Plan/Collections/database/collection/shards/s99"));
    REQUIRE(pre1.hasKey("/arango/Supervision/Health/leader/Status"));

    REQUIRE(pre1.get("/arango/Current/Collections/database/collection/s99")
      .get("oldEmpty").isFalse());
    REQUIRE(pre1.get("/arango/Plan/Collections/database/collection/shards/s99")
      .get("oldEmpty").isTrue());
    REQUIRE(pre1.get("/arango/Supervision/Health/leader/Status")
      .get("old").isEqualString("FAILED"));

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
