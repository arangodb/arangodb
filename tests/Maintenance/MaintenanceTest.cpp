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
/// @author Matthew Von-Maszewski
/// @author Copyright 2017-2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Agency/AgencyPaths.h"
#include "Agency/AgencyStrings.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Cluster/Maintenance.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/ResignShardLeadership.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <typeinfo>

#include "MaintenanceFeatureMock.h"

using namespace arangodb;
using namespace arangodb::consensus;
using namespace arangodb::maintenance;

#ifndef _WIN32
char const* planStr =
#include "Plan.json"
    ;
char const* currentStr =
#include "Current.json"
    ;
char const* supervisionStr =
#include "Supervision.json"
    ;
char const* dbs0Str =
#include "DBServer0001.json"
    ;
char const* dbs1Str =
#include "DBServer0002.json"
    ;
char const* dbs2Str =
#include "DBServer0003.json"
    ;
#else  // _WIN32

#include <Windows.h>
#include "jsonresource.h"
LPSTR planStr = nullptr;
LPSTR currentStr = nullptr;
LPSTR supervisionStr = nullptr;
LPSTR dbs0Str = nullptr;
LPSTR dbs1Str = nullptr;
LPSTR dbs2Str = nullptr;

#endif  // _WIN32

// Random stuff
std::random_device rd{};
std::mt19937 g(rd());

std::vector<std::string> const shortNames{"DBServer0001", "DBServer0002",
                                          "DBServer0003"};

std::string const PLAN_COL_PATH = "/Collections/";
std::string const PLAN_DB_PATH = "/Databases/";

size_t localId = 1016002;

std::string S("s");
std::string C("c");



namespace arangodb {
class LogicalCollection;
}
using namespace arangodb::maintenance;

class SharedMaintenanceTest : public ::testing::Test {
 protected:
  SharedMaintenanceTest() : server(nullptr, nullptr), engine(server) {
    loadResources();
    plan = createNode(planStr);
    originalPlan = plan;
    supervision = createNode(supervisionStr);
    current = createNode(currentStr);
    dbsIds = matchShortLongIds(supervision);
    arangodb::AgencyCommHelper::initialize("arango");
  }

 protected:
  // Relevant agency
  Node plan{""};
  Node originalPlan{""};
  Node supervision{""};
  Node current{""};
  application_features::ApplicationServer server;
  StorageEngineMock engine;

  // map <shortId, UUID>
  std::map<std::string, std::string> dbsIds;

  /**
   * @brief Helper methods
   *
   */
 protected:
#ifndef _WIN32
  int loadResources(void) { return 0; }

#else  // _WIN32
  LPSTR getResource(int which) {
    HRSRC myResource = ::FindResource(NULL, MAKEINTRESOURCE(which), RT_RCDATA);
    HGLOBAL myResourceData = ::LoadResource(NULL, myResource);
    return (LPSTR)::LockResource(myResourceData);
  }
  int loadResources(void) {
    if ((planStr == nullptr) && (currentStr == nullptr) && (supervisionStr == nullptr) &&
        (dbs0Str == nullptr) && (dbs1Str == nullptr) && (dbs2Str == nullptr)) {
      planStr = getResource(IDS_PLAN);
      currentStr = getResource(IDS_CURRENT);
      dbs0Str = getResource(IDS_DBSERVER0001);
      dbs1Str = getResource(IDS_DBSERVER0002);
      dbs2Str = getResource(IDS_DBSERVER0003);
      supervisionStr = getResource(IDS_SUPERVISION);
    }
    return 0;
  }

#endif  // _WIN32

  std::map<std::string, std::string> matchShortLongIds(Node const& supervision) {
    std::map<std::string, std::string> ret;
    for (auto const& dbs : supervision("Health").children()) {
      if (dbs.first.front() == 'P') {
        ret.emplace((*dbs.second)("ShortName").getString(), dbs.first);
      }
    }
    return ret;
  }

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
    VPackOptions options;
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

  VPackBuilder createDatabase(std::string const& dbname) {
    Builder builder;
    {
      VPackObjectBuilder o(&builder);
      builder.add("id", VPackValue(std::to_string(localId++)));
      builder.add("coordinator",
                  VPackValue("CRDN-42df19c3-73d5-48f4-b02e-09b29008eff8"));
      builder.add(VPackValue("options"));
      { VPackObjectBuilder oo(&builder); }
      builder.add("name", VPackValue(dbname));
    }
    return builder;
  }

  void createPlanDatabase(std::string const& dbname, Node& plan) {
    plan(PLAN_DB_PATH + dbname) = createDatabase(dbname).slice();
  }

  VPackBuilder createIndex(std::string const& type, std::vector<std::string> const& fields,
                           bool unique, bool sparse, bool deduplicate) {
    VPackBuilder index;
    {
      VPackObjectBuilder o(&index);
      {
        index.add("deduplicate", VPackValue(deduplicate));
        index.add(VPackValue("fields"));
        {
          VPackArrayBuilder a(&index);
          for (auto const& field : fields) {
            index.add(VPackValue(field));
          }
        }
        index.add("id", VPackValue(std::to_string(localId++)));
        index.add("sparse", VPackValue(sparse));
        index.add("type", VPackValue(type));
        index.add("unique", VPackValue(unique));
      }
    }

    return index;
  }

  void createPlanIndex(std::string const& dbname, std::string const& colname,
                       std::string const& type, std::vector<std::string> const& fields,
                       bool unique, bool sparse, bool deduplicate, Node& plan) {
    VPackBuilder val;
    {
      VPackObjectBuilder o(&val);
      val.add("new", createIndex(type, fields, unique, sparse, deduplicate).slice());
    }
    plan(PLAN_COL_PATH + dbname + "/" + colname + "/indexes").handle<PUSH>(val.slice());
  }

  void createCollection(std::string const& colname, VPackBuilder& col) {
    VPackBuilder keyOptions;
    {
      VPackObjectBuilder o(&keyOptions);
      keyOptions.add("lastValue", VPackValue(0));
      keyOptions.add("type", VPackValue("traditional"));
      keyOptions.add("allowUserKeys", VPackValue(true));
    }

    VPackBuilder shardKeys;
    {
      VPackArrayBuilder a(&shardKeys);
      shardKeys.add(VPackValue("_key"));
    }

    VPackBuilder indexes;
    {
      VPackArrayBuilder a(&indexes);
      indexes.add(createIndex("primary", {"_key"}, true, false, false).slice());
    }

    col.add("id", VPackValue(std::to_string(localId++)));
    col.add("status", VPackValue(3));
    col.add("keyOptions", keyOptions.slice());
    col.add("cacheEnabled", VPackValue(false));
    col.add("waitForSync", VPackValue(false));
    col.add("type", VPackValue(2));
    col.add("isSystem", VPackValue(true));
    col.add("name", VPackValue(colname));
    col.add("shardingStrategy", VPackValue("hash"));
    col.add("statusString", VPackValue("loaded"));
    col.add("shardKeys", shardKeys.slice());
  }


  void createPlanShards(size_t numberOfShards, size_t replicationFactor, VPackBuilder& col) {
    auto servers = shortNames;
    std::shuffle(servers.begin(), servers.end(), g);

    col.add("numberOfShards", VPackValue(1));
    col.add("replicationFactor", VPackValue(2));
    col.add(VPackValue("shards"));
    {
      VPackObjectBuilder s(&col);
      for (size_t i = 0; i < numberOfShards; ++i) {
        col.add(VPackValue(S + std::to_string(localId++)));
        {
          VPackArrayBuilder a(&col);
          size_t j = 0;
          for (auto const& server : servers) {
            if (j++ < replicationFactor) {
              col.add(VPackValue(dbsIds[server]));
            }
          }
        }
      }
    }
  }

  void createPlanCollection(std::string const& dbname, std::string const& colname,
                            size_t numberOfShards, size_t replicationFactor, Node& plan) {
    VPackBuilder tmp;
    {
      VPackObjectBuilder o(&tmp);
      createCollection(colname, tmp);
      tmp.add("isSmart", VPackValue(false));
      tmp.add("deleted", VPackValue(false));
      createPlanShards(numberOfShards, replicationFactor, tmp);
    }

    Slice col = tmp.slice();
    auto id = col.get("id").copyString();
    plan(PLAN_COL_PATH + dbname + "/" + col.get("id").copyString()).applies(col);
  }

  void createLocalCollection(std::string const& dbname,
                             std::string const& colname, Node& node) {
    size_t planId = std::stoull(colname);
    VPackBuilder tmp;
    {
      VPackObjectBuilder o(&tmp);
      createCollection(colname, tmp);
      tmp.add("planId", VPackValue(colname));
      tmp.add("theLeader", VPackValue(""));
      tmp.add("globallyUniqueId",
              VPackValue(C + colname + "/" + S + std::to_string(planId + 1)));
      tmp.add("objectId", VPackValue("9031415"));
    }
    node(dbname + "/" + S + std::to_string(planId + 1)).applies(tmp.slice());
  }

  std::map<std::string, std::string> collectionMap(Node const& plan) {
    std::map<std::string, std::string> ret;
    auto const pb = plan("Collections").toBuilder();
    auto const ps = pb.slice();
    for (auto const& db : VPackObjectIterator(ps)) {
      for (auto const& col : VPackObjectIterator(db.value)) {
        ret.emplace(db.key.copyString() + "/" + col.value.get("name").copyString(),
                    col.key.copyString());
      }
    }
    return ret;
  }
};

class MaintenanceTestActionDescription : public SharedMaintenanceTest {

 protected:
  MaintenanceTestActionDescription() : SharedMaintenanceTest() {
  }
};

TEST_F(MaintenanceTestActionDescription, construct_minimal_actiondescription) {
  ActionDescription desc(std::map<std::string, std::string>{{"name",
                                                             "SomeAction"}},
                         NORMAL_PRIORITY, false);
  ASSERT_EQ(desc.get("name"), "SomeAction");
}

TEST_F(MaintenanceTestActionDescription, construct_minimal_actiondescription_with_nullptr_props) {
  std::shared_ptr<VPackBuilder> props;
  ActionDescription desc({{"name", "SomeAction"}}, NORMAL_PRIORITY, false, props);
}

TEST_F(MaintenanceTestActionDescription, construct_minimal_actiondescription_with_empty_props) {
  std::shared_ptr<VPackBuilder> props;
  ActionDescription desc({{"name", "SomeAction"}}, NORMAL_PRIORITY, false, props);
  ASSERT_EQ(desc.get("name"), "SomeAction");
}

TEST_F(MaintenanceTestActionDescription, retrieve_nonassigned_key_from_actiondescription) {
  std::shared_ptr<VPackBuilder> props;
  ActionDescription desc({{"name", "SomeAction"}}, NORMAL_PRIORITY, false, props);
  ASSERT_EQ(desc.get("name"), "SomeAction");
  try {
    auto bogus = desc.get("bogus");
    ASSERT_EQ(bogus, "bogus");
  } catch (std::out_of_range const&) {
  }
  std::string value;
  auto res = desc.get("bogus", value);
  ASSERT_TRUE(value.empty());
  ASSERT_FALSE(res.ok());
}

TEST_F(MaintenanceTestActionDescription, retrieve_nonassigned_key_from_actiondescription_2) {
  std::shared_ptr<VPackBuilder> props;
  ActionDescription desc({{"name", "SomeAction"}, {"bogus", "bogus"}}, NORMAL_PRIORITY, false, props);
  ASSERT_EQ(desc.get("name"), "SomeAction");
  try {
    auto bogus = desc.get("bogus");
    ASSERT_EQ(bogus, "bogus");
  } catch (std::out_of_range const&) {
  }
  std::string value;
  auto res = desc.get("bogus", value);
  ASSERT_EQ(value, "bogus");
  ASSERT_TRUE(res.ok());
}

TEST_F(MaintenanceTestActionDescription, retrieve_nonassigned_properties_from_actiondescription) {
  std::shared_ptr<VPackBuilder> props;
  ActionDescription desc({{"name", "SomeAction"}}, NORMAL_PRIORITY, false, props);
  ASSERT_EQ(desc.get("name"), "SomeAction");
  ASSERT_EQ(desc.properties(), nullptr);
}

TEST_F(MaintenanceTestActionDescription, retrieve_empty_properties_from_actiondescription) {
  auto props = std::make_shared<VPackBuilder>();
  ActionDescription desc({{"name", "SomeAction"}}, NORMAL_PRIORITY, false, props);
  ASSERT_EQ(desc.get("name"), "SomeAction");
  ASSERT_TRUE(desc.properties()->isEmpty());
}

TEST_F(MaintenanceTestActionDescription, retrieve_empty_object_properties_from_actiondescription) {
  auto props = std::make_shared<VPackBuilder>();
  { VPackObjectBuilder empty(props.get()); }
  ActionDescription desc({{"name", "SomeAction"}}, NORMAL_PRIORITY, false, props);
  ASSERT_EQ(desc.get("name"), "SomeAction");
  ASSERT_TRUE(desc.properties()->slice().isEmptyObject());
}

TEST_F(MaintenanceTestActionDescription, retrieve_string_value_from_actiondescriptions_properties) {
  auto props = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder obj(props.get());
    props->add("hello", VPackValue("world"));
  }
  ActionDescription desc({{"name", "SomeAction"}}, NORMAL_PRIORITY, false, props);
  ASSERT_EQ(desc.get("name"), "SomeAction");
  ASSERT_TRUE(desc.properties()->slice().hasKey("hello"));
  ASSERT_EQ(desc.properties()->slice().get("hello").copyString(), "world");
}

TEST_F(MaintenanceTestActionDescription, retrieve_double_value_from_actiondescriptions_properties) {
  double pi = 3.14159265359;
  auto props = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder obj(props.get());
    props->add("pi", VPackValue(3.14159265359));
  }
  ActionDescription desc({{"name", "SomeAction"}}, NORMAL_PRIORITY, false, props);
  ASSERT_EQ(desc.get("name"), "SomeAction");
  ASSERT_TRUE(desc.properties()->slice().hasKey("pi"));
  ASSERT_EQ(desc.properties()->slice().get("pi").getNumber<double>(), pi);
}

TEST_F(MaintenanceTestActionDescription, retrieve_integer_value_from_actiondescriptions_property) {
  size_t one = 1;
  auto props = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder obj(props.get());
    props->add("one", VPackValue(one));
  }
  ActionDescription desc({{"name", "SomeAction"}}, NORMAL_PRIORITY, false, props);
  ASSERT_EQ(desc.get("name"), "SomeAction");
  ASSERT_TRUE(desc.properties()->slice().hasKey("one"));
  ASSERT_EQ(desc.properties()->slice().get("one").getNumber<size_t>(), one);
}

TEST_F(MaintenanceTestActionDescription, retrieve_array_value_from_actiondescriptions_properties) {
  double pi = 3.14159265359;
  size_t one = 1;
  std::string hello("hello world!");
  auto props = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder obj(props.get());
    props->add(VPackValue("array"));
    {
      VPackArrayBuilder arr(props.get());
      props->add(VPackValue(pi));
      props->add(VPackValue(one));
      props->add(VPackValue(hello));
    }
  }
  ActionDescription desc({{"name", "SomeAction"}}, NORMAL_PRIORITY, false, props);
  ASSERT_EQ(desc.get("name"), "SomeAction");
  ASSERT_TRUE(desc.properties()->slice().hasKey("array"));
  ASSERT_TRUE(desc.properties()->slice().get("array").isArray());
  ASSERT_EQ(desc.properties()->slice().get("array").length(), 3);
  ASSERT_EQ(desc.properties()->slice().get("array")[0].getNumber<double>(), pi);
  ASSERT_EQ(desc.properties()->slice().get("array")[1].getNumber<size_t>(), one);
  ASSERT_EQ(desc.properties()->slice().get("array")[2].copyString(), hello);
}

class MaintenanceTestActionPhaseOne : public SharedMaintenanceTest {
 protected:
  int _dummy;
  std::shared_ptr<arangodb::options::ProgramOptions> po;
  arangodb::application_features::ApplicationServer as;
  std::unordered_set<DatabaseID> makeDirty;
  MaintenanceFeature::errors_t errors;

  std::map<std::string, Node> localNodes;

  arangodb::RocksDBEngine engine;  // arbitrary implementation that has index types registered
  arangodb::StorageEngine* origStorageEngine;

  MaintenanceTestActionPhaseOne()
      : SharedMaintenanceTest(),
        _dummy(loadResources()),
        po(std::make_shared<arangodb::options::ProgramOptions>("test", std::string(),
                                                               std::string(),
                                                               "path")),
        as(po, nullptr),
        localNodes{{dbsIds[shortNames[0]], createNode(dbs0Str)},
                   {dbsIds[shortNames[1]], createNode(dbs1Str)},
                   {dbsIds[shortNames[2]], createNode(dbs2Str)}},
        engine(as) {
    as.addFeature<arangodb::MetricsFeature>();
    as.addFeature<arangodb::application_features::GreetingsFeaturePhase>(false);
    auto& selector = as.addFeature<arangodb::EngineSelectorFeature>();

    selector.setEngineTesting(&engine);
  }

  ~MaintenanceTestActionPhaseOne() {
    as.getFeature<arangodb::EngineSelectorFeature>().setEngineTesting(nullptr);
  }

  auto dbName() const -> std::string {
    // this is a database known in the test files
    return "foo";
  }

  auto planId() const -> std::string {
    // This is a gobal collection known in the test files.
    // It is required to have 6 shards, 2 per DBServer
    return "2010088";
  }

  auto unusedServer() const -> std::string {
    return "PRMR-deadbeef-1337-7331-abcd-123456789abc";
  }

  enum class PLAN_LEADERSHIP_TYPE { SELF, RESIGNED_SELF, OTHER, RESIGNED_OTHER };

  enum class LOCAL_LEADERSHIP_TYPE { SELF, OTHER, RESIGNED, REBOOTED };

  auto getShardsForServer(std::string const& dbName, std::string const& planId,
                          std::string const& serverId, Node const& plan)
      -> std::unordered_set<std::string> {
    auto path = arangodb::cluster::paths::aliases::plan()
                    ->collections()
                    ->database(dbName)
                    ->collection(planId)
                    ->shards();

    auto vec = path->vec(2);
    TRI_ASSERT(plan.has(vec));
    auto const& shardList = plan(vec);
    std::unordered_set<std::string> res;
    for (auto const& [shard, servers] : shardList.children()) {
      auto oldValue = servers->slice();
      TRI_ASSERT(oldValue.isArray());
      TRI_ASSERT(oldValue.length() == 2);
      if (oldValue.at(0).isEqualString(serverId)) {
        res.emplace(shard);
      }
    }
    return res;
  }

  auto setLeadershipPlan(std::string const& dbName, std::string const& planId,
                         PLAN_LEADERSHIP_TYPE type, Node& plan) -> void {
    auto path = arangodb::cluster::paths::aliases::plan()
                    ->collections()
                    ->database(dbName)
                    ->collection(planId)
                    ->shards();

    auto vec = path->vec(2);
    ASSERT_TRUE(plan.has(vec)) << "The underlying test plan is modified, it "
                                  "does not contain Database '"
                               << dbName << "' and Collection '" << planId << "' anymore.";
    auto& shardList = plan(vec);
    for (auto& [shard, servers] : shardList.children()) {
      auto oldValue = servers->slice();
      ASSERT_TRUE(oldValue.isArray());
      ASSERT_EQ(oldValue.length(), 2)
          << "We assume to have one leader and one follower";
      switch (type) {
        case PLAN_LEADERSHIP_TYPE::SELF: {
          // Nothing to do here, we are leader
          break;
        }
        case PLAN_LEADERSHIP_TYPE::RESIGNED_SELF: {
          // We simulate a resign leader, this is indicated by appending an `_` in front of the server name.
          VPackBuilder newValue;
          {
            VPackArrayBuilder guard(&newValue);
            newValue.add(VPackValue("_" + oldValue.at(0).copyString()));
            newValue.add(VPackValue(oldValue.at(1).copyString()));
          }
          *servers = newValue.slice();
          break;
        }
        case PLAN_LEADERSHIP_TYPE::OTHER: {
          // We simulate we get told another leader is there.
          VPackBuilder newValue;
          {
            VPackArrayBuilder guard(&newValue);
            newValue.add(VPackValue(unusedServer()));
            newValue.add(VPackValue(oldValue.at(0).copyString()));
            newValue.add(VPackValue(oldValue.at(1).copyString()));
          }
          *servers = newValue.slice();
          break;
        }
        case PLAN_LEADERSHIP_TYPE::RESIGNED_OTHER: {
          // We simulate we get told another leader is there, and resigned, this is indicated by appending an `_` in front of the server name.
          VPackBuilder newValue;
          {
            VPackArrayBuilder guard(&newValue);
            newValue.add(VPackValue("_" + unusedServer()));
            newValue.add(VPackValue(oldValue.at(0).copyString()));
            newValue.add(VPackValue(oldValue.at(1).copyString()));
          }
          *servers = newValue.slice();
          break;
        }
      }
    }
  }

  // Will resign leadership on the given plan.
  // The plan will be modified in-place
  // Asserts that dbName and planId exists in plan
  auto resignLeadershipPlan(std::string const& dbName, std::string const& planId, Node& plan) -> void {
    return setLeadershipPlan(dbName, planId, PLAN_LEADERSHIP_TYPE::RESIGNED_SELF, plan);
  }

  // Will take leadership in plan
  // Asserts that dbName and planId exists in plan
  // NOTE: The plan already contians leadersip of SELF so this is a noop besides assertions.
  auto takeLeadershipPlan(std::string const& dbName, std::string const& planId, Node& plan) -> void {
    return setLeadershipPlan(dbName, planId, PLAN_LEADERSHIP_TYPE::SELF, plan);
  }

  // Another server will take leadership in plan
  // Asserts that dbName and planId exists in plan
  auto otherTakeLeadershipPlan(std::string const& dbName, std::string const& planId, Node& plan) -> void {
    return setLeadershipPlan(dbName, planId, PLAN_LEADERSHIP_TYPE::OTHER, plan);
  }

  // Another server will take resigned leadership in plan
  // Asserts that dbName and planId exists in plan
  auto otherTakeResignedLeadershipPlan(std::string const& dbName,
                               std::string const& planId, Node& plan) -> void {
    return setLeadershipPlan(dbName, planId, PLAN_LEADERSHIP_TYPE::RESIGNED_OTHER, plan);
  }

  auto setLeadershipLocal(std::string const& dbName,
                            std::unordered_set<std::string> const& shardNames,
                            LOCAL_LEADERSHIP_TYPE type, Node& local) {
    for (auto const& shardName : shardNames) {
      auto vec = {dbName, shardName, std::string(THE_LEADER)};
      ASSERT_TRUE(local.has(vec))
          << "The underlying test plan is modified, it "
             "does not contain Database '"
          << dbName << "' and Shard '" << shardName << "' anymore.";
      auto& leaderInfo = local(vec);
      VPackBuilder newValue;
      switch (type) {
        case LOCAL_LEADERSHIP_TYPE::SELF: {
          newValue.add(VPackValue(""));
          break;
        }
        case LOCAL_LEADERSHIP_TYPE::OTHER: {
          newValue.add(VPackValue(unusedServer()));
          break;
        }
        case LOCAL_LEADERSHIP_TYPE::RESIGNED: {
          newValue.add(VPackValue(ResignShardLeadership::LeaderNotYetKnownString));
          break;
        }
        case LOCAL_LEADERSHIP_TYPE::REBOOTED: {
          newValue.add(VPackValue(LEADER_NOT_YET_KNOWN));
          break;
        }
      }
      leaderInfo = newValue.slice();
    }
  }

  // Claim leadership of the given shards ourself.
  auto takeLeadershipLocal(std::string const& dbName,
                             std::unordered_set<std::string> const& shardNames,
                             Node& local) {
    setLeadershipLocal(dbName, shardNames, LOCAL_LEADERSHIP_TYPE::SELF, local);
  }

    // Resign leadership of the given shards ourself.
  auto resignLeadershipLocal(std::string const& dbName,
                             std::unordered_set<std::string> const& shardNames,
                             Node& local) {
    setLeadershipLocal(dbName, shardNames, LOCAL_LEADERSHIP_TYPE::RESIGNED, local);
  }

  // Let other server claim leadership of the given shards ourself.
  auto otherTakeLeadershipLocal(std::string const& dbName,
                                  std::unordered_set<std::string> const& shardNames,
                                  Node& local) {
    setLeadershipLocal(dbName, shardNames, LOCAL_LEADERSHIP_TYPE::OTHER, local);
  }

  // Claim leadership of the given shards ourself.
  auto rebootLeadershipLocal(std::string const& dbName,
                               std::unordered_set<std::string> const& shardNames,
                               Node& local) {
    setLeadershipLocal(dbName, shardNames, LOCAL_LEADERSHIP_TYPE::REBOOTED, local);
  }

  auto assertIsTakeoverLeadershipAction(ActionDescription const& action,
                                        std::string const& dbName,
                                        std::string const& planId) -> void {
    ASSERT_EQ(action.name(), "TakeoverShardLeadership");
    ASSERT_TRUE(action.has(DATABASE));
    ASSERT_TRUE(action.has(COLLECTION));
    ASSERT_TRUE(action.has(SHARD));
    ASSERT_TRUE(action.has(THE_LEADER));
    ASSERT_TRUE(action.has(LOCAL_LEADER));
    ASSERT_TRUE(action.has(PLAN_RAFT_INDEX));
    ASSERT_EQ(action.get(DATABASE), dbName);
    ASSERT_EQ(action.get(COLLECTION), planId);
  }

  auto assertIsResignLeadershipAction(ActionDescription const& action,
                                      std::string const& dbName) -> void {
    ASSERT_EQ(action.name(), "ResignShardLeadership");
    ASSERT_TRUE(action.has(DATABASE));
    ASSERT_TRUE(action.has(SHARD));
    ASSERT_EQ(action.get(DATABASE), dbName);
  }
};


std::vector<std::string> PLAN_SECTIONS {ANALYZERS, COLLECTIONS, DATABASES, VIEWS};
std::unordered_map<std::string,std::shared_ptr<VPackBuilder>> planToChangeset(Node const& plan) {
  std::unordered_map<std::string,std::shared_ptr<VPackBuilder>> ret;
  for (auto const& db : plan(DATABASES).children()) {
    VPackBuilder& dbbuilder =
      *ret.try_emplace(db.first, std::make_shared<VPackBuilder>()).first->second;

    { VPackArrayBuilder env(&dbbuilder);
      { VPackObjectBuilder o(&dbbuilder);
        dbbuilder.add(VPackValue(AgencyCommHelper::path()));
        { VPackObjectBuilder a(&dbbuilder);
          dbbuilder.add(VPackValue(PLAN));
          { VPackObjectBuilder p(&dbbuilder);
            for (auto const& section : PLAN_SECTIONS) {
              dbbuilder.add(VPackValue(section));
              VPackObjectBuilder c(&dbbuilder);
              auto path = std::vector<std::string>{section, db.first};
              if (plan.has(path)) {
                dbbuilder.add(db.first, plan(path).toBuilder().slice());
              }
            }}}}}
  }
  return ret;
}

std::unordered_map<std::string,std::shared_ptr<VPackBuilder>> localToChangeset(Node const& local) {
  std::unordered_map<std::string,std::shared_ptr<VPackBuilder>> ret;
  for (auto const& db : local.children()) {
    ret.try_emplace(db.first, std::make_shared<VPackBuilder>(db.second->toBuilder()));
  }
  return ret;
}

TEST_F(MaintenanceTestActionPhaseOne, in_sync_should_have_0_effects) {
  std::vector<std::shared_ptr<ActionDescription>> actions;

  auto pcs = planToChangeset(plan);

  std::unordered_set<std::string> dirty {};
  bool callNotify = false;
  for (auto const& node : localNodes) {
    arangodb::maintenance::diffPlanLocal(engine, pcs, 0, dirty,
                                         localToChangeset(node.second), node.first,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});
    ASSERT_EQ(actions.size(), 0);
  }

  dirty.emplace("_system"); // should still have no effect (equilibrium)
  for (auto const& node : localNodes) {
    arangodb::maintenance::diffPlanLocal(engine, pcs, 0, dirty,
                                         localToChangeset(node.second), node.first,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});
    ASSERT_EQ(actions.size(), 0);
  }

  dirty.emplace("foo"); // should still have no effect (equilibrium)
  for (auto const& node : localNodes) {
    arangodb::maintenance::diffPlanLocal(engine, pcs, 0, dirty,
                                         localToChangeset(node.second), node.first,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});
    ASSERT_EQ(actions.size(), 0);
  }

}

TEST_F(MaintenanceTestActionPhaseOne, local_databases_one_more_empty_database_should_be_dropped) {
  std::vector<std::shared_ptr<ActionDescription>> actions;

  localNodes.begin()->second("db3") = arangodb::velocypack::Slice::emptyObjectSlice();
  std::unordered_set<std::string> dirty{};
  bool callNotify = false;

  arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0, dirty,
                                       localToChangeset(localNodes.begin()->second),
                                       localNodes.begin()->first, errors,
                                       makeDirty, callNotify, actions,
                                       arangodb::MaintenanceFeature::ShardActionMap{});

  ASSERT_EQ(actions.size(), 0);

  dirty.emplace("db3");
  arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0, dirty,
                                       localToChangeset(localNodes.begin()->second),
                                       localNodes.begin()->first, errors,
                                       makeDirty, callNotify, actions,
                                       arangodb::MaintenanceFeature::ShardActionMap{});

  ASSERT_EQ(actions.size(), 1);
  ASSERT_EQ(actions.front()->name(), "DropDatabase");
  ASSERT_EQ(actions.front()->get("database"), "db3");
}

TEST_F(MaintenanceTestActionPhaseOne,
       local_databases_one_more_non_empty_database_should_be_dropped) {
  std::vector<std::shared_ptr<ActionDescription>> actions;
  localNodes.begin()->second("db3/col") = arangodb::velocypack::Slice::emptyObjectSlice();
  std::unordered_set<std::string> dirty{"db3"};
  bool callNotify = false;

  arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0, dirty,
                                       localToChangeset(localNodes.begin()->second),
                                       localNodes.begin()->first, errors,
                                       makeDirty, callNotify, actions,
                                       arangodb::MaintenanceFeature::ShardActionMap{});

  ASSERT_EQ(actions.size(), 1);
  ASSERT_EQ(actions[0]->name(), "DropDatabase");
  ASSERT_EQ(actions[0]->get("database"), "db3");
}

TEST_F(MaintenanceTestActionPhaseOne,
       add_one_collection_to_db3_in_plan_with_shards_for_all_db_servers) {
  std::string dbname("db3"), colname("x");

  plan = originalPlan;
  createPlanDatabase(dbname, plan);
  createPlanCollection(dbname, colname, 1, 3, plan);
  std::unordered_set<std::string> dirty{"db3"};
  bool callNotify = false;

  for (auto node : localNodes) {
    std::vector<std::shared_ptr<ActionDescription>> actions;

    node.second("db3") = arangodb::velocypack::Slice::emptyObjectSlice();

    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0, dirty,
                                         localToChangeset(node.second), node.first,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), 1);
    for (auto const& action : actions) {
      ASSERT_EQ(action->name(), "CreateCollection");
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne,
       add_one_collection_to_db3_in_plan_with_shards_for_all_db_servers_shard_locked) {
  // This tests that phaseOne does not consider a shard which is locked.
  std::string dbname("db3"), colname("x");
  std::unordered_set<std::string> dirty{"db3"};
  bool callNotify = false;

  plan = originalPlan;
  createPlanDatabase(dbname, plan);
  createPlanCollection(dbname, colname, 1, 3, plan);

  auto cid = collectionMap(plan).at("db3/x");
  auto shards = plan({COLLECTIONS, dbname, cid}).hasAsChildren(SHARDS);
  ASSERT_TRUE(shards.second);
  ASSERT_EQ(shards.first.size(), 1);
  std::string shardName = shards.first.begin()->first;

  for (auto node : localNodes) {
    std::vector<std::shared_ptr<ActionDescription>> actions;

    node.second("db3") = arangodb::velocypack::Slice::emptyObjectSlice();

    arangodb::maintenance::diffPlanLocal(
        engine, planToChangeset(plan), 0, dirty, localToChangeset(node.second),
        node.first, errors, makeDirty, callNotify, actions,
        arangodb::MaintenanceFeature::ShardActionMap{
            {shardName,
             std::make_shared<ActionDescription>(std::map<std::string, std::string>(
                                                     {{"name", CREATE_COLLECTION}}),
                                                 0, true)}});

    ASSERT_EQ(actions.size(), 0);
  }
}

TEST_F(MaintenanceTestActionPhaseOne,
       add_two_more_collections_to_db3_in_plan_with_shards_for_all_db_servers) {
  std::string dbname("db3"), colname1("x"), colname2("y");
  std::unordered_set<std::string> dirty{"db3"};
  bool callNotify = false;

  plan = originalPlan;
  createPlanDatabase(dbname, plan);
  createPlanCollection(dbname, colname1, 1, 3, plan);
  createPlanCollection(dbname, colname2, 1, 3, plan);

  for (auto node : localNodes) {
    std::vector<std::shared_ptr<ActionDescription>> actions;

    node.second("db3") = arangodb::velocypack::Slice::emptyObjectSlice();

    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0, dirty,
                                         localToChangeset(node.second), node.first,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), 2);
    for (auto const& action : actions) {
      ASSERT_EQ(action->name(), "CreateCollection");
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne, add_an_index_to_queues) {
  plan = originalPlan;
  auto cid = collectionMap(plan).at("_system/_queues");
  auto shards = plan({"Collections", "_system", cid, "shards"}).children();
  std::unordered_set<std::string> dirty{"_system"};
  bool callNotify = false;

  createPlanIndex("_system", cid, "hash", {"someField"}, false, false, false, plan);

  for (auto node : localNodes) {
    std::vector<std::shared_ptr<ActionDescription>> actions;

    auto local = node.second;

    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0, dirty,
                                         localToChangeset(local), node.first,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    size_t n = 0;
    for (auto const& shard : shards) {
      if (local.has({"_system", shard.first})) {
        ++n;
      }
    }

    ASSERT_EQ(actions.size(), n);
    for (auto const& action : actions) {
      ASSERT_EQ(action->name(), "EnsureIndex");
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne, remove_an_index_from_plan) {
  std::string dbname("_system");
  std::string indexes("indexes");

  plan = originalPlan;
  auto cid = collectionMap(plan).at("_system/bar");
  auto shards = plan({"Collections", dbname, cid, "shards"}).children();

  plan({"Collections", dbname, cid, indexes}).handle<POP>(
    arangodb::velocypack::Slice::emptyObjectSlice());
  std::unordered_set<std::string> dirty{"_system"};
  bool callNotify = false;

  for (auto node : localNodes) {

    std::vector<std::shared_ptr<ActionDescription>> actions;
    auto local = node.second;

    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0, dirty,
                                         localToChangeset(local), node.first,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    size_t n = 0;
    for (auto const& shard : shards) {
      if (local.has({"_system", shard.first})) {
        ++n;
      }
    }

    ASSERT_EQ(actions.size(), n);
    for (auto const& action : actions) {
      ASSERT_EQ(action->name(), "DropIndex");
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne, add_one_collection_to_local) {
  plan = originalPlan;
  std::unordered_set<std::string> dirty{"_system"};

  for (auto node : localNodes) {
    std::vector<std::shared_ptr<ActionDescription>> actions;
    createLocalCollection("_system", "1111111", node.second);
    std::unordered_set<std::string> dirty{"_system"};
    bool callNotify = false;

    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0, dirty,
                                         localToChangeset(node.second), node.first,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), 1);
    for (auto const& action : actions) {
      ASSERT_EQ(action->name(), "DropCollection");
      ASSERT_EQ(action->get("database"), "_system");
      ASSERT_EQ(action->get("shard"), "s1111112");
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne,
       modify_waitforsync_in_plan_should_update_the_according_collection) {
  VPackBuilder v;
  v.add(VPackValue(true));
  std::string dbname = "_system";
  std::string prop = arangodb::maintenance::WAIT_FOR_SYNC;
  std::unordered_set<std::string> dirty{dbname};
  bool callNotify = false;

  for (auto node : localNodes) {
    std::vector<std::shared_ptr<ActionDescription>> actions;

    auto cb = node.second(dbname).children().begin()->second->toBuilder();
    auto collection = cb.slice();
    auto shname = collection.get(NAME).copyString();

    (*node.second(dbname).children().begin()->second)(prop) = v.slice();

    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0, dirty,
                                         localToChangeset(node.second), node.first,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    if (actions.size() != 1) {
      std::cout << __FILE__ << ":" << __LINE__ << " " << actions  << std::endl;
    }
    ASSERT_EQ(actions.size(), 1);
    for (auto const& action : actions) {

      ASSERT_EQ(action->name(), "UpdateCollection");
      ASSERT_EQ(action->get("shard"), shname);
      ASSERT_EQ(action->get("database"), dbname);
      auto const props = action->properties();

    }
  }
}


TEST_F(MaintenanceTestActionPhaseOne, leader_behaviour_plan_self_local_self) {
  plan = originalPlan;
  takeLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto [server, local]  : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), server, originalPlan);
    takeLeadershipLocal(dbName(), relevantShards, local);

    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0,
                                         dirty, localToChangeset(local), server,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), 0);
  }
}

TEST_F(MaintenanceTestActionPhaseOne, leader_behaviour_plan_resign_self_local_self) {
  plan = originalPlan;
  resignLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto [server, local]  : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), server, originalPlan);
    takeLeadershipLocal(dbName(), relevantShards, local);

    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0,
                                         dirty, localToChangeset(local), server,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), 2);
    for (auto const& action : actions) {
      assertIsResignLeadershipAction(*action, dbName());
      auto shardName = action->get(SHARD);
      auto removed = relevantShards.erase(shardName);
      EXPECT_EQ(removed, 1) << "We created a JOB for a shard we do not expect " << shardName;
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne, leader_behaviour_plan_other_local_self) {
  plan = originalPlan;
  otherTakeLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto [server, local]  : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), server, originalPlan);
    takeLeadershipLocal(dbName(), relevantShards, local);

    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0,
                                         dirty, localToChangeset(local), server,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), 2);
    for (auto const& action : actions) {
      assertIsResignLeadershipAction(*action, dbName());
      auto shardName = action->get(SHARD);
      auto removed = relevantShards.erase(shardName);
      EXPECT_EQ(removed, 1) << "We created a JOB for a shard we do not expect " << shardName;
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne, leader_behaviour_plan_resign_other_local_self) {
  plan = originalPlan;
  otherTakeResignedLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto [server, local] : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), server, originalPlan);
    takeLeadershipLocal(dbName(), relevantShards, local);

    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0,
                                         dirty, localToChangeset(local), server,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    for (auto const& action : actions) {
      assertIsResignLeadershipAction(*action, dbName());
      auto shardName = action->get(SHARD);
      auto removed = relevantShards.erase(shardName);
      EXPECT_EQ(removed, 1) << "We created a JOB for a shard we do not expect " << shardName;
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne, leader_behaviour_plan_self_local_other) {
  plan = originalPlan;
  takeLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto [server, local] : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), server, originalPlan);
    otherTakeLeadershipLocal(dbName(), relevantShards, local);

    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0,
                                         dirty, localToChangeset(local), server,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), 2);
    for (auto const& action : actions) {
      assertIsTakeoverLeadershipAction(*action, dbName(), planId());
      auto shardName = action->get(SHARD);
      auto removed = relevantShards.erase(shardName);
      EXPECT_EQ(removed, 1) << "We created a JOB for a shard we do not expect " << shardName;
      EXPECT_EQ(action->get(THE_LEADER), "");
      EXPECT_EQ(action->get(LOCAL_LEADER), unusedServer());
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne, leader_behaviour_plan_resign_self_local_other) {
  plan = originalPlan;
  resignLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto [server, local]  : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), server, originalPlan);
    otherTakeLeadershipLocal(dbName(), relevantShards, local);

    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0,
                                         dirty, localToChangeset(local), server,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), 2);
    for (auto const& action : actions) {
      assertIsResignLeadershipAction(*action, dbName());
      auto shardName = action->get(SHARD);
      auto removed = relevantShards.erase(shardName);
      EXPECT_EQ(removed, 1) << "We created a JOB for a shard we do not expect " << shardName;
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne, leader_behaviour_plan_other_local_other) {
  plan = originalPlan;
  otherTakeLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto [server, local] : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), server, originalPlan);
    otherTakeLeadershipLocal(dbName(), relevantShards, local);

    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0,
                                         dirty, localToChangeset(local), server,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), 0);
  }
}

TEST_F(MaintenanceTestActionPhaseOne, leader_behaviour_plan_resign_other_local_other) {
  plan = originalPlan;
  otherTakeResignedLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto [server, local] : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), server, originalPlan);
    otherTakeLeadershipLocal(dbName(), relevantShards, local);

    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0,
                                         dirty, localToChangeset(local), server,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), 0);
  }
}

TEST_F(MaintenanceTestActionPhaseOne, leader_behaviour_plan_self_local_resigned) {
  plan = originalPlan;
  takeLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto [server, local]  : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), server, originalPlan);
    resignLeadershipLocal(dbName(), relevantShards, local);

    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0,
                                         dirty, localToChangeset(local), server,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), 2);
    for (auto const& action : actions) {
      assertIsTakeoverLeadershipAction(*action, dbName(), planId());
      auto shardName = action->get(SHARD);
      auto removed = relevantShards.erase(shardName);
      EXPECT_EQ(removed, 1) << "We created a JOB for a shard we do not expect " << shardName;
      EXPECT_EQ(action->get(THE_LEADER), "");
      EXPECT_EQ(action->get(LOCAL_LEADER), ResignShardLeadership::LeaderNotYetKnownString);
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne, leader_behaviour_plan_resign_self_local_resigned) {
  plan = originalPlan;
  resignLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto [server, local]  : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), server, originalPlan);
    resignLeadershipLocal(dbName(), relevantShards, local);

    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0,
                                         dirty, localToChangeset(local), server,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), 0);
  }
}

TEST_F(MaintenanceTestActionPhaseOne, leader_behaviour_plan_other_local_resigned) {
  plan = originalPlan;
  otherTakeLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto [server, local]  : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), server, originalPlan);
    resignLeadershipLocal(dbName(), relevantShards, local);

    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0,
                                         dirty, localToChangeset(local), server,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    // Synchronize in Phase 2 is responsible for this.
    ASSERT_EQ(actions.size(), 0);
  }
}

TEST_F(MaintenanceTestActionPhaseOne, leader_behaviour_plan_resign_other_local_resigned) {
  plan = originalPlan;
  otherTakeResignedLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto [server, local]  : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), server, originalPlan);
    resignLeadershipLocal(dbName(), relevantShards, local);

    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0,
                                         dirty, localToChangeset(local), server,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    // Synchronize in Phase 2 is responsible for this.
    ASSERT_EQ(actions.size(), 0);
  }
}

TEST_F(MaintenanceTestActionPhaseOne, leader_behaviour_plan_self_local_reboot) {
  plan = originalPlan;
  takeLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto [server, local]  : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), server, originalPlan);
    rebootLeadershipLocal(dbName(), relevantShards, local);

    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0,
                                         dirty, localToChangeset(local), server,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), 2);
    for (auto const& action : actions) {
      assertIsTakeoverLeadershipAction(*action, dbName(), planId());
      auto shardName = action->get(SHARD);
      auto removed = relevantShards.erase(shardName);
      EXPECT_EQ(removed, 1) << "We created a JOB for a shard we do not expect " << shardName;
      EXPECT_EQ(action->get(THE_LEADER), "");
      EXPECT_EQ(action->get(LOCAL_LEADER), LEADER_NOT_YET_KNOWN);
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne, leader_behaviour_plan_resign_self_local_reboot) {
  plan = originalPlan;
  resignLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto [server, local]  : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), server, originalPlan);
    rebootLeadershipLocal(dbName(), relevantShards, local);

    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0,
                                         dirty, localToChangeset(local), server,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), 2);
    for (auto const& action : actions) {
      assertIsResignLeadershipAction(*action, dbName());
      auto shardName = action->get(SHARD);
      auto removed = relevantShards.erase(shardName);
      EXPECT_EQ(removed, 1) << "We created a JOB for a shard we do not expect " << shardName;
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne, leader_behaviour_plan_other_local_reboot) {
  plan = originalPlan;
  otherTakeLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto [server, local]  : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), server, originalPlan);
    rebootLeadershipLocal(dbName(), relevantShards, local);

    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0,
                                         dirty, localToChangeset(local), server,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    // We will just resign in this case to get a clear state.
    ASSERT_EQ(actions.size(), 2);
    for (auto const& action : actions) {
      assertIsResignLeadershipAction(*action, dbName());
      auto shardName = action->get(SHARD);
      auto removed = relevantShards.erase(shardName);
      EXPECT_EQ(removed, 1) << "We created a JOB for a shard we do not expect " << shardName;
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne, leader_behaviour_plan_resign_other_local_reboot) {
  plan = originalPlan;
  otherTakeResignedLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto [server, local]  : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), server, originalPlan);
    rebootLeadershipLocal(dbName(), relevantShards, local);

    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0,
                                         dirty, localToChangeset(local), server,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    // We will just resign in this case to get a clear state.
    ASSERT_EQ(actions.size(), 2);
    for (auto const& action : actions) {
      assertIsResignLeadershipAction(*action, dbName());
      auto shardName = action->get(SHARD);
      auto removed = relevantShards.erase(shardName);
      EXPECT_EQ(removed, 1) << "We created a JOB for a shard we do not expect " << shardName;
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne, have_theleader_set_to_empty) {
  VPackBuilder v;
  v.add(VPackValue(std::string()));
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto node : localNodes) {
    std::vector<std::shared_ptr<ActionDescription>> actions;

    auto& collection = *node.second(dbName()).children().begin()->second;
    auto& leader = collection("theLeader");

    bool check = false;
    if (!leader.getString().empty()) {
      check = true;
      leader = v.slice();
    }
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0, dirty,
                                         localToChangeset(node.second), node.first,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    if (check) {
      ASSERT_EQ(actions.size(), 1);
      for (auto const& action : actions) {
        assertIsResignLeadershipAction(*action, dbName());
        ASSERT_EQ(action->get("shard"), collection("name").getString());
      }
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne, resign_leadership_plan) {
  plan = originalPlan;
  resignLeadershipPlan(dbName(), planId(), plan);
  std::unordered_set<std::string> dirty{dbName()};
  bool callNotify = false;

  for (auto node : localNodes) {
    auto relevantShards = getShardsForServer(dbName(), planId(), node.first, originalPlan);
    std::vector<std::shared_ptr<ActionDescription>> actions;
    // every server is responsible for two shards of dbName() and planId()
    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0, dirty,
                                         localToChangeset(node.second), node.first,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), relevantShards.size());
    for (auto const& action : actions) {
      assertIsResignLeadershipAction(*action, dbName());
      auto shardName = action->get(SHARD);
      auto removed = relevantShards.erase(shardName);
      EXPECT_EQ(removed, 1) << "We created a JOB for a shard we do not expect " << shardName;
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne,
       empty_db3_in_plan_should_drop_all_local_db3_collections_on_all_servers) {
  plan(PLAN_COL_PATH + "db3") = arangodb::velocypack::Slice::emptyObjectSlice();
  std::unordered_set<std::string> dirty{"db3"};
  bool callNotify = false;

  createPlanDatabase("db3", plan);

  for (auto& node : localNodes) {
    std::vector<std::shared_ptr<ActionDescription>> actions;
    node.second("db3") = node.second("_system");

    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0, dirty,
                                         localToChangeset(node.second), node.first,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), node.second("db3").children().size());
    for (auto const& action : actions) {
      ASSERT_EQ(action->name(), "DropCollection");
    }
  }
}

TEST_F(MaintenanceTestActionPhaseOne, resign_leadership) {
  plan = originalPlan;
  std::string const dbname("_system");
  std::string const colname("bar");
  auto cid = collectionMap(plan).at(dbname + "/" + colname);
  auto& shards = plan({"Collections", dbname, cid, "shards"}).children();
  std::unordered_set<std::string> dirty{dbname};
  bool callNotify = false;

  for (auto const& node : localNodes) {
    std::vector<std::shared_ptr<ActionDescription>> actions;

    std::string shname;

    for (auto const& shard : shards) {
      shname = shard.first;
      auto shardBuilder = shard.second->toBuilder();
      Slice servers = shardBuilder.slice();

      ASSERT_TRUE(servers.isArray());
      ASSERT_EQ(servers.length(), 2);
      auto const leader = servers[0].copyString();
      auto const follower = servers[1].copyString();

      if (leader == node.first) {
        VPackBuilder newServers;
        {
          VPackArrayBuilder a(&newServers);
          newServers.add(VPackValue(std::string("_") + leader));
          newServers.add(VPackValue(follower));
        }
        plan({"Collections", dbname, cid, "shards", shname}) = newServers.slice();
        break;
      }
    }

    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0, dirty,
                                         localToChangeset(node.second), node.first,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    ASSERT_EQ(actions.size(), 1);
    assertIsResignLeadershipAction(*actions[0], "_system");
    ASSERT_EQ(actions[0]->get(SHARD), shname);
  }
}

TEST_F(MaintenanceTestActionPhaseOne, removed_follower_in_plan_must_be_dropped) {
  plan = originalPlan;
  std::string const dbname("_system");
  std::string const colname("bar");
  auto cid = collectionMap(plan).at(dbname + "/" + colname);
  Node::Children& shards = plan({"Collections", dbname, cid, "shards"}).children();
  auto firstShard = shards.begin();
  VPackBuilder b = firstShard->second->toBuilder();
  std::string const shname = firstShard->first;
  std::string const leaderName = b.slice()[0].copyString();
  std::string const followerName = b.slice()[1].copyString();
  firstShard->second->handle<POP>(arangodb::velocypack::Slice::emptyObjectSlice());
  std::unordered_set<std::string> dirty{dbname};
  bool callNotify = false;

  for (auto const& node : localNodes) {
    std::vector<std::shared_ptr<ActionDescription>> actions;

    arangodb::maintenance::diffPlanLocal(engine, planToChangeset(plan), 0, dirty,
                                         localToChangeset(node.second), node.first,
                                         errors, makeDirty, callNotify, actions,
                                         arangodb::MaintenanceFeature::ShardActionMap{});

    if (node.first == followerName) {
      // Must see an action dropping the shard
      ASSERT_EQ(actions.size(), 1);
      ASSERT_EQ(actions[0]->name(), "DropCollection");
      ASSERT_EQ(actions[0]->get(DATABASE), dbname);
      ASSERT_EQ(actions[0]->get(SHARD), shname);
    } else if (node.first == leaderName) {
      // Must see an UpdateCollection action to drop the follower
      ASSERT_EQ(actions.size(), 1);
      ASSERT_EQ(actions[0]->name(), "UpdateCollection");
      ASSERT_EQ(actions[0]->get(DATABASE), dbname);
      ASSERT_EQ(actions[0]->get(SHARD), shname);
      ASSERT_EQ(actions[0]->get(FOLLOWERS_TO_DROP), followerName);
    } else {
      // No actions required
      ASSERT_EQ(actions.size(), 0);
    }
  }
}
