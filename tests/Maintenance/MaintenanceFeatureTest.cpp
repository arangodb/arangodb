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

#include "Basics/Result.h"
#include "Cluster/MaintenanceAction.h"
#include "Cluster/MaintenanceFeature.h"


//
// TestFeature wraps MaintenanceFeature to all test specific action objects
//  by overriding the actionFactory() virtual function
//
class TestMaintenanceFeature : public arangodb::MaintenanceFeature {
public:
  TestMaintenanceFeature() {
//    prepare();
//    start();  ... threads die due to lack of ApplicationServer instance
  };

  virtual ~TestMaintenanceFeature() {
//    beginShutdown();
//    stop();
//    unprepare();
};
#if 1
  virtual arangodb::maintenance::MaintenanceActionPtr_t actionFactory(std::string & name,
                                                                      std::shared_ptr<arangodb::maintenance::ActionDescription_t> const & description,
                                                            std::shared_ptr<VPackBuilder> const & properties) override;

#endif
  arangodb::maintenance::MaintenanceActionPtr_t _recentAction;

};// TestMaintenanceFeature


//
// TestActionBasic simulates a multistep action by counting down
//  on each call to first() and next() until iteration counter is zero.
//  Returns false upon reaching zero
//
class TestActionBasic : public arangodb::maintenance::MaintenanceAction {
public:
    TestActionBasic(arangodb::MaintenanceFeature & feature,
                    std::shared_ptr<arangodb::maintenance::ActionDescription_t> const & description,
                    std::shared_ptr<VPackBuilder> const & properties)
      : MaintenanceAction(feature, description, properties), _iteration(1)
  {
    auto des_it = description->find("iterate_count");

    if (description->end() != des_it) {
      _iteration = atol(des_it->second.c_str());
      // safety check
      if (_iteration < 1) {
        _iteration = 1;
      } // if
    } // if
  };

  virtual ~TestActionBasic() {};

  bool first() override {return(0 < --_iteration);};

  bool next() override {return(0 < --_iteration);};

  int _iteration;

};// TestActionBasic

#if 1
arangodb::maintenance::MaintenanceActionPtr_t TestMaintenanceFeature::actionFactory(std::string & name,
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

} // TestMaintenanceFeature::actionFactory
#endif

TEST_CASE("MaintenanceFeature", "[cluster][maintenance][devel]") {

  SECTION("Basic action test") {
    TestMaintenanceFeature tf;
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","2"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf.addAction(desc_ptr, prop_ptr, true);

    REQUIRE(result.ok());
  }


  SECTION("Local databases one more empty database should be dropped") {


  }
}
