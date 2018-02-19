////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for ClusterComm
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

#include "Cluster/ActionRegistry.h"
#include "Cluster/Maintenance.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>
#include <typeinfo>

using namespace arangodb;
using namespace arangodb::consensus;
using namespace arangodb::maintenance;

char const* planStr =
#include "MaintenancePlan.json"
;
char const* currentStr =
#include "MaintenanceCurrent.json"
;
char const* dbs1Str = 
#include "MaintenanceDBServer0001.json"
;
char const* dbs2Str = 
#include "MaintenanceDBServer0002.json"
;
char const* dbs3Str =
#include "MaintenanceDBServer0003.json"
;

std::string PLAN_COL_PATH = "/arango/Plan/Collections/";

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

namespace arangodb {
class LogicalCollection;
}

TEST_CASE("Maintenance", "[cluster][maintenance][differencePlanLocal]") {

  auto plan = createNode(planStr);
  auto current = createNode(currentStr);

  std::vector<std::string> dbsIds;
  for (auto const& dbs : plan("/arango/Plan/DBServers").children()) {
    dbsIds.push_back(dbs.first);
  }
  
  std::map<std::string, Node> localNodes {
    {dbsIds[1], createNode(dbs1Str)}, {dbsIds[2], createNode(dbs2Str)},
                                      {dbsIds[0], createNode(dbs3Str)}};
  

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
    localNodes.begin()->second("db2") =
      arangodb::basics::VelocyPackHelper::EmptyObjectValue();
    
    arangodb::maintenance::diffPlanLocal(
      plan.toBuilder().slice(), localNodes.begin()->second.toBuilder().slice(),
      localNodes.begin()->first, actions);

    REQUIRE(actions.size() == 1);
    REQUIRE(actions.front().name() == "DropDatabase");
    REQUIRE(actions.front().get("database") == "db2");

  }


  SECTION("Local databases one more non empty database should be dropped") {

    std::vector<ActionDescription> actions;
    localNodes.begin()->second("db2/col") =
      arangodb::basics::VelocyPackHelper::EmptyObjectValue();
    
    arangodb::maintenance::diffPlanLocal(
      plan.toBuilder().slice(), localNodes.begin()->second.toBuilder().slice(),
      localNodes.begin()->first, actions);

    REQUIRE(actions.size() == 1);
    REQUIRE(actions.front().name() == "DropDatabase");
    REQUIRE(actions.front().get("database") == "db2");

  }

  
  SECTION("Local databases one more empty database should be dropped") {

    std::vector<ActionDescription> actions;
    localNodes.begin()->second("db2") =
      arangodb::basics::VelocyPackHelper::EmptyObjectValue();
    
    arangodb::maintenance::diffPlanLocal(
      plan.toBuilder().slice(), localNodes.begin()->second.toBuilder().slice(),
      localNodes.begin()->first, actions);

    REQUIRE(actions.size() == 1);
    REQUIRE(actions.front().name() == "DropDatabase");
    REQUIRE(actions.front().get("database") == "db2");

  }

  
  SECTION(
    "Add one more collection to db2 in plan with shards for all db servers") {

    VPackBuilder shards;
    { VPackObjectBuilder s(&shards);
      shards.add(VPackValue("s1016002"));
      { VPackArrayBuilder a(&shards);
        for (auto const& id : dbsIds) {
          shards.add(VPackValue(id));
        }}}

    std::string colpath = PLAN_COL_PATH + "db2/1016001";

    plan(colpath) =
      *(plan(PLAN_COL_PATH  + "_system").children().begin()->second);
    plan(PLAN_COL_PATH  + "db2/1016001/shards") = shards.slice();

    for (auto node : localNodes) {
      std::vector<ActionDescription> actions;

      node.second("db2") =
        arangodb::basics::VelocyPackHelper::EmptyObjectValue();
      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, actions);

      REQUIRE(actions.size() == 1);
      for (auto const& action : actions) {
        REQUIRE(action.name() == "CreateCollection");
      }
    }
    
  }

  // Plan also now has db3 =====================================================
  SECTION("Add one more collection to plan") {

    VPackBuilder shards;
    { VPackObjectBuilder s(&shards);
      shards.add(VPackValue("s1016002"));
      { VPackArrayBuilder a(&shards);
        for (auto const& id : dbsIds) {
          shards.add(VPackValue(id));
        }}}

    plan(PLAN_COL_PATH + "db2/1016001") =
      *(plan(PLAN_COL_PATH + "_system").children().begin()->second);
    plan(PLAN_COL_PATH + "db2/1016001/shards") = shards.slice();

    for (auto node : localNodes) {
      std::vector<ActionDescription> actions;

      node.second("db2") =
        arangodb::basics::VelocyPackHelper::EmptyObjectValue();
      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, actions);

      REQUIRE(actions.size() == 1);
      for (auto const& action : actions) {
        REQUIRE(action.name() == "CreateCollection");
      }
    }
    
  }

  // Plan also now has db3 =====================================================
  SECTION("Add one collection to local") {

    for (auto node : localNodes) {
      std::vector<ActionDescription> actions;
      node.second("db2/1111111") =
        arangodb::basics::VelocyPackHelper::EmptyObjectValue();
      plan(PLAN_COL_PATH + "db2") =
        arangodb::basics::VelocyPackHelper::EmptyObjectValue();
      
      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, actions);
      
      REQUIRE(actions.size() == 1);
      for (auto const& action : actions) {
        REQUIRE(action.name() == "DropCollection");
        REQUIRE(action.get("database") == "db2");
        REQUIRE(action.get("collection") == "1111111");
      }
    }
    
  }
  
  // Plan also now has db3 =====================================================
  SECTION("Add one collection to local") {

    VPackBuilder v;
    v.add(VPackValue(0));

    for (auto node : localNodes) {

      std::vector<ActionDescription> actions;
      
      (*node.second("_system").children().begin()->second)("journalSize") =
        v.slice();
      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, actions);

      REQUIRE(actions.size() == 1);
      for (auto const& action : actions) {
        REQUIRE(action.name() == "UpdateCollection");
      }
        
    }
  }
  
  // Plan also now has db3 =====================================================
  SECTION(
    "Empty db2 in plan should drop all local db2 collections on all servers") {

    plan(PLAN_COL_PATH + "db2") =
      arangodb::basics::VelocyPackHelper::EmptyObjectValue();

    for (auto& node : localNodes) {
      
      std::vector<ActionDescription> actions;
      node.second("db2") = node.second("_system");
      
      arangodb::maintenance::diffPlanLocal(
        plan.toBuilder().slice(), node.second.toBuilder().slice(),
        node.first, actions);

      REQUIRE(actions.size() == node.second("db2").children().size());
      for (auto const& action : actions) {
        REQUIRE(action.name() == "DropCollection");
      }
      
    }
    
  }

  // Local has databases _system and db2 =====================================
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
  SECTION("Indexes missing in local") {

    plan(PLAN_COL_PATH  + "_system/1010021/indexes") =
      arangodb::basics::VelocyPackHelper::EmptyArrayValue();

    for (auto& node : localNodes) {

      if (node.second.has("_system/s1010022/")) {

        std::vector<ActionDescription> actions;

        arangodb::maintenance::diffPlanLocal (
          plan.toBuilder().slice(), node.second.toBuilder().slice(), node.first,
          actions);

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


} 
