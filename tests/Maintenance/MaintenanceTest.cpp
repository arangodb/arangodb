////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for Cluster maintenance
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
/// @author Matthew Von-Maszewski
/// @author Copyright 2017-2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"

#include "Cluster/Maintenance.h"
#include "MMFiles/MMFilesEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <fstream>
#include <iterator>
#include <iostream>
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

std::map<std::string, std::string> matchShortLongIds(Node const& supervision) {
  std::map<std::string,std::string> ret;
  for (auto const& dbs : supervision("Health").children()) {
    if (dbs.first.front() == 'P') {
      ret.emplace((*dbs.second)("ShortName").getString(),dbs.first);
    }
  }
  return ret;
}

Node createNodeFromBuilder(Builder const& builder) {
  Builder opBuilder;
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

  Builder builder;
  builder.add(parser.steal()->slice());
  return builder;

}

Node createNode(char const* c) {
  return createNodeFromBuilder(createBuilder(c));
}

// Random stuff
std::random_device rd;
std::mt19937 g(rd());

// Relevant agency
Node plan("");
Node originalPlan("");
Node supervision("");
Node current("");

std::vector<std::string> const shortNames {
  "DBServer0001","DBServer0002","DBServer0003"};

// map <shortId, UUID>
std::map<std::string,std::string> dbsIds;

std::string const PLAN_COL_PATH = "/Collections/";
std::string const PLAN_DB_PATH = "/Databases/";

size_t localId =  1016002;

VPackBuilder createDatabase(std::string const& dbname) {
  Builder builder;
  { VPackObjectBuilder o(&builder);
    builder.add("id", VPackValue(std::to_string(localId++)));
    builder.add(
      "coordinator", VPackValue("CRDN-42df19c3-73d5-48f4-b02e-09b29008eff8"));
    builder.add(VPackValue("options"));
    { VPackObjectBuilder oo(&builder); }
    builder.add("name", VPackValue(dbname)); }
  return builder;
}

void createPlanDatabase(std::string const& dbname, Node& plan) {
  plan(PLAN_DB_PATH + dbname) = createDatabase(dbname).slice();
}

VPackBuilder createIndex(
  std::string const& type, std::vector<std::string> const& fields,
  bool unique, bool sparse, bool deduplicate) {

  VPackBuilder index;
  { VPackObjectBuilder o(&index);
    { index.add("deduplicate", VPackValue(deduplicate));
      index.add(VPackValue("fields"));
      { VPackArrayBuilder a(&index);
        for (auto const& field : fields) {
          index.add(VPackValue(field));
        }}
      index.add("id", VPackValue(std::to_string(localId++)));
      index.add("sparse", VPackValue(sparse));
      index.add("type", VPackValue(type));
      index.add("unique", VPackValue(unique));
    }}
  
  return index;
}

void createPlanIndex(
  std::string const& dbname, std::string const& colname,
  std::string const& type, std::vector<std::string> const& fields,
  bool unique, bool sparse, bool deduplicate, Node& plan) {

  VPackBuilder val;
  { VPackObjectBuilder o(&val); 
    val.add("new", createIndex(type, fields, unique, sparse, deduplicate).slice()); }
  plan(PLAN_COL_PATH + dbname + "/" + colname + "/indexes").handle<PUSH>(val.slice());
}

void createCollection(
  std::string const& colname, VPackBuilder& col) {

  VPackBuilder keyOptions;
  { VPackObjectBuilder o(&keyOptions);
    keyOptions.add("lastValue", VPackValue(0));
    keyOptions.add("type", VPackValue("traditional"));
    keyOptions.add("allowUserKeys", VPackValue(true)); }
  
  VPackBuilder shardKeys;
  { VPackArrayBuilder a(&shardKeys);
    shardKeys.add(VPackValue("_key")); }
  
  VPackBuilder indexes;
  { VPackArrayBuilder a(&indexes);
    indexes.add(createIndex("primary", {"_key"}, true, false, false).slice()); }
  
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

std::string S("s");
std::string C("c");

void createPlanShards (
  size_t numberOfShards, size_t replicationFactor, VPackBuilder& col) {
  
  auto servers = shortNames;
  std::shuffle(servers.begin(), servers.end(), g);

  col.add("numberOfShards", VPackValue(1));
  col.add("replicationFactor", VPackValue(2));
  col.add(VPackValue("shards"));
  { VPackObjectBuilder s(&col);
    for (size_t i = 0; i < numberOfShards; ++i) {
      col.add(VPackValue(S + std::to_string(localId++)));
      { VPackArrayBuilder a(&col);
        size_t j = 0;
        for (auto const& server : servers) {
          if (j++ < replicationFactor) {
            col.add(VPackValue(dbsIds[server]));
          }
        }}}}
}

void createPlanCollection(
  std::string const& dbname, std::string const& colname, 
  size_t numberOfShards, size_t replicationFactor, Node& plan) {

  VPackBuilder tmp;
  { VPackObjectBuilder o(&tmp);
    createCollection(colname, tmp);
    tmp.add("isSmart", VPackValue(false));
    tmp.add("deleted", VPackValue(false));
    createPlanShards(numberOfShards, replicationFactor, tmp);}
  
  Slice col = tmp.slice();
  auto id = col.get("id").copyString();
  plan(PLAN_COL_PATH + dbname + "/" + col.get("id").copyString()) = col;
  
}

void createLocalCollection(
  std::string const& dbname, std::string const& colname, Node& node) {

  size_t planId = std::stoull(colname);
  VPackBuilder tmp;
  { VPackObjectBuilder o(&tmp);
    createCollection(colname, tmp); 
    tmp.add("planId", VPackValue(colname));
    tmp.add("theLeader", VPackValue(""));
    tmp.add("globallyUniqueId",
            VPackValue(C + colname + "/" + S + std::to_string(planId+1)));
    tmp.add("objectId", VPackValue("9031415")); }
  node(dbname + "/" + S + std::to_string(planId+1)) = tmp.slice();

}

std::map<std::string, std::string> collectionMap(Node const& plan) {
  std::map<std::string, std::string> ret;
  auto const pb = plan("Collections").toBuilder();
  auto const ps = pb.slice();
  for (auto const& db : VPackObjectIterator(ps)) {
    for (auto const& col : VPackObjectIterator(db.value)) {
      ret.emplace(
        db.key.copyString()+"/"+col.value.get("name").copyString(),
        col.key.copyString());
    }
  }
  return ret;
}



namespace arangodb {
class LogicalCollection;
}

TEST_CASE("ActionDescription", "[cluster][maintenance]") {
  plan = createNode(planStr);
  originalPlan = plan;
  supervision = createNode(supervisionStr);
  current = createNode(currentStr);
  
  dbsIds = matchShortLongIds(supervision);

  SECTION("Construct minimal ActionDescription") {
    ActionDescription desc(std::map<std::string,std::string>{{"name", "SomeAction"}});
    REQUIRE(desc.get("name") == "SomeAction");
  }

  SECTION("Construct minimal ActionDescription with nullptr props") {
    std::shared_ptr<VPackBuilder> props;
    ActionDescription desc({{"name", "SomeAction"}}, props);
  }

  SECTION("Construct minimal ActionDescription with empty props") {
    std::shared_ptr<VPackBuilder> props;
    ActionDescription desc({{"name", "SomeAction"}}, props);
    REQUIRE(desc.get("name") == "SomeAction");
  }

  SECTION("Retrieve non-assigned key from ActionDescription") {
    std::shared_ptr<VPackBuilder> props;
    ActionDescription desc({{"name", "SomeAction"}}, props);
    REQUIRE(desc.get("name") == "SomeAction");
    try {
      auto bogus = desc.get("bogus");
      REQUIRE(bogus == "bogus");
    } catch (std::out_of_range const& e) {}
    std::string value;
    auto res = desc.get("bogus", value);
    REQUIRE(value.empty());
    REQUIRE(!res.ok());
  }

  SECTION("Retrieve non-assigned key from ActionDescription") {
    std::shared_ptr<VPackBuilder> props;
    ActionDescription desc({{"name", "SomeAction"}, {"bogus", "bogus"}}, props);
    REQUIRE(desc.get("name") == "SomeAction");
    try {
      auto bogus = desc.get("bogus");
      REQUIRE(bogus == "bogus");
    } catch (std::out_of_range const& e) {}
    std::string value;
    auto res = desc.get("bogus", value);
    REQUIRE(value == "bogus");
    REQUIRE(res.ok());
  }

  SECTION("Retrieve non-assigned properties from ActionDescription") {
    std::shared_ptr<VPackBuilder> props;
    ActionDescription desc({{"name", "SomeAction"}}, props);
    REQUIRE(desc.get("name") == "SomeAction");
    REQUIRE(desc.properties() == nullptr);
  }

  SECTION("Retrieve empty properties from ActionDescription") {
    auto props = std::make_shared<VPackBuilder>();
    ActionDescription desc({{"name", "SomeAction"}}, props);
    REQUIRE(desc.get("name") == "SomeAction");
    REQUIRE(desc.properties()->isEmpty());
  }

  SECTION("Retrieve empty object properties from ActionDescription") {
    auto props = std::make_shared<VPackBuilder>();
    { VPackObjectBuilder empty(props.get()); }
    ActionDescription desc({{"name", "SomeAction"}}, props);
    REQUIRE(desc.get("name") == "SomeAction");
    REQUIRE(desc.properties()->slice().isEmptyObject());
  }

  SECTION("Retrieve string value from ActionDescription's properties") {
    auto props = std::make_shared<VPackBuilder>();
    { VPackObjectBuilder obj(props.get());
      props->add("hello", VPackValue("world")); }
    ActionDescription desc({{"name", "SomeAction"}}, props);
    REQUIRE(desc.get("name") == "SomeAction");
    REQUIRE(desc.properties()->slice().hasKey("hello"));
    REQUIRE(desc.properties()->slice().get("hello").copyString() == "world");
  }

  SECTION("Retrieve double value from ActionDescription's properties") {
    double pi = 3.14159265359;
    auto props = std::make_shared<VPackBuilder>();
    { VPackObjectBuilder obj(props.get());
      props->add("pi", VPackValue(3.14159265359)); }
    ActionDescription desc({{"name", "SomeAction"}}, props);
    REQUIRE(desc.get("name") == "SomeAction");
    REQUIRE(desc.properties()->slice().hasKey("pi"));
    REQUIRE(
      desc.properties()->slice().get("pi").getNumber<double>() == pi);
  }

  SECTION("Retrieve integer value from ActionDescription's properties") {
    size_t one = 1;
    auto props = std::make_shared<VPackBuilder>();
    { VPackObjectBuilder obj(props.get());
      props->add("one", VPackValue(one)); }
    ActionDescription desc({{"name", "SomeAction"}}, props);
    REQUIRE(desc.get("name") == "SomeAction");
    REQUIRE(desc.properties()->slice().hasKey("one"));
    REQUIRE(
      desc.properties()->slice().get("one").getNumber<size_t>() == one);
  }

  SECTION("Retrieve array value from ActionDescription's properties") {
    double pi = 3.14159265359;
    size_t one = 1;
    std::string hello("hello world!");
    auto props = std::make_shared<VPackBuilder>();
    { VPackObjectBuilder obj(props.get());
      props->add(VPackValue("array"));
      { VPackArrayBuilder arr(props.get());
        props->add(VPackValue(pi));
        props->add(VPackValue(one));
        props->add(VPackValue(hello)); }}
    ActionDescription desc({{"name", "SomeAction"}}, props);
    REQUIRE(desc.get("name") == "SomeAction");
    REQUIRE(desc.properties()->slice().hasKey("array"));
    REQUIRE(desc.properties()->slice().get("array").isArray());
    REQUIRE(desc.properties()->slice().get("array").length() == 3);
    REQUIRE(desc.properties()->slice().get("array")[0].getNumber<double>()==pi);
    REQUIRE(desc.properties()->slice().get("array")[1].getNumber<size_t>()==one);
    REQUIRE(desc.properties()->slice().get("array")[2].copyString() == hello );
  }

}

TEST_CASE("ActionPhaseOne", "[cluster][maintenance]") {
  std::shared_ptr<arangodb::options::ProgramOptions> po =
    std::make_shared<arangodb::options::ProgramOptions>(
      "test", std::string(), std::string(), "path");
  arangodb::application_features::ApplicationServer as(po, nullptr);
  TestMaintenanceFeature feature(as);
  MaintenanceFeature::errors_t errors;

  std::map<std::string, Node> localNodes {
    {dbsIds[shortNames[0]], createNode(dbs0Str)},
    {dbsIds[shortNames[1]], createNode(dbs1Str)},
    {dbsIds[shortNames[2]], createNode(dbs2Str)}};

  arangodb::MMFilesEngine engine(as); // arbitrary implementation that has index types registered
  auto* origStorageEngine = arangodb::EngineSelectorFeature::ENGINE;
  arangodb::EngineSelectorFeature::ENGINE = &engine;
  auto resetStorageEngine = std::shared_ptr<void>(&engine, [origStorageEngine](void*)->void { arangodb::EngineSelectorFeature::ENGINE = origStorageEngine; });

  SECTION("In sync should have 0 effects") {

    std::vector<ActionDescription> actions;

    for (auto const& node : localNodes) {
      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, errors, feature, actions);

      if (actions.size() != 0) {
        std::cout << __FILE__ << ":" << __LINE__ << " " << actions  << std::endl;
      }
      REQUIRE(actions.size() == 0);
    }

  }

  
  SECTION("Local databases one more empty database should be dropped") {

    std::vector<ActionDescription> actions;
    
    localNodes.begin()->second("db3") =
      arangodb::velocypack::Slice::emptyObjectSlice();

    arangodb::maintenance::diffPlanLocal(
      plan.toBuilder().slice(), localNodes.begin()->second.toBuilder().slice(),
      localNodes.begin()->first, errors, feature, actions);

    if (actions.size() != 1) {
      std::cout << __FILE__ << ":" << __LINE__ << " " << actions  << std::endl;
    }
    REQUIRE(actions.size() == 1);
    REQUIRE(actions.front().name() == "DropDatabase");
    REQUIRE(actions.front().get("database") == "db3");

  }


  SECTION("Local databases one more non empty database should be dropped") {

    std::vector<ActionDescription> actions;
    localNodes.begin()->second("db3/col") =
      arangodb::velocypack::Slice::emptyObjectSlice();

    arangodb::maintenance::diffPlanLocal(
      plan.toBuilder().slice(), localNodes.begin()->second.toBuilder().slice(),
      localNodes.begin()->first, errors, feature, actions);

    REQUIRE(actions.size() == 1);
    REQUIRE(actions.front().name() == "DropDatabase");
    REQUIRE(actions.front().get("database") == "db3");

  }


  SECTION(
    "Add one collection to db3 in plan with shards for all db servers") {

    std::string dbname("db3"), colname("x");
    
    plan = originalPlan;
    createPlanDatabase(dbname, plan);
    createPlanCollection(dbname, colname, 1, 3, plan);
    
    for (auto node : localNodes) {
      std::vector<ActionDescription> actions;
      
      node.second("db3") =
        arangodb::velocypack::Slice::emptyObjectSlice();

      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, errors, feature, actions);

      if (actions.size() != 1) {
        std::cout << __FILE__ << ":" << __LINE__ << " " << actions  << std::endl;
      }
      REQUIRE(actions.size() == 1);
      for (auto const& action : actions) {
        REQUIRE(action.name() == "CreateCollection");
      }
    }

  }


  SECTION("Add two more collection to db3 in plan with shards for all db servers") {

    std::string dbname("db3"), colname1("x"), colname2("y");

    plan = originalPlan;
    createPlanDatabase(dbname, plan);
    createPlanCollection(dbname, colname1, 1, 3, plan);
    createPlanCollection(dbname, colname2, 1, 3, plan);
    
    for (auto node : localNodes) {
      std::vector<ActionDescription> actions;

      node.second("db3") =
        arangodb::velocypack::Slice::emptyObjectSlice();

      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, errors, feature, actions);

      if (actions.size() != 2) {
        std::cout << __FILE__ << ":" << __LINE__ << " " << actions  << std::endl;
      }
      REQUIRE(actions.size() == 2);
      for (auto const& action : actions) {
        REQUIRE(action.name() == "CreateCollection");
      }
    }

  }

  
  SECTION("Add an index to _queues") {

    plan = originalPlan;
    auto cid = collectionMap(plan).at("_system/_queues");
    auto shards = plan({"Collections","_system",cid,"shards"}).children();
    
    createPlanIndex(
      "_system", cid, "hash", {"someField"}, false, false, false, plan);
    
    for (auto node : localNodes) {
      std::vector<ActionDescription> actions;

      auto local = node.second;
      
      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), local.toBuilder().slice(), node.first, errors, feature,
        actions);

      size_t n = 0;
      for (auto const& shard : shards) {
        if (local.has({"_system", shard.first})) {
          ++n;
        }
      }
        
      if (actions.size() != n) {
        std::cout << __FILE__ << ":" << __LINE__ << " " << actions  << std::endl;
      }
      REQUIRE(actions.size() == n);
      for (auto const& action : actions) {
        REQUIRE(action.name() == "EnsureIndex");
      }
    }

  }


  SECTION("Remove an index from plan") {

    std::string dbname("_system");
    std::string indexes("indexes");

    plan = originalPlan;
    auto cid = collectionMap(plan).at("_system/bar");
    auto shards = plan({"Collections",dbname,cid,"shards"}).children();
    
    plan({"Collections", dbname, cid, indexes}).handle<POP>(
      arangodb::velocypack::Slice::emptyObjectSlice());

    for (auto node : localNodes) {
      std::vector<ActionDescription> actions;
      
      auto local = node.second;
      
      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), local.toBuilder().slice(), node.first, errors, feature,
        actions);
      
      size_t n = 0;
      for (auto const& shard : shards) {
        if (local.has({"_system", shard.first})) {
          ++n;
        }
      }
      
      if (actions.size() != n) {
        std::cout << __FILE__ << ":" << __LINE__ << " " << actions  << std::endl;
      }
      REQUIRE(actions.size() == n);
      for (auto const& action : actions) {
        REQUIRE(action.name() == "DropIndex");
      }
    }

  }


  SECTION("Add one collection to local") {

    plan = originalPlan;
    
    for (auto node : localNodes) {

      std::vector<ActionDescription> actions;
      createLocalCollection("_system", "1111111", node.second);
      
      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, errors, feature, actions);
      
      if (actions.size() != 1) {
        std::cout << __FILE__ << ":" << __LINE__ << " " << actions  << std::endl;
      }
      REQUIRE(actions.size() == 1);
      for (auto const& action : actions) {
        REQUIRE(action.name() == "DropCollection");
        REQUIRE(action.get("database") == "_system");
        REQUIRE(action.get("collection") == "s1111112");
      }
    }

  }


  SECTION("Modify journalSize in plan should update the according collection") {

    VPackBuilder v; v.add(VPackValue(0));

    for (auto node : localNodes) {

      std::vector<ActionDescription> actions;
      std::string dbname = "_system";
      std::string prop = arangodb::maintenance::JOURNAL_SIZE;

      auto cb =
        node.second(dbname).children().begin()->second->toBuilder();
      auto collection = cb.slice();
      auto shname = collection.get(NAME).copyString();

      (*node.second(dbname).children().begin()->second)(prop) =
        v.slice();

      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, errors, feature, actions);

      /*
      if (actions.size() != 1) {
        std::cout << __FILE__ << ":" << __LINE__ << " " << actions  << std::endl;
      }
      REQUIRE(actions.size() == 1);
      for (auto const& action : actions) {

        REQUIRE(action.name() == "UpdateCollection");
        REQUIRE(action.get("shard") == shname);
        REQUIRE(action.get("database") == dbname);
        auto const props = action.properties();

      }
      */
    }
  }


  SECTION("Have theLeader set to empty") {

    VPackBuilder v; v.add(VPackValue(std::string()));

    for (auto node : localNodes) {

      std::vector<ActionDescription> actions;

      auto& collection = *node.second("foo").children().begin()->second;
      auto& leader = collection("theLeader");

      bool check = false;
      if (!leader.getString().empty()) {
        check = true;
        leader = v.slice();
      } 

      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, errors, feature, actions);

      if (check) {
        if (actions.size() != 1) {
          std::cout << __FILE__ << ":" << __LINE__ << " " << actions  << std::endl;
        }
        REQUIRE(actions.size() == 1);
        for (auto const& action : actions) {
          REQUIRE(action.name() == "UpdateCollection");
          REQUIRE(action.has("shard"));
          REQUIRE(action.get("shard") == collection("name").getString());
          REQUIRE(action.get("localLeader").empty());
        }
      }
    }
  }


  SECTION(
    "Empty db3 in plan should drop all local db3 collections on all servers") {

    plan(PLAN_COL_PATH + "db3") =
      arangodb::velocypack::Slice::emptyObjectSlice();

    createPlanDatabase("db3", plan);

    for (auto& node : localNodes) {

      std::vector<ActionDescription> actions;
      node.second("db3") = node.second("_system");

      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, errors, feature, actions);

      REQUIRE(actions.size() == node.second("db3").children().size());
      for (auto const& action : actions) {
        REQUIRE(action.name() == "DropCollection");
      }

    }

  }


  SECTION("Resign leadership") {

    plan = originalPlan;
    std::string const dbname("_system");
    std::string const colname("bar");
    auto cid = collectionMap(plan).at(dbname + "/" + colname);
    auto& shards = plan({"Collections",dbname,cid,"shards"}).children();

    for (auto const& node : localNodes) {
      std::vector<ActionDescription> actions;

      std::string shname;

      for (auto const& shard : shards) {
        shname = shard.first;
        auto shardBuilder = shard.second->toBuilder();
        Slice servers = shardBuilder.slice();

        REQUIRE(servers.isArray());
        REQUIRE(servers.length() == 2);
        auto const leader = servers[0].copyString();
        auto const follower = servers[1].copyString();

        if (leader == node.first) {

          VPackBuilder newServers;
          { VPackArrayBuilder a(&newServers);
            newServers.add(VPackValue(std::string("_") + leader));
            newServers.add(VPackValue(follower)); }
          plan({"Collections", dbname, cid, "shards", shname}) = newServers.slice();
          break;
        }
      }
      
      arangodb::maintenance::diffPlanLocal (
        plan.toBuilder().slice(), node.second.toBuilder().slice(), node.first,
        errors, feature, actions);

      if (actions.size() != 2) {
        std::cout << actions << std::endl;
      }
      REQUIRE(actions.size() == 2);
      REQUIRE(actions.front().name() == "UpdateCollection");
      REQUIRE(actions.front().get(DATABASE) == dbname);
      REQUIRE(actions.front().get(SHARD) == shname);
      REQUIRE(actions.front().get("localLeader") == std::string(""));
      REQUIRE(actions[1].name() == "ResignShardLeadership");
      REQUIRE(actions[1].get(DATABASE) == dbname);
      REQUIRE(actions[1].get(SHARD) == shname);
    }

  }

  SECTION( "Removed follower in Plan must be dropped" ) {
    plan = originalPlan;
    std::string const dbname("_system");
    std::string const colname("bar");
    auto cid = collectionMap(plan).at(dbname + "/" + colname);
    Node::Children& shards = plan({"Collections",dbname,cid,"shards"}).children();
    auto firstShard = shards.begin();
    VPackBuilder b = firstShard->second->toBuilder();
    std::string const shname = firstShard->first;
    std::string const leaderName = b.slice()[0].copyString();
    std::string const followerName = b.slice()[1].copyString();
    firstShard->second->handle<POP>(
      arangodb::velocypack::Slice::emptyObjectSlice());

    for (auto const& node : localNodes) {
      std::vector<ActionDescription> actions;
      
      arangodb::maintenance::diffPlanLocal (
        plan.toBuilder().slice(), node.second.toBuilder().slice(), node.first,
        errors, feature, actions);

      if (node.first == followerName) {
        // Must see an action dropping the shard
        REQUIRE(actions.size() == 1);
        REQUIRE(actions.front().name() == "DropCollection");
        REQUIRE(actions.front().get(DATABASE) == dbname);
        REQUIRE(actions.front().get(COLLECTION) == shname);
      } else if (node.first == leaderName) {
        // Must see an UpdateCollection action to drop the follower
        REQUIRE(actions.size() == 1);
        REQUIRE(actions.front().name() == "UpdateCollection");
        REQUIRE(actions.front().get(DATABASE) == dbname);
        REQUIRE(actions.front().get(SHARD) == shname);
        REQUIRE(actions.front().get(FOLLOWERS_TO_DROP) == followerName);
      } else {
        // No actions required
        REQUIRE(actions.size() == 0);
      }
    }

  }

}

TEST_CASE("ActionPhaseTwo", "[cluster][maintenance]") {

  plan = originalPlan;
  
  std::map<std::string, Node> localNodes {
    {dbsIds[shortNames[0]], createNode(dbs0Str)},
    {dbsIds[shortNames[1]], createNode(dbs1Str)},
    {dbsIds[shortNames[2]], createNode(dbs2Str)}};


/*  SECTION("Diffing local and current in equilibrium") {
    
    VPackSlice p = plan.toBuilder().slice();
    REQUIRE(p.hasKey("Collections"));
    REQUIRE(p.get("Collections").isObject());
    VPackSlice c = current.toBuilder().slice();

    for (auto const& node : localNodes) {

      VPackSlice l = node.second.toBuilder().slice();
      REQUIRE(c.isObject());
      REQUIRE(p.isObject());
      REQUIRE(l.isObject());

      VPackBuilder rb;
      { VPackObjectBuilder o(&rb);
        rb.add(VPackValue("phaseTwo"));
        { VPackObjectBuilder oo(&rb);
          arangodb::maintenance::reportInCurrent(p, c, l, node.first, rb); }}

      VPackSlice report = rb.slice();

      VPackSlice pt = report.get("PhaseTwo");
      REQUIRE(pt.isObject());
      
      if (!pt.isEmptyObject()) {
        std::cout << pt.toJson() << std::endl;
      }
      REQUIRE(pt.isEmptyObject());
      
    }
  }
*/
}

#endif