////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for ClusterComm
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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"

//#include <map>

#include <velocypack/vpack.h>
#include <velocypack/velocypack-aliases.h>

#include "Cluster/MaintenanceAction.h"
#include "Cluster/MaintenanceFeature.h"


class TestFeature : public arangodb::MaintenanceFeature {
public:
  TestFeature() {};

  virtual ~TestFeature() {};

  virtual arangodb::maintenance::MaintenanceActionPtr_t actionFactory(std::string name,
                                                                      std::shared_ptr<arangodb::maintenance::ActionDescription_t> const & description,
                                                            std::shared_ptr<VPackBuilder> const & properties) override;


  arangodb::maintenance::MaintenanceActionPtr_t _recentAction;

};// TestFeature


class TestActionBasic : public arangodb::maintenance::MaintenanceAction {
public:
    TestActionBasic(arangodb::MaintenanceFeature & feature,
                    std::shared_ptr<arangodb::maintenance::ActionDescription_t> const & description,
                    std::shared_ptr<VPackBuilder> const & properties) : MaintenanceAction(feature, description, properties) {};

  virtual ~TestActionBasic() {};

  bool first() {return false;};
};// TestActionBasic


arangodb::maintenance::MaintenanceActionPtr_t TestFeature::actionFactory(std::string name,
                                                                         std::shared_ptr<arangodb::maintenance::ActionDescription_t> const & description,
                                                               std::shared_ptr<VPackBuilder> const & properties) {
  arangodb::maintenance::MaintenanceActionPtr_t newAction;

    // walk list until we find the object of our desire
  if (name == "TestActionBasic") {
    newAction.reset(new TestActionBasic(*this, description, properties));
  }

  // make test access to this new action easy ... and prevent its deletion to soon
  _recentAction = newAction;

  return newAction;

} // TestFeature::actionFactory


TEST_CASE("MaintenanceFeature", "[cluster][maintenance][devel]") {

  SECTION("Basic action test") {
    TestFeature tf;


  }


  SECTION("Local databases one more empty database should be dropped") {


  }
}
