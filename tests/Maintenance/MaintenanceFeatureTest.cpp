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

  virtual arangodb::maintenance::MaintenanceActionPtr_t actionFactory(std::string & name,
                                                                      std::shared_ptr<arangodb::maintenance::ActionDescription_t> const & description,
                                                            std::shared_ptr<VPackBuilder> const & properties) override;

  void setSecondsActionsBlock(uint32_t seconds) {_secondsActionsBlock = seconds;};

public:
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
      : MaintenanceAction(feature, description, properties), _iteration(1), _resultCode(0)
  {
    auto des_it = description->find("iterate_count");

    if (description->end() != des_it) {
      _iteration = atol(des_it->second.c_str());
      // safety check
      if (_iteration < 0) {
        _iteration = 1;
      } // if
    } // if

    auto res_it = description->find("result_code");

    if (description->end() != res_it) {
      _resultCode = atol(res_it->second.c_str());
    } // if

  };

  virtual ~TestActionBasic() {};

  bool first() override {if (0==_iteration) _result.reset(_resultCode); return(0 < _iteration--);};

  bool next() override {if (0==_iteration) _result.reset(_resultCode); return(0 < _iteration--);};

  int _iteration;
  int _resultCode;

};// TestActionBasic


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


TEST_CASE("MaintenanceFeature", "[cluster][maintenance][devel]") {

  SECTION("Iterate Action 0 times - ok") {
    TestMaintenanceFeature tf;
    tf.setSecondsActionsBlock(0);  // disable retry wait for now
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","0"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf.addAction(desc_ptr, prop_ptr, true);

    REQUIRE(result.ok());
    REQUIRE(tf._recentAction->result().ok());
    REQUIRE(0==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == arangodb::maintenance::MaintenanceAction::COMPLETE);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());
  }

  SECTION("Iterate Action 0 times - fail") {
    TestMaintenanceFeature tf;
    tf.setSecondsActionsBlock(0);  // disable retry wait for now
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","0"},
                                                     {"result_code","1"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf.addAction(desc_ptr, prop_ptr, true);

    REQUIRE(!result.ok());
    REQUIRE(!tf._recentAction->result().ok());
    REQUIRE(0==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == arangodb::maintenance::MaintenanceAction::FAILED);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());
  }

  SECTION("Iterate Action 1 time - ok") {
    TestMaintenanceFeature tf;
    tf.setSecondsActionsBlock(0);  // disable retry wait for now
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","1"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf.addAction(desc_ptr, prop_ptr, true);

    REQUIRE(result.ok());
    REQUIRE(tf._recentAction->result().ok());
    REQUIRE(1==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == arangodb::maintenance::MaintenanceAction::COMPLETE);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());
  }

  SECTION("Iterate Action 1 time - fail") {
    TestMaintenanceFeature tf;
    tf.setSecondsActionsBlock(0);  // disable retry wait for now
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","1"},
                                                     {"result_code","1"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf.addAction(desc_ptr, prop_ptr, true);

    REQUIRE(!result.ok());
    REQUIRE(!tf._recentAction->result().ok());
    REQUIRE(1==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == arangodb::maintenance::MaintenanceAction::FAILED);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());
  }

  SECTION("Iterate Action 2 times - ok") {
    TestMaintenanceFeature tf;
    tf.setSecondsActionsBlock(0);  // disable retry wait for now
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","2"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf.addAction(desc_ptr, prop_ptr, true);

    REQUIRE(result.ok());
    REQUIRE(tf._recentAction->result().ok());
    REQUIRE(2==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == arangodb::maintenance::MaintenanceAction::COMPLETE);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());
  }

  SECTION("Iterate Action 100 times - ok") {
    TestMaintenanceFeature tf;
    tf.setSecondsActionsBlock(0);  // disable retry wait for now
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","100"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf.addAction(desc_ptr, prop_ptr, true);

    REQUIRE(result.ok());
    REQUIRE(tf._recentAction->result().ok());
    REQUIRE(100==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == arangodb::maintenance::MaintenanceAction::COMPLETE);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());
  }

  SECTION("Iterate Action 100 times - fail") {
    TestMaintenanceFeature tf;
    tf.setSecondsActionsBlock(0);  // disable retry wait for now
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","100"},
                                                     {"result_code","1"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf.addAction(desc_ptr, prop_ptr, true);

    REQUIRE(!result.ok());
    REQUIRE(!tf._recentAction->result().ok());
    REQUIRE(100==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == arangodb::maintenance::MaintenanceAction::FAILED);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());
  }


  SECTION("Local databases one more empty database should be dropped") {


  }
}
