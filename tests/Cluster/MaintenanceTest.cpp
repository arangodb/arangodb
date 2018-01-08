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

const char *agency =
#include "MaintenanceTest.json"
;

VPackBuilder test2, test1;


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

Node createRootNode() {
  return createNode(agency);
}

TEST_CASE("Maintenance", "[cluster][maintenance][differencePlanLocal]") {

  auto baseStructure = createRootNode();

  Builder builder;
  baseStructure.toBuilder(builder);

  { VPackObjectBuilder b(&test2);
    test2.add("id",VPackValue("3"));
    test2.add("name",VPackValue("test2")); }
  
  { VPackObjectBuilder b(&test1);
    test1.add("id",VPackValue("2"));
    test1.add("name",VPackValue("test1")); } 

  // Plan and local in sync
  SECTION("Identical lists") {
    std::vector<std::string> local {"_system"}, toCreate, toDrop;
    Node plan = baseStructure(PLANNED_DATABASES);
    
    arangodb::maintenance::diffPlanLocalForDatabases(
      plan, local, toCreate, toDrop);

    REQUIRE(toCreate.size() == 0);
    REQUIRE(toDrop.size() == 0);
  }


  // Local has databases _system and test1 =====================================
  SECTION("Local databases one more") {
    std::vector<std::string> local{"_system","test1"}, toCreate, toDrop;
    Node plan = baseStructure(PLANNED_DATABASES);

    arangodb::maintenance::diffPlanLocalForDatabases(
      plan, local, toCreate, toDrop);

    REQUIRE(toCreate.size() == 0);
    
    REQUIRE(toDrop.size() == 1);
    REQUIRE(toDrop.front() == "test1");

    local.pop_back();
  }


  // Plan has databases _system and test1 ======================================
  SECTION("Plan has one more than local") {
    std::vector<std::string> local{"_system"}, toCreate, toDrop;

    Node plan = baseStructure(PLANNED_DATABASES);
    plan("test1") = test1.slice();

    arangodb::maintenance::diffPlanLocalForDatabases(
      plan, local, toCreate, toDrop);

    REQUIRE(toCreate.size() == 1);
    REQUIRE(toCreate.front() == "test1");
    
    REQUIRE(toDrop.size() == 0);
  }


  // Local has databases _system and test1 =====================================
  // Plan has databases _system and test2
  SECTION("Plan has one more than local and local has one more than plan") {
    std::vector<std::string> local {"_system", "test1"}, toCreate, toDrop;
    Node plan = baseStructure(PLANNED_DATABASES);
    plan("test2") = test2.slice();

    arangodb::maintenance::diffPlanLocalForDatabases(
      plan, local, toCreate, toDrop);

    REQUIRE(toCreate.size() == 1);
    REQUIRE(toCreate.front() == "test2");
    
    REQUIRE(toDrop.size() == 1);
    REQUIRE(toDrop.front() == "test1");
  }


  // Check executePlanForDatabase ==============================================
  SECTION("Execute plan for database") {
    arangodb::Result executePlanForDatabases (
      arangodb::consensus::Node plan, arangodb::consensus::Node current,
      std::vector<std::string>);

    std::vector<std::string> local {"_system", "test1"}, toCreate, toDrop;
    Node plan = baseStructure(PLANNED_DATABASES);
    plan("test2") = test2.slice();

    arangodb::maintenance::executePlanForDatabases(plan, plan, local);
    REQUIRE(ActionRegistry::instance()->size() == 2);
  }
  
  
  // Check that not a new action is create for same difference =================
  SECTION("Execute plan for database") {
    arangodb::Result executePlanForDatabases (
      arangodb::consensus::Node plan, arangodb::consensus::Node current,
      std::vector<std::string>);

    std::vector<std::string> local {"_system", "test1"}, toCreate, toDrop;
    Node plan = baseStructure(PLANNED_DATABASES);
    plan("test2") = test2.slice();

    arangodb::maintenance::executePlanForDatabases(plan, plan, local);
    REQUIRE(ActionRegistry::instance()->size() == 2);

    std::cout << ActionRegistry::instance()->toVelocyPack().toJson() << std::endl;

    
  }
  
  
}

