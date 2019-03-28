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

#include "Random/RandomGenerator.h"

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
const std::string SERVER = "leader";
const std::string JOBID = "1";

bool aborts = false;

typedef std::function<std::unique_ptr<Builder>(
  Slice const&, std::string const&)>TestStructureType;

const char *agency =
#include "CleanOutServerTest.json"
;

VPackBuilder createMoveShardJob() {
  VPackBuilder builder;
  {
    VPackObjectBuilder b(&builder);
    // fake a moveshard job
    builder.add("type", VPackValue("moveShard"));
    builder.add("fromServer", VPackValue("test"));
    builder.add("toServer", VPackValue("test2"));
    builder.add("isLeader", VPackValue(true));
    builder.add("remainsFollower", VPackValue(false));
    builder.add("collection", VPackValue("test"));
    builder.add("shard", VPackValue("s99"));
    builder.add("creator", VPackValue("unittest"));
    builder.add("jobId", VPackValue(JOBID + "-0"));
    builder.add("database", VPackValue("test"));
  }
  return builder;
}

void checkFailed(JOB_STATUS status, query_t const& q) {
  INFO("WRITE: " << q->toJson());

  REQUIRE(std::string(q->slice().typeName()) == "array" );
  REQUIRE(q->slice().length() == 1);
  REQUIRE(std::string(q->slice()[0].typeName()) == "array");
  REQUIRE(q->slice()[0].length() == 1); // we always simply override! no preconditions...
  REQUIRE(std::string(q->slice()[0][0].typeName()) == "object");

  auto writes = q->slice()[0][0];
  if (status == JOB_STATUS::PENDING) {
    REQUIRE(std::string(writes.get("/arango/Supervision/DBServers/leader").get("op").typeName()) == "string");
    REQUIRE(writes.get("/arango/Supervision/DBServers/leader").get("op").copyString() == "delete");
  }
  REQUIRE(std::string(writes.get("/arango" + pos[status] + "1").typeName()) == "object");
  REQUIRE(std::string(writes.get("/arango" + pos[status] + "1").get("op").typeName()) == "string");
  CHECK(writes.get("/arango" + pos[status] + "1").get("op").copyString() == "delete");
  CHECK(std::string(writes.get("/arango/Target/Failed/1").typeName()) == "object");
}

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
  {
    VPackObjectBuilder a(&builder);
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
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  auto baseStructure = createRootNode();

  write_ret_t fakeWriteResult {true, "", std::vector<apply_ret_t> {APPLIED}, std::vector<index_t> {1}};
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
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    checkFailed(JOB_STATUS::TODO, q);
    return fakeWriteResult;
  });
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
  cleanOutServer.start(aborts);
  Verify(Method(mockAgent, write));
}

SECTION("cleanout server should wait if the server is currently blocked") {
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
        builder->add(JOBID, createJob(SERVER).slice());
      } else if (path == "/arango/Supervision/DBServers") {
        builder->add(SERVER, VPackValue("1"));
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
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
  cleanOutServer.start(aborts);
  REQUIRE(true);
}

SECTION("cleanout server should wait if the server is not healthy right now") {
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
        builder->add(JOBID, createJob(SERVER).slice());
      } else if (path == "/arango/Supervision/DBServers") {
        builder->add(SERVER, VPackValue("1"));
      }
      builder->close();
    } else {
      if (path == "/arango/Supervision/Health/" + SERVER +"/Status") {
        builder->add(VPackValue("BAD"));
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
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
  cleanOutServer.start(aborts);
  REQUIRE(true);
}

SECTION("cleanout server should fail if the server is already cleaned") {
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
        builder->add(JOBID, createJob(SERVER).slice());
      }
      builder->close();
    } else {
      if (path == "/arango/Target/CleanedServers") {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue(SERVER));
        builder->close();
      } else {
        builder->add(s);
      }
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    checkFailed(JOB_STATUS::TODO, q);
    return fakeWriteResult;
  });
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
  cleanOutServer.start(aborts);
  Verify(Method(mockAgent, write));
}

SECTION("cleanout server should fail if the server is failed") {
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
        builder->add(JOBID, createJob(SERVER).slice());
      } else if (path == "/arango/Target/FailedServers") {
        builder->add(SERVER, VPackValue("s99"));
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    checkFailed(JOB_STATUS::TODO, q);
    return fakeWriteResult;
  });
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
  cleanOutServer.start(aborts);
  Verify(Method(mockAgent, write));
}

SECTION("cleanout server should fail if the replicationFactor is too big for any shard after counting in failedservers") {
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
        builder->add(JOBID, createJob(SERVER).slice());
      } else if (path == "/arango/Target/FailedServers") {
        builder->add("follower1", VPackValue("s99"));
        builder->add("follower2", VPackValue("s99"));
        builder->add("free", VPackValue("s99"));
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    checkFailed(JOB_STATUS::TODO, q);
    return fakeWriteResult;
  });
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
  cleanOutServer.start(aborts);
  Verify(Method(mockAgent, write));
}

SECTION("cleanout server should fail if the replicationFactor is too big for any shard after counting in cleanedoutservers") {
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
        builder->add(JOBID, createJob(SERVER).slice());
      }
      builder->close();
    } else {
      if (path == "/arango/Target/CleanedServers") {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue("free"));
        builder->close();
      }
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    checkFailed(JOB_STATUS::TODO, q);
    return fakeWriteResult;
  });
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
  cleanOutServer.start(aborts);
  Verify(Method(mockAgent, write));
}

SECTION("cleanout server should fail if the replicationFactor is too big for any shard after counting in tobecleanedoutservers") {
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
        builder->add(JOBID, createJob(SERVER).slice());
      }
      builder->close();
    } else {
      if (path == "/arango/Target/ToBeCleanedServers") {
        builder->add(VPackValue(VPackValueType::Array));
        builder->add(VPackValue("free"));
        builder->close();
      }
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    checkFailed(JOB_STATUS::TODO, q);
    return fakeWriteResult;
  });
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
  cleanOutServer.start(aborts);
  Verify(Method(mockAgent, write));
}

SECTION("a cleanout server job should move into pending when everything is ok") {
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
        builder->add(JOBID, createJob(SERVER).slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WRITE: " << q->toJson());

    REQUIRE(std::string(q->slice().typeName()) == "array" );
    REQUIRE(q->slice().length() == 1);
    REQUIRE(std::string(q->slice()[0].typeName()) == "array");
    REQUIRE(q->slice()[0].length() == 2); // we have preconditions
    REQUIRE(std::string(q->slice()[0][0].typeName()) == "object");

    auto writes = q->slice()[0][0];
    REQUIRE(std::string(writes.get("/arango/Target/ToDo/1").typeName()) == "object");
    REQUIRE(std::string(writes.get("/arango/Target/ToDo/1").get("op").typeName()) == "string");
    CHECK(writes.get("/arango/Target/ToDo/1").get("op").copyString() == "delete");
    REQUIRE(std::string(writes.get("/arango/Target/Pending/1").typeName()) == "object");
    CHECK(std::string(writes.get("/arango/Target/Pending/1").get("timeStarted").typeName()) == "string");
    REQUIRE(std::string(writes.get("/arango/Supervision/DBServers/" + SERVER).typeName()) == "string");
    REQUIRE(writes.get("/arango/Supervision/DBServers/" + SERVER).copyString() == JOBID);
    REQUIRE(writes.get("/arango/Target/ToBeCleanedServers").get("op").copyString() == "push");
    REQUIRE(writes.get("/arango/Target/ToBeCleanedServers").get("new").copyString() == SERVER);
    REQUIRE(writes.get("/arango/Target/ToDo/1-0").get("toServer").copyString() == "free");

    auto preconditions = q->slice()[0][1];
    REQUIRE(preconditions.get("/arango/Supervision/DBServers/leader").get("oldEmpty").getBool() == true);
    REQUIRE(preconditions.get("/arango/Supervision/Health/leader/Status").get("old").copyString() == "GOOD");
    REQUIRE(preconditions.get("/arango/Target/CleanedServers").get("old").toJson() == "[]");
    REQUIRE(preconditions.get("/arango/Target/FailedServers").get("old").toJson() == "{}");
    return fakeWriteResult;
  });
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
  cleanOutServer.start(aborts);
  Verify(Method(mockAgent, write));
}

SECTION("a cleanout server job should abort after a long timeout") {
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

      if (path == "/arango/Target/Pending") {
        auto job = createJob(SERVER);
        builder->add(VPackValue(JOBID));
        builder->add(VPackValue(VPackValueType::Object));
        for (auto const& jobIt: VPackObjectIterator(job.slice())) {
          if (jobIt.key.copyString() == "timeCreated") {
            builder->add(jobIt.key.copyString(), VPackValue("2015-01-03T20:00:00Z"));
          } else {
            builder->add(jobIt.key.copyString(), jobIt.value);
          }
        }
        builder->close();
      } else if (path == "/arango/Target/ToDo") {
        VPackBuilder moveBuilder = createMoveShardJob();
        builder->add("1-0", moveBuilder.slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };

  int qCount = 0;
  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    if (qCount++ == 0) {
      // first the moveShard job should be aborted
      INFO("WRITE FIRST: " << q->toJson());

      REQUIRE(std::string(q->slice().typeName()) == "array" );
      REQUIRE(q->slice().length() == 1);
      REQUIRE(std::string(q->slice()[0].typeName()) == "array");
      REQUIRE(q->slice()[0].length() == 2); // precondition that still in ToDo
      REQUIRE(std::string(q->slice()[0][0].typeName()) == "object");

      auto writes = q->slice()[0][0];
      REQUIRE(std::string(writes.get("/arango/Target/ToDo/1-0").typeName()) == "object");
      REQUIRE(std::string(writes.get("/arango/Target/ToDo/1-0").get("op").typeName()) == "string");
      CHECK(writes.get("/arango/Target/ToDo/1-0").get("op").copyString() == "delete");
      // a not yet started job will be moved to finished
      CHECK(std::string(writes.get("/arango/Target/Finished/1-0").typeName()) == "object");
      auto preconds = q->slice()[0][1];
      CHECK(preconds.get("/arango/Target/ToDo/1-0").get("oldEmpty").isFalse());
    } else {
      // finally cleanout should be failed
      checkFailed(JOB_STATUS::PENDING, q);
    }
    return fakeWriteResult;
  });
  AgentInterface &agent = mockAgent.get();

  Node agency = createAgency(createTestStructure);
  INFO("AGENCY: " << agency.toJson());
  // should not throw
  auto cleanOutServer = CleanOutServer(
    agency,
    &agent,
    JOB_STATUS::PENDING,
    JOBID
  );
  cleanOutServer.run(aborts);
  Verify(Method(mockAgent, write));
}

SECTION("when there are still subjobs to be done it should wait") {
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

      if (path == "/arango/Target/Pending") {
        builder->add(JOBID, createJob(SERVER).slice());
      } else if (path == "/arango/Target/ToDo") {
        VPackBuilder moveBuilder = createMoveShardJob();
        builder->add("1-0", moveBuilder.slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };
  Mock<AgentInterface> mockAgent;
  AgentInterface &agent = mockAgent.get();
  Node agency = createAgency(createTestStructure);
  INFO("AGENCY: " << agency.toJson());
  // should not throw
  auto cleanOutServer = CleanOutServer(
    agency,
    &agent,
    JOB_STATUS::PENDING,
    JOBID
  );
  cleanOutServer.run(aborts);
  REQUIRE(true);
};

SECTION("once all subjobs were successful then the job should be finished") {
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

      if (path == "/arango/Target/Pending") {
        builder->add(JOBID, createJob(SERVER).slice());
      } else if (path == "/arango/Target/Finished") {
        VPackBuilder moveBuilder = createMoveShardJob();
        builder->add("1-0", moveBuilder.slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };
  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    INFO("WRITE: " << q->toJson());

    REQUIRE(std::string(q->slice().typeName()) == "array" );
    REQUIRE(q->slice().length() == 1);
    REQUIRE(std::string(q->slice()[0].typeName()) == "array");
    REQUIRE(q->slice()[0].length() == 1); // we always simply override! no preconditions...
    REQUIRE(std::string(q->slice()[0][0].typeName()) == "object");

    auto writes = q->slice()[0][0];
    REQUIRE(std::string(writes.get("/arango/Supervision/DBServers/leader").get("op").typeName()) == "string");
    REQUIRE(writes.get("/arango/Supervision/DBServers/leader").get("op").copyString() == "delete");
    REQUIRE(std::string(writes.get("/arango/Target/Pending/1").typeName()) == "object");
    REQUIRE(std::string(writes.get("/arango/Target/Pending/1").get("op").typeName()) == "string");
    CHECK(writes.get("/arango/Target/Pending/1").get("op").copyString() == "delete");
    CHECK(std::string(writes.get("/arango/Target/Finished/1").typeName()) == "object");
    return fakeWriteResult;
  });
  AgentInterface &agent = mockAgent.get();
  Node agency = createAgency(createTestStructure);
  INFO("AGENCY: " << agency.toJson());
  // should not throw
  auto cleanOutServer = CleanOutServer(
    agency,
    &agent,
    JOB_STATUS::PENDING,
    JOBID
  );
  cleanOutServer.run(aborts);
  REQUIRE(true);
}

SECTION("if there was a failed subjob then the job should also fail") {
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

      if (path == "/arango/Target/Pending") {
        builder->add(JOBID, createJob(SERVER).slice());
      } else if (path == "/arango/Target/Failed") {
        VPackBuilder moveBuilder = createMoveShardJob();
        builder->add("1-0", moveBuilder.slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };
  Mock<AgentInterface> mockAgent;
  When(Method(mockAgent, write)).Do([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    checkFailed(JOB_STATUS::PENDING, q);
    return fakeWriteResult;
  });
  AgentInterface &agent = mockAgent.get();
  Node agency = createAgency(createTestStructure);
  INFO("AGENCY: " << agency.toJson());
  // should not throw
  auto cleanOutServer = CleanOutServer(
    agency,
    &agent,
    JOB_STATUS::PENDING,
    JOBID
  );
  cleanOutServer.run(aborts);
  REQUIRE(true);
}

SECTION("when the cleanout server job is aborted all subjobs should be aborted too") {
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

      if (path == "/arango/Target/Pending") {
        builder->add(JOBID, createJob(SERVER).slice());
      } else if (path == "/arango/Target/ToDo") {
        VPackBuilder moveBuilder = createMoveShardJob();
        builder->add("1-0", moveBuilder.slice());
      }
      builder->close();
    } else {
      builder->add(s);
    }
    return builder;
  };
  Mock<AgentInterface> mockAgent;
  int qCount = 0;
  When(Method(mockAgent, write)).AlwaysDo([&](query_t const& q, consensus::AgentInterface::WriteMode w) -> write_ret_t {
    if (qCount++ == 0) {
      // first the moveShard job should be aborted
      INFO("WRITE FIRST: " << q->toJson());

      REQUIRE(std::string(q->slice().typeName()) == "array" );
      REQUIRE(q->slice().length() == 1);
      REQUIRE(std::string(q->slice()[0].typeName()) == "array");
      REQUIRE(q->slice()[0].length() == 2); // precondition that still in ToDo
      REQUIRE(std::string(q->slice()[0][0].typeName()) == "object");

      auto writes = q->slice()[0][0];
      REQUIRE(std::string(writes.get("/arango/Target/ToDo/1-0").typeName()) == "object");
      REQUIRE(std::string(writes.get("/arango/Target/ToDo/1-0").get("op").typeName()) == "string");
      CHECK(writes.get("/arango/Target/ToDo/1-0").get("op").copyString() == "delete");
      // a not yet started job will be moved to finished
      CHECK(std::string(writes.get("/arango/Target/Finished/1-0").typeName()) == "object");
      auto preconds = q->slice()[0][1];
      CHECK(preconds.get("/arango/Target/ToDo/1-0").get("oldEmpty").isFalse());
    } else {
      checkFailed(JOB_STATUS::PENDING, q);
    }
    return fakeWriteResult;
  });
  AgentInterface &agent = mockAgent.get();
  Node agency = createAgency(createTestStructure);
  INFO("AGENCY: " << agency.toJson());
  // should not throw
  auto cleanOutServer = CleanOutServer(
    agency,
    &agent,
    JOB_STATUS::PENDING,
    JOBID
  );
  cleanOutServer.abort();
  REQUIRE(true);
}

}
}
}
}
