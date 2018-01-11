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

#include <iostream>

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

  std::vector<Node> localNodes {
    createNode(dbs1Str),  createNode(dbs2Str),  createNode(dbs3Str)};
  
  // Plan and local in sync
  SECTION("Identical lists") {

    std::vector<ActionDescription> actions;
    
    arangodb::maintenance::diffPlanLocal(
      plan.toBuilder().slice(), localNodes[0].toBuilder().slice(),
      plan("/arango/Plan/DBServers").children().begin()->first, actions);

    REQUIRE(actions.size() == 0);

  }

  // Local additionally has db2 ================================================
  SECTION("Local databases one more") {

    std::vector<ActionDescription> actions;
    localNodes[0]("db2") =
      arangodb::basics::VelocyPackHelper::EmptyObjectValue();
    
    arangodb::maintenance::diffPlanLocal(
      plan.toBuilder().slice(), localNodes[0].toBuilder().slice(),
      plan("/arango/Plan/DBServers").children().begin()->first, actions);

    REQUIRE(actions.size() == 1);
    REQUIRE(actions.front().name() == "DropDatabase");
    REQUIRE(actions.front().get("database") == "db2");

  }


  // Plan also now has db2 =====================================================
  SECTION("Add db2 to plan should have no actions again") {

    std::vector<ActionDescription> actions;
    localNodes[0]("db2") =
      arangodb::basics::VelocyPackHelper::EmptyObjectValue();
    plan("/arango/Plan/Collections/db2") =
      arangodb::basics::VelocyPackHelper::EmptyObjectValue();
    
    arangodb::maintenance::diffPlanLocal(
      plan.toBuilder().slice(), localNodes[0].toBuilder().slice(),
      plan("/arango/Plan/DBServers").children().begin()->first, actions);

    REQUIRE(actions.size() == 0);

  }

  // Plan also now has db3 =====================================================
  SECTION("Add db2 to plan should have no actions again") {

    std::vector<ActionDescription> actions;
    plan("/arango/Plan/Collections/db2") =
      plan("/arango/Plan/Collections/_system");
    localNodes[0]("db2") =
      arangodb::basics::VelocyPackHelper::EmptyObjectValue();

    arangodb::maintenance::diffPlanLocal(
      plan.toBuilder().slice(), localNodes[0].toBuilder().slice(),
      plan("/arango/Plan/DBServers").children().begin()->first, actions);

    REQUIRE(actions.size() == 17);
    for (auto const& action : actions) {
      REQUIRE(action.name() == "CreateCollection");
    }

  }
  
  // Plan also now has db3 =====================================================
  SECTION("Add db2 to plan should have no actions again") {

    std::vector<ActionDescription> actions;
    plan("/arango/Plan/Collections/db2") =
      plan("/arango/Plan/Collections/_system");
    localNodes[0]("db2") = localNodes[0]("_system");

    arangodb::maintenance::diffPlanLocal(
      plan.toBuilder().slice(), localNodes[0].toBuilder().slice(),
      plan("/arango/Plan/DBServers").children().begin()->first, actions);

    REQUIRE(actions.size() == 17);
    for (auto const& action : actions) {
      REQUIRE(action.name() == "DropCollection");
    }

  }
  
  // Local has databases _system and db2 =====================================
  SECTION("Local collections") {
    size_t i = 0;
    
    for (auto const& dbServer : plan("/arango/Plan/DBServers").children()) {
      std::vector<ActionDescription> actions;
      arangodb::maintenance::diffPlanLocal (
        plan.toBuilder().slice(), localNodes[i++].toBuilder().slice(), dbServer.first,
        actions);
      REQUIRE(actions.size() == 0);
    } 
    
  } 
  
}

