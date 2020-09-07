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
/// @author Kaveh Vahedipour
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "Agency/AgentInterface.h"
#include "Agency/FailedLeader.h"
#include "Agency/MoveShard.h"
#include "Agency/Node.h"
#include "Agency/RemoveFollower.h"
#include "Basics/StringUtils.h"
#include "Random/RandomGenerator.h"

#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <iostream>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::consensus;
using namespace fakeit;

namespace arangodb {
namespace tests {
namespace remove_follower_test {

const std::string PREFIX = "arango";
const std::string DATABASE = "database";
const std::string COLLECTION = "collection1";
const std::string CLONE = "collection2";
const std::string SHARD = "s1";
const std::string SHARD_LEADER = "leader";
const std::string SHARD_FOLLOWER1 = "follower1";
const std::string SHARD_FOLLOWER2 = "follower2";
const std::string FREE_SERVER = "free";
const std::string FREE_SERVER2 = "free2";

bool aborts = false;

const char* agency =
#include "RemoveFollowerTest.json"
    ;
std::string const agencyLarge =
#include "RemoveFollowerTestLarge.json"
    ;
const char* todo =
#include "RemoveFollowerTestToDo.json"
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

Node createNode(char const* c) {
  return createNodeFromBuilder(createBuilder(c));
}

Node createRootNode() { return createNode(agency); }

typedef std::function<std::unique_ptr<Builder>(Slice const&, std::string const&)> TestStructType;

inline static std::string typeName(Slice const& slice) {
  return std::string(slice.typeName());
}

class RemoveFollowerTest : public ::testing::Test {
 protected:
  Node baseStructure;
  Builder builder;
  std::string jobId;
  write_ret_t fakeWriteResult;
  trans_ret_t fakeTransResult;

  RemoveFollowerTest()
      : baseStructure(createRootNode()),
        jobId("1"),
        fakeWriteResult(true, "", std::vector<apply_ret_t>{APPLIED},
                        std::vector<index_t>{1}),
        fakeTransResult(true, "", 1, 0, std::make_shared<Builder>()) {
    arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
    baseStructure.toBuilder(builder);
  }
};

TEST_F(RemoveFollowerTest, creating_a_job_should_create_a_job_in_todo) {
  Mock<AgentInterface> mockAgent;

  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    auto expectedJobKey = "/arango/Target/ToDo/" + jobId;
    EXPECT_EQ(typeName(q->slice()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(typeName(q->slice()[0]), "array");
    EXPECT_EQ(q->slice()[0].length(), 1);  // we always simply override! no preconditions...
    EXPECT_EQ(typeName(q->slice()[0][0]), "object");
    EXPECT_EQ(q->slice()[0][0].length(), 1);  // should ONLY do an entry in todo
    EXPECT_EQ(typeName(q->slice()[0][0].get(expectedJobKey)), "object");

    auto job = q->slice()[0][0].get(expectedJobKey);
    EXPECT_EQ(typeName(job.get("creator")), "string");
    EXPECT_EQ(typeName(job.get("type")), "string");
    EXPECT_EQ(job.get("type").copyString(), "removeFollower");
    EXPECT_EQ(typeName(job.get("database")), "string");
    EXPECT_EQ(job.get("database").copyString(), DATABASE);
    EXPECT_EQ(typeName(job.get("collection")), "string");
    EXPECT_EQ(job.get("collection").copyString(), COLLECTION);
    EXPECT_EQ(typeName(job.get("shard")), "string");
    EXPECT_EQ(job.get("shard").copyString(), SHARD);
    EXPECT_EQ(typeName(job.get("jobId")), "string");
    EXPECT_EQ(typeName(job.get("timeCreated")), "string");

    return fakeWriteResult;
  });

  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  auto removeFollower = RemoveFollower(baseStructure, &agent, jobId, "unittest",
                                       DATABASE, COLLECTION, SHARD);

  removeFollower.create();
}

TEST_F(RemoveFollowerTest, collection_still_exists_if_missing_job_is_finished_move_to_finished) {
  TestStructType createTestStructure = [&](Slice const& s, std::string const& path) {
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
        builder->add(jobId, createBuilder(todo).slice());
      }
    } else {
      builder->add(s);
    }

    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_EQ(typeName(q->slice()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(typeName(q->slice()[0]), "array");
    // we always simply override! no preconditions...
    EXPECT_EQ(q->slice()[0].length(), 1);
    EXPECT_EQ(typeName(q->slice()[0][0]), "object");

    auto writes = q->slice()[0][0];
    EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
    EXPECT_TRUE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) ==
                "string");
    EXPECT_TRUE(writes.get("/arango/Target/ToDo/1").get("op").copyString() ==
                "delete");
    EXPECT_EQ(typeName(writes.get("/arango/Target/Finished/1")), "object");
    return fakeWriteResult;
  });

  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  RemoveFollower(agency("arango"), &agent, JOB_STATUS::TODO, jobId).start(aborts);
}

TEST_F(RemoveFollowerTest,
       if_collection_has_a_nonempty_distributeshardslike_attribute_the_job_immediately_fails) {
  TestStructType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
    if (s.isObject()) {
      VPackObjectBuilder b(builder.get());
      for (auto it : VPackObjectIterator(s)) {
        auto childBuilder =
            createTestStructure(it.value, path + "/" + it.key.copyString());
        if (childBuilder) {
          builder->add(it.key.copyString(), childBuilder->slice());
        }
      }
      if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION) {
        builder->add("distributeShardsLike", VPackValue("PENG"));
      }
      if (path == "/arango/Target/ToDo") {
        builder->add(jobId, createBuilder(todo).slice());
      }
    } else {
      builder->add(s);
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  auto agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_EQ(typeName(q->slice()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(typeName(q->slice()[0]), "array");
    EXPECT_EQ(q->slice()[0].length(), 1);  // we always simply override! no preconditions...
    EXPECT_EQ(typeName(q->slice()[0][0]), "object");

    auto writes = q->slice()[0][0];
    EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
    EXPECT_TRUE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) ==
                "string");
    EXPECT_TRUE(writes.get("/arango/Target/ToDo/1").get("op").copyString() ==
                "delete");
    EXPECT_EQ(typeName(writes.get("/arango/Target/Failed/1")), "object");
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  RemoveFollower(agency("arango"), &agent, JOB_STATUS::TODO, jobId).start(aborts);
}

TEST_F(RemoveFollowerTest, condition_still_holds_for_the_mentioned_collections_move_to_finished) {
  TestStructType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
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
        builder->add(jobId, createBuilder(todo).slice());
      }
    } else {
      if (path == "/arango/Plan/Collections/" + DATABASE + "/" + COLLECTION +
                      "/shards/" + SHARD) {
        VPackArrayBuilder a(builder.get());
        for (auto const& serv : VPackArrayIterator(s)) {
          if (serv.copyString() != SHARD_FOLLOWER1 && serv.copyString() != SHARD_FOLLOWER2) {
            builder->add(serv);
          }
        }
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  auto agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_EQ(typeName(q->slice()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(typeName(q->slice()[0]), "array");
    EXPECT_EQ(q->slice()[0].length(), 1);  // we always simply override! no preconditions...
    EXPECT_EQ(typeName(q->slice()[0][0]), "object");

    auto writes = q->slice()[0][0];
    EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
    EXPECT_TRUE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) ==
                "string");
    EXPECT_TRUE(writes.get("/arango/Target/ToDo/1").get("op").copyString() ==
                "delete");
    EXPECT_EQ(typeName(writes.get("/arango/Target/Finished/1")), "object");
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  RemoveFollower(agency("arango"), &agent, JOB_STATUS::TODO, jobId).start(aborts);
}

TEST_F(RemoveFollowerTest,
       compute_the_list_all_shards_of_colleciton_pairs_that_correspond_to_distributeshardslike_attributes) {
  TestStructType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
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
        builder->add(jobId, createBuilder(todo).slice());
      }
      if (path == "/arango/Plan/Collections/" + DATABASE + "/" + CLONE) {
        builder->add("distributeShardsLike", VPackValue(COLLECTION));
      }
    } else {
      builder->add(s);
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  Node agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_EQ(typeName(q->slice()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(typeName(q->slice()[0]), "array");
    EXPECT_EQ(q->slice()[0].length(), 2);  // precondition
    EXPECT_EQ(typeName(q->slice()[0][0]), "object");

    auto writes = q->slice()[0][0];
    EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
    EXPECT_TRUE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) ==
                "string");
    EXPECT_TRUE(writes.get("/arango/Target/ToDo/1").get("op").copyString() ==
                "delete");
    EXPECT_EQ(writes.get("/arango/Target/Finished/1").get("collection").copyString(), COLLECTION);
    EXPECT_EQ(typeName(writes.get("/arango/Target/Failed/1")), "none");

    auto precond = q->slice()[0][1];
    EXPECT_EQ(typeName(precond), "object");
    EXPECT_TRUE(
        typeName(precond.get("/arango/Supervision/Health/follower1/Status")) ==
        "object");

    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  AgentInterface& agent = mockAgent.get();
  RemoveFollower(agency("arango"), &agent, JOB_STATUS::TODO, jobId).start(aborts);
}

TEST_F(RemoveFollowerTest, all_good_should_remove_folower) {
  TestStructType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
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
        builder->add(jobId, createBuilder(todo).slice());
      }
    } else {
      builder->add(s);
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  auto agency = createNodeFromBuilder(*builder);

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_EQ(typeName(q->slice()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(typeName(q->slice()[0]), "array");
    EXPECT_EQ(q->slice()[0].length(), 2);  // we always simply override! no preconditions...
    EXPECT_EQ(typeName(q->slice()[0][0]), "object");

    auto writes = q->slice()[0][0];
    EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
    EXPECT_TRUE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) ==
                "string");
    EXPECT_TRUE(writes.get("/arango/Target/ToDo/1").get("op").copyString() ==
                "delete");
    EXPECT_EQ(typeName(writes.get("/arango/Target/Finished/1")), "object");
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  RemoveFollower(agency("arango"), &agent, JOB_STATUS::TODO, jobId).start(aborts);

  EXPECT_NO_THROW(Verify(Method(mockAgent, write)));
}

TEST(RemoveFollowerLargeTest, an_agency_with_12_dbservers) {
  auto baseStructure = createNode(agencyLarge.c_str());
  arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
  std::string jobId = "1";
  write_ret_t fakeWriteResult{true, "", std::vector<apply_ret_t>{APPLIED},
                              std::vector<index_t>{1}};
  trans_ret_t fakeTransResult{true, "", 1, 0, std::make_shared<Builder>()};

  TestStructType createTestStructure = [&](Slice const& s, std::string const& path) {
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();
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
        builder->add(jobId, createBuilder(todo).slice());
      }
    } else {
      builder->add(s);
    }
    return builder;
  };

  auto builder = createTestStructure(baseStructure.toBuilder().slice(), "");
  ASSERT_TRUE(builder);
  auto agency = createNodeFromBuilder(*builder);

  // The reason for using so much DBServers is to make it nearly impossible
  // for the test to pass by accident. Trying with lower numbers
  // (say remove 5 out of 10) seemed to indicate that the implementation
  // of unordered_map makes it, at least for small numbers, likely
  // that the last added elements appear first when iterating over it.
  // When choosing different names for the 10 DBServers, I expected the
  // order to change (and it did), but in 4 out of 5 tries made the correct
  // DBServers were removed, while I expected the chance for that to be
  // 1 / (10 choose 5) = 1/252.
  // Thus I increased the agency to 100 DBServers with a replicationFactor
  // of 50.

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    EXPECT_EQ(typeName(q->slice()), "array");
    EXPECT_EQ(q->slice().length(), 1);
    EXPECT_EQ(typeName(q->slice()[0]), "array");
    EXPECT_EQ(q->slice()[0].length(), 2);  // we always simply override! no preconditions...
    EXPECT_EQ(typeName(q->slice()[0][0]), "object");

    Slice writes = q->slice()[0][0];
    EXPECT_EQ(typeName(writes.get("/arango/Target/ToDo/1")), "object");
    EXPECT_TRUE(typeName(writes.get("/arango/Target/ToDo/1").get("op")) ==
                "string");
    EXPECT_TRUE(writes.get("/arango/Target/ToDo/1").get("op").copyString() ==
                "delete");
    EXPECT_EQ(typeName(writes.get("/arango/Target/Finished/1")), "object");
    Slice servers =
        writes.get("/arango/Plan/Collections/database/collection1/shards/s1");
    EXPECT_EQ(typeName(servers), "array");
    EXPECT_EQ(servers.length(), 50);
    EXPECT_EQ(servers[0].copyString(), "leader");
    EXPECT_TRUE(servers[1].copyString() ==
                "follower-1-48a5e486-10c9-4953-8630-9a3de12a6169");
    EXPECT_TRUE(servers[2].copyString() ==
                "follower-2-34e18222-8ce3-4016-9558-7092e41eb22c");
    EXPECT_TRUE(servers[3].copyString() ==
                "follower-3-27452c0b-efc8-4d9a-b5b1-d557997c4337");
    EXPECT_TRUE(servers[4].copyString() ==
                "follower-4-cc56c772-58cc-4c16-b571-56283ad813c8");
    EXPECT_TRUE(servers[5].copyString() ==
                "follower-5-aec34c5c-9939-42af-bf5d-afc15f960c50");
    EXPECT_TRUE(servers[6].copyString() ==
                "follower-6-8477db61-d46f-46f7-a816-8176e1514494");
    EXPECT_TRUE(servers[7].copyString() ==
                "follower-7-58b689ae-e7e8-45e7-83cb-6006b2375f61");
    EXPECT_TRUE(servers[8].copyString() ==
                "follower-8-d5e9c550-4a68-4dca-a50d-c84d4b690945");
    EXPECT_TRUE(servers[9].copyString() ==
                "follower-9-349e7296-b4fc-4fd3-b8a8-02befbb0380e");
    EXPECT_TRUE(servers[10].copyString() ==
                "follower-10-1ad1aa18-00b3-430a-8144-ba973bfed5fe");
    EXPECT_TRUE(servers[11].copyString() ==
                "follower-11-110920e0-d079-4f06-99ed-c482d19b5112");
    EXPECT_TRUE(servers[12].copyString() ==
                "follower-12-b7d64986-c458-4332-934a-ecb2caf19259");
    EXPECT_TRUE(servers[13].copyString() ==
                "follower-13-2e7ba82c-b837-4126-8cda-0db6ac98e30b");
    EXPECT_TRUE(servers[14].copyString() ==
                "follower-14-6b9d6b95-420f-44d8-a714-d17ae95eecdd");
    EXPECT_TRUE(servers[15].copyString() ==
                "follower-15-08c7dc8d-bb31-4cc3-a7ca-4b8bfba19b70");
    EXPECT_TRUE(servers[16].copyString() ==
                "follower-16-5a301b07-d1d8-4c86-8e8a-bd7957a2cafb");
    EXPECT_TRUE(servers[17].copyString() ==
                "follower-17-fd04d0f0-821e-401e-8f8e-0b2837ddc41d");
    EXPECT_TRUE(servers[18].copyString() ==
                "follower-18-0a2bfdf4-c277-45ea-8af3-4d60eba67910");
    EXPECT_TRUE(servers[19].copyString() ==
                "follower-19-69c98e93-b1c2-416f-b5e4-84cc50b65efe");
    EXPECT_TRUE(servers[20].copyString() ==
                "follower-20-e9eca0f2-530b-4496-950e-341b71086f8b");
    EXPECT_TRUE(servers[21].copyString() ==
                "follower-21-1042a97a-aa82-48ee-8388-8480a6e57249");
    EXPECT_TRUE(servers[22].copyString() ==
                "follower-22-c3922c1e-53df-42d6-9bcd-476d01e581fd");
    EXPECT_TRUE(servers[23].copyString() ==
                "follower-23-cec0e2ed-3a5b-4b9a-a615-39a1f24179c2");
    EXPECT_TRUE(servers[24].copyString() ==
                "follower-24-1753643f-2d1e-4014-8cc6-4f063c0f143e");
    EXPECT_TRUE(servers[25].copyString() ==
                "follower-25-1a4edf05-e6ed-47bc-8765-0b8292fc3175");
    EXPECT_TRUE(servers[26].copyString() ==
                "follower-26-fcc5fb9e-b4a0-4986-ae14-8b330350fa67");
    EXPECT_TRUE(servers[27].copyString() ==
                "follower-27-aa738702-aeb8-4306-86cd-a77516eef44d");
    EXPECT_TRUE(servers[28].copyString() ==
                "follower-28-4f6cd6dc-9e12-4fcc-9083-23900ffad0d1");
    EXPECT_TRUE(servers[29].copyString() ==
                "follower-29-884e050b-0d33-440b-88bf-13cd41e00c10");
    EXPECT_TRUE(servers[30].copyString() ==
                "follower-30-bac109ba-a0ba-4235-b665-743fec5e2ea1");
    EXPECT_TRUE(servers[31].copyString() ==
                "follower-31-62a74a8e-f141-44bb-a818-57259c7d6323");
    EXPECT_TRUE(servers[32].copyString() ==
                "follower-32-7a0e8f27-04a4-4094-a00c-830dfe3e937c");
    EXPECT_TRUE(servers[33].copyString() ==
                "follower-33-83c9df58-91b1-4703-bce7-1d47c633a2c4");
    EXPECT_TRUE(servers[34].copyString() ==
                "follower-34-d8f1aa6e-fbd0-49c0-9560-b447417d0284");
    EXPECT_TRUE(servers[35].copyString() ==
                "follower-35-77b8626e-30d8-4b04-8ac9-42dd788a4c46");
    EXPECT_TRUE(servers[36].copyString() ==
                "follower-36-8239c391-86fe-462d-9036-c129983103f2");
    EXPECT_TRUE(servers[37].copyString() ==
                "follower-37-41b1fe2a-2826-43a6-8222-fc9480b4f211");
    EXPECT_TRUE(servers[38].copyString() ==
                "follower-38-4a4b54db-17ff-4f5f-882b-973907d9dc27");
    EXPECT_TRUE(servers[39].copyString() ==
                "follower-39-e6e0cb50-a609-4f5f-b376-4ec72fefb938");
    EXPECT_TRUE(servers[40].copyString() ==
                "follower-40-2c6f13c1-46dc-4d54-992f-4f923169e5e2");
    EXPECT_TRUE(servers[41].copyString() ==
                "follower-41-b4c3d57c-ec01-4162-8107-823a09176fc4");
    EXPECT_TRUE(servers[42].copyString() ==
                "follower-42-e65dfaf4-cdbd-485a-a4d3-f56848e58d28");
    EXPECT_TRUE(servers[43].copyString() ==
                "follower-43-a248deeb-817f-4f0d-9813-c08a40e9027a");
    EXPECT_TRUE(servers[44].copyString() ==
                "follower-44-c8f4e52e-7a12-4a3b-8a93-cd543f512a55");
    EXPECT_TRUE(servers[45].copyString() ==
                "follower-45-d2a70a84-2a12-4fa3-b0e9-68945fd34cfc");
    EXPECT_TRUE(servers[46].copyString() ==
                "follower-46-bf70b49c-ff50-4255-a704-70a5a4d7a4b3");
    EXPECT_TRUE(servers[47].copyString() ==
                "follower-47-ca6aaf76-0bf8-4289-9033-605883e514ca");
    EXPECT_TRUE(servers[48].copyString() ==
                "follower-48-30442bc5-2dc0-434c-b21f-989610a199e7");
    EXPECT_TRUE(servers[49].copyString() ==
                "follower-49-788d2a9b-6d56-42a7-bacb-1dafff7d58a9");
    return fakeWriteResult;
  });
  When(Method(mockAgent, waitFor)).AlwaysReturn(AgentInterface::raft_commit_t::OK);
  auto& agent = mockAgent.get();
  RemoveFollower(agency("arango"), &agent, JOB_STATUS::TODO, jobId).start(aborts);

  EXPECT_NO_THROW(Verify(Method(mockAgent, write)));
}

}  // namespace remove_follower_test
}  // namespace tests
}  // namespace arangodb
