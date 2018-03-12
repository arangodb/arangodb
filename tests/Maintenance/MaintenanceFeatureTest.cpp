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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Result.h"
#include "Cluster/MaintenanceAction.h"
#include "Cluster/MaintenanceFeature.h"

//
// TestProgressHandler lets us know once ApplicationServer is ready
//
class TestProgressHandler : public arangodb::application_features::ProgressHandler {
public:
  TestProgressHandler() {
    _serverReady=false;

    using std::placeholders::_1;
    _state = std::bind(& TestProgressHandler::StateChange, this, _1);

    using std::placeholders::_2;
    _feature = std::bind(& TestProgressHandler::FeatureChange, this, _1, _2);
  }


  void StateChange(arangodb::application_features::ServerState newState) {
    if (arangodb::application_features::ServerState::IN_WAIT == newState) {
      CONDITION_LOCKER(clock, _serverReadyCond);
      _serverReady = true;
      _serverReadyCond.broadcast();
    }
  }

  void FeatureChange(arangodb::application_features::ServerState newState, std::string const &) {
  }

  arangodb::basics::ConditionVariable _serverReadyCond;
  std::atomic_bool _serverReady;

};// class TestProgressHandler


//
// TestFeature wraps MaintenanceFeature to all test specific action objects
//  by overriding the actionFactory() virtual function.  Two versions:
//  1. default constructor for non-threaded actions
//  2. constructor with ApplicationServer pointer for threaded actions
//
class TestMaintenanceFeature : public arangodb::MaintenanceFeature {
public:
  TestMaintenanceFeature() {

  };

  TestMaintenanceFeature(arangodb::application_features::ApplicationServer * as)
    : arangodb::MaintenanceFeature(as) {

    // begin with no threads to allow queue validation
    _maintenanceThreadsMax = 0;
    as->addReporter(_progressHandler);
  };

  virtual ~TestMaintenanceFeature() {

  };

  virtual arangodb::maintenance::MaintenanceActionPtr_t actionFactory(std::string & name,
                                                                      std::shared_ptr<arangodb::maintenance::ActionDescription_t> const & description,
                                                            std::shared_ptr<VPackBuilder> const & properties) override;

  void setSecondsActionsBlock(uint32_t seconds) {_secondsActionsBlock = seconds;};

  /// @brief set thread count, then activate the threads via start().  One time usage only.
  void setMaintenanceThreadsMax(uint32_t threads) {_maintenanceThreadsMax = threads; start(); }

public:
  arangodb::maintenance::MaintenanceActionPtr_t _recentAction;
  TestProgressHandler _progressHandler;

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

  bool first() override {
    // time to set result?
    if (0==_iteration) {
      _result.reset(_resultCode);
    } // if

    // verify first() called once
    if (0!=getProgress()) {
      _result.reset(2);
    } // if

    return(0 < _iteration-- && _result.ok());
  };

  bool next() override {
    // time to set result?
    if (0==_iteration) {
      _result.reset(_resultCode);
    } // if

    // verify next() called properly
    if (0==getProgress()) {
      _result.reset(2);
    } // if

    return(0 < _iteration-- && _result.ok());
  };

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


TEST_CASE("MaintenanceFeatureUnthreaded", "[cluster][maintenance][devel]") {

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
} // MaintenanceFeatureUnthreaded

TEST_CASE("MaintenanceFeatureThreaded", "[cluster][maintenance][devel]") {

  // structure used to store expected states of action properties
  struct Expected {
    int _id;
    int _result;
    int _state;
    int _progress;
  };

  SECTION("Populate action queue and validate") {
    std::vector<Expected> pre_thread, post_thread;

    std::shared_ptr<arangodb::options::ProgramOptions> po = std::make_shared<arangodb::options::ProgramOptions>("test", std::string(), std::string(), "path");
    arangodb::application_features::ApplicationServer as(po, nullptr);
    TestMaintenanceFeature * tf = new TestMaintenanceFeature(&as);
    as.addFeature(tf);
    std::thread th(&arangodb::application_features::ApplicationServer::run, &as, 0, nullptr);

    //
    // 1. load up the queue without threads running
    //   a. 100 iterations then fail
    arangodb::maintenance::ActionDescription_t desc={{"name","TestActionBasic"},{"iterate_count","100"},
                                                     {"result_code","1"}};
    auto desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    auto prop_ptr = std::make_shared<VPackBuilder>();

    arangodb::Result result = tf->addAction(desc_ptr, prop_ptr, false);
    REQUIRE(result.ok());   // has not executed, ok() is about parse and list add
    REQUIRE(tf->_recentAction->result().ok());
    pre_thread.push_back({1,0,1,0});
    post_thread.push_back({1,1,6,100});

    //   b. 2 iterations then succeed;
    desc={{"name","TestActionBasic"},{"iterate_count","2"}};
    desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    prop_ptr = std::make_shared<VPackBuilder>();

    result = tf->addAction(desc_ptr, prop_ptr, false);
    REQUIRE(result.ok());   // has not executed, ok() is about parse and list add
    REQUIRE(tf->_recentAction->result().ok());
    pre_thread.push_back({2,0,1,0});
    post_thread.push_back({2,0,5,2});

    //   c. duplicate of 'a', should fail to add
    desc={{"name","TestActionBasic"},{"iterate_count","100"},
                                                     {"result_code","1"}};
    desc_ptr = std::make_shared<arangodb::maintenance::ActionDescription_t>(desc);
    prop_ptr = std::make_shared<VPackBuilder>();

    result = tf->addAction(desc_ptr, prop_ptr, false);
    REQUIRE(!result.ok());   // has not executed, ok() is about parse and list add
    // _recentAction will NOT contain the aborted object ... don't test it

    //
    // 2. see if happy about queue prior to threads running
    {
      VPackArrayIterator ai1(tf->toVelocityPack().slice());
      REQUIRE(2 == ai1.size());
      REQUIRE(ai1.size() == pre_thread.size());

      auto ma = ai1.begin();
      auto check = pre_thread.begin();

      for ( ; ai1.end() != ma && pre_thread.end()!=check; ++ma, ++check) {
        VPackSlice id = (*ma).get("id");
        REQUIRE((id.isInteger() && id.getInt() == check->_id));

        VPackSlice result = (*ma).get("result");
        REQUIRE((result.isInteger() && check->_result == result.getInt()));

        VPackSlice state = (*ma).get("state");
        REQUIRE((state.isInteger() && check->_state == state.getInt()));

        VPackSlice progress = (*ma).get("progress");
        REQUIRE((progress.isInteger() && check->_progress == progress.getInt()));
      } // for
    }

    //
    // 3. start threads AFTER ApplicationServer known to be running
    {
      CONDITION_LOCKER(clock, tf->_progressHandler._serverReadyCond);
      while(!tf->_progressHandler._serverReady) {
        tf->_progressHandler._serverReadyCond.wait();
      } // while
    }

    tf->setMaintenanceThreadsMax(1);

    //
    // 4. loop while waiting for threads to complete all actions
    bool again;

    do {
      again = false;
      sleep(1);
      VPackArrayIterator ai1(tf->toVelocityPack().slice());
      for (auto ma : ai1 )
      {
        VPackSlice state = ma.get("state");
        again = again || (6 != state.getInt() && 5 != state.getInt());
      } // for
    } while(again);

    //
    // 5. verify completed actions
    {
      VPackArrayIterator ai1(tf->toVelocityPack().slice());
      REQUIRE(2 == ai1.size());
      REQUIRE(ai1.size() == post_thread.size());

      auto ma = ai1.begin();
      auto check = post_thread.begin();

      for ( ; ai1.end() != ma && post_thread.end()!=check; ++ma, ++check) {
        VPackSlice id = (*ma).get("id");
        REQUIRE((id.isInteger() && id.getInt() == check->_id));

        VPackSlice result = (*ma).get("result");
        REQUIRE((result.isInteger() && check->_result == result.getInt()));

        VPackSlice state = (*ma).get("state");
        REQUIRE((state.isInteger() && check->_state == state.getInt()));

        VPackSlice progress = (*ma).get("progress");
        REQUIRE((progress.isInteger() && check->_progress == progress.getInt()));
      } // for
    }

#if 0   // for debugging
    std::string xx = (tf->toVelocityPack()).toJson();
    printf("%s\n", xx.c_str());
#endif

    //
    // 6. bring down the ApplicationServer, i.e. clean up
    as.beginShutdown();
    th.join();
  }
} // MaintenanceFeatureThreaded
