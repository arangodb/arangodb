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
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"

#include "Cluster/Maintenance.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>
#include <random>
#include <typeinfo>

using namespace arangodb;
using namespace arangodb::consensus;
using namespace arangodb::maintenance;

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

std::map<std::string,std::string> matchShortLongIds(Node const& supervision) {
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
auto plan = createNode(planStr);
auto supervision = createNode(supervisionStr);
auto current = createNode(currentStr);

std::vector<std::string> shortNames {
  "DBServer0001","DBServer0002","DBServer0003"};

// map <shortId, UUID>
auto dbsIds = matchShortLongIds(supervision);

std::string PLAN_COL_PATH = "/Collections/";
std::string PLAN_DB_PATH = "/Databases/";

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
  std::string dbpath = PLAN_DB_PATH + dbname;
  plan(dbpath) = createDatabase(dbname).slice();
}

void createPlanCollection(
  std::string const& dbname, std::string const& colname, 
  size_t numberOfShards, size_t replicationFactor, Node& plan) {

  std::string prefix("s");
  std::string colpath = PLAN_COL_PATH + dbname + "/" + colname;
  
  auto servers = shortNames;
  std::shuffle(servers.begin(), servers.end(), g);
  auto cid = localId++; 

  // shards map
  VPackBuilder shards;
  { VPackObjectBuilder s(&shards);
    for (size_t i = 0; i < numberOfShards; ++i) {
      shards.add(VPackValue(prefix + std::to_string(localId++)));
      { VPackArrayBuilder a(&shards);
        size_t j = 0;
        for (auto const& server : servers) {
          if (j++ < replicationFactor) {
            shards.add(VPackValue(dbsIds[server]));
          }
        }}}}

  VPackBuilder keyOptions;
  { VPackObjectBuilder o(&keyOptions);
    keyOptions.add("lastValue", VPackValue(0));
    keyOptions.add("type", VPackValue("traditional"));
    keyOptions.add("allowUserKeys", VPackValue(true)); }

  VPackBuilder shardKeys;
  { VPackArrayBuilder a(&shardKeys);
    shardKeys.add(VPackValue("_key")); }
  
  VPackBuilder fields;
  { VPackArrayBuilder a(&fields);
    fields.add(VPackValue("_key")); }

  VPackBuilder indexes;
  { VPackArrayBuilder a(&indexes);
    { VPackObjectBuilder o(&indexes);
      { indexes.add("fields", fields.slice());
        indexes.add("id", VPackValue("0"));
        indexes.add("sparse", VPackValue(false));
        indexes.add("type", VPackValue("primary"));
        indexes.add("unique", VPackValue(true));
      }}}

  VPackBuilder col;
  { VPackObjectBuilder o(&col);
    col.add("status", VPackValue(3));
    col.add("shards", shards.slice());
    col.add("keyOptions", keyOptions.slice());
    col.add("cacheEnabled", VPackValue(false));
    col.add("waitForSync", VPackValue(false));
    col.add("id", VPackValue(std::to_string(cid)));
    col.add("isSmart", VPackValue(false));
    col.add("type", VPackValue(2));
    col.add("numberOfShards", VPackValue(1));
    col.add("deleted", VPackValue(false));
    col.add("isSystem", VPackValue(true));
    col.add("name", VPackValue(colname));
    col.add("shardingStrategy", VPackValue("hash"));
    col.add("statusString", VPackValue("loaded"));
    col.add("replicationFactor", VPackValue(2));
    col.add("shardKeys", shardKeys.slice()); }
  
  plan(colpath) = col.slice();
  
}

void createPlanIndex(
  std::string const& dbname, std::string const& colname,
  std::string const& type, std::vector<std::string> const& fields) {
  
}

namespace arangodb {
class LogicalCollection;
}

TEST_CASE("ActionDescription", "[cluster][maintenance]") {

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

TEST_CASE("ActionPhases", "[cluster][maintenance]") {

  std::map<std::string, Node> localNodes {
    {dbsIds[shortNames[0]], createNode(dbs0Str)},
    {dbsIds[shortNames[1]], createNode(dbs1Str)},
    {dbsIds[shortNames[2]], createNode(dbs2Str)}};

  SECTION("In sync should have 0 effects") {

    std::vector<ActionDescription> actions;

    for (auto const& node : localNodes) {
      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, actions);

      if (actions.size() != 0) {
        std::cout << actions << std::endl;
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
      localNodes.begin()->first, actions);

    if (actions.size() != 1) {
      std::cout << actions << std::endl;
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
      localNodes.begin()->first, actions);

    REQUIRE(actions.size() == 1);
    REQUIRE(actions.front().name() == "DropDatabase");
    REQUIRE(actions.front().get("database") == "db3");

  }


  SECTION(
    "Add one more collection to db3 in plan with shards for all db servers") {

    std::string dbname("db3"), colname("x");

    createPlanDatabase(dbname, plan);
    createPlanCollection(dbname, colname, 1, 3, plan);
    
    for (auto node : localNodes) {
      std::vector<ActionDescription> actions;
      
      node.second("db3") =
        arangodb::velocypack::Slice::emptyObjectSlice();

      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, actions);

      if (actions.size() != 1) {
        std::cout << actions << std::endl;
      }
      REQUIRE(actions.size() == 1);
      for (auto const& action : actions) {
        REQUIRE(action.name() == "CreateCollection");
      }
    }

  }


  SECTION("Add two more collection to db3 in plan with shards for all db servers") {

    std::string dbname("db3"), colname("y");

    createPlanDatabase(dbname, plan);
    createPlanCollection(dbname, colname, 1, 3, plan);
    
    for (auto node : localNodes) {
      std::vector<ActionDescription> actions;

      node.second("db3") =
        arangodb::velocypack::Slice::emptyObjectSlice();

      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, actions);

      if (actions.size() != 2) {
        std::cout << actions << std::endl;
      }
      REQUIRE(actions.size() == 2);
      for (auto const& action : actions) {
        REQUIRE(action.name() == "CreateCollection");
      }
    }

  }

/*
  SECTION("Add one collection to local") {

    createPlanDatabase("db3", plan);
    
    for (auto node : localNodes) {
      std::vector<ActionDescription> actions;
      node.second("db3/1111111") =
        arangodb::velocypack::Slice::emptyObjectSlice();
      
      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, actions);
      
      if (actions.size() != 1) {
        std::cout << actions << std::endl;
      }
      REQUIRE(actions.size() == 1);
      for (auto const& action : actions) {
        REQUIRE(action.name() == "DropCollection");
        REQUIRE(action.get("database") == "db3");
        REQUIRE(action.get("collection") == "1111111");
      }
    }

  }

  // Plan also now has db3 =====================================================
  SECTION("Modify journalSize in plan should update the according collection") {

    VPackBuilder v; v.add(VPackValue(0));

    for (auto node : localNodes) {

      std::vector<ActionDescription> actions;
      std::string dbname = "_system";
      std::string prop = "journalSize";

      auto cb =
        node.second(dbname).children().begin()->second->toBuilder();
      auto collection = cb.slice();
      auto colname = collection.get(NAME).copyString();

      // colname is shard_name (and gets added to plan as side effect)
      //  ^^^ plan(PLAN_COL_PATH + dbname + "/" + colname + "/" + "journalSize");
      (*node.second(dbname).children().begin()->second)(prop) =
        v.slice();

      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, actions);

      if (actions.size() != 1) {
        std::cout << actions << std::endl;
      }
      REQUIRE(actions.size() == 1);
      for (auto const& action : actions) {

        REQUIRE(action.name() == "UpdateCollection");
        REQUIRE(action.get("collection") == colname);
        REQUIRE(action.get("database") == dbname);
        auto const props = action.properties();
// v is empty        REQUIRE(props->slice().get(prop).toJson() == v.slice().toJson());
      }

    }
  }

  // Plan also now has db3 =====================================================
  SECTION("Add one collection to local") {

    VPackBuilder v; v.add(VPackValue(std::string()));

    for (auto node : localNodes) {

      std::vector<ActionDescription> actions;

      (*node.second("_system").children().begin()->second)("theLeader") =
        v.slice();

      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, actions);

      std::cout << actions << std::endl;
      
      REQUIRE(actions.size() == 1);
      for (auto const& action : actions) {
        //REQUIRE(action.name() == "UpdateCollection");
      }

    }
  }

  // Plan also now has db3 =====================================================
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
        node.first, actions);

      REQUIRE(actions.size() == node.second("db3").children().size());
      for (auto const& action : actions) {
        REQUIRE(action.name() == "DropCollection");
      }

    }

  }

  // Local has databases _system and db3 =====================================
  SECTION("Local collections") {

    for (auto const& node : localNodes) {
      std::vector<ActionDescription> actions;
      arangodb::maintenance::diffPlanLocal (
        plan.toBuilder().slice(), node.second.toBuilder().slice(), node.first,
        actions);

      REQUIRE(actions.size() == 0);
    }

  }

  // Plan also now has db3 =====================================================
  SECTION("Indexes missing in plan should be removed locally") {

    plan(PLAN_COL_PATH  + "_system/1010021/indexes") =
      arangodb::velocypack::Slice::emptyArraySlice();

    for (auto& node : localNodes) {

      if (node.second.has("_system/s1010022/")) {

        std::vector<ActionDescription> actions;

        arangodb::maintenance::diffPlanLocal (
          plan.toBuilder().slice(), node.second.toBuilder().slice(), node.first,
          actions);

        std::cout << actions << std::endl;
        REQUIRE(actions.size() == 2);
        for (auto const action : actions) {
          REQUIRE(actions.front().name() == "DropIndex");
          REQUIRE(actions.front().get("collection") == "s1010022");
          REQUIRE(actions.front().get("database") == "_system");
        }

      }
    }

  }

  SECTION("Diffing local and current") {

    for (auto const& node : localNodes) {
      arangodb::maintenance::Transactions report;
      arangodb::maintenance::diffLocalCurrent(
        node.second.toBuilder().slice(), current.toBuilder().slice(), node.first,
        report);
      for (auto const& tp : report) {
        std::cout
          << "[ " << tp.first.toJson() << ", " << tp.second.toJson() << " ]"
          << std::endl;
      }
    }

  }
*/

}
