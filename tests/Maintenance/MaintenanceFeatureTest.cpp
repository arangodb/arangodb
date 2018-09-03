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

#include <iostream>

#include <velocypack/vpack.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Result.h"
#include "Cluster/Action.h"
#include "Cluster/MaintenanceFeature.h"

//
// structure used to store expected states of action properties
//
struct Expected {
  int _id;
  int _result;
  int _state;
  int _progress;
};

typedef std::vector<Expected> ExpectedVec_t;

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


using namespace arangodb::maintenance;

//
// TestFeature wraps MaintenanceFeature to all test specific action objects
//  by overriding the actionFactory() virtual function.  Two versions:
//  1. default constructor for non-threaded actions
//  2. constructor with ApplicationServer pointer for threaded actions
//
class TestMaintenanceFeature : public arangodb::MaintenanceFeature {
public:
  TestMaintenanceFeature(arangodb::application_features::ApplicationServer& as)
    : arangodb::MaintenanceFeature(as) {

    // force activation of the feature, even in agency/single-server mode
    // (the catch tests use single-server mode)
    _forceActivation = true;

    // begin with no threads to allow queue validation
    _maintenanceThreadsMax = 0;
    as.addReporter(_progressHandler);
  }

  virtual ~TestMaintenanceFeature() {}

  void validateOptions(std::shared_ptr<arangodb::options::ProgramOptions> options) override {}

  void setSecondsActionsBlock(uint32_t seconds) { _secondsActionsBlock = seconds; }

  /// @brief set thread count, then activate the threads via start().  One time usage only.
  ///   Code waits until background ApplicationServer known to have fully started.
  void setMaintenanceThreadsMax(uint32_t threads) {
    CONDITION_LOCKER(clock, _progressHandler._serverReadyCond);
    while(!_progressHandler._serverReady) {
      _progressHandler._serverReadyCond.wait();
    } // while

    _maintenanceThreadsMax = threads;
    start();
  } // setMaintenanceThreadsMax


  virtual arangodb::Result addAction(
    std::shared_ptr<arangodb::maintenance::Action> action, bool executeNow = false) override {
    _recentAction = action;
    return MaintenanceFeature::addAction(action, executeNow);
  }

  virtual arangodb::Result addAction(
    std::shared_ptr<arangodb::maintenance::ActionDescription> const & description,
    bool executeNow = false) override {
    return MaintenanceFeature::addAction(description, executeNow);
  }


  bool verifyRegistryState(ExpectedVec_t & expected) {
    bool good(true);

    VPackBuilder registryBuilder(toVelocyPack());
    VPackArrayIterator registry(registryBuilder.slice());

    auto action = registry.begin();
    auto check = expected.begin();

    for ( ; registry.end() != action && expected.end()!=check; ++action, ++check) {
      VPackSlice id = (*action).get("id");
      if (!(id.isInteger() && id.getInt() == check->_id)) {
        std::cerr << "Id mismatch: action has " << id.getInt()
                  << " expected " << check->_id << std::endl;
        good = false;
      } // if

      VPackSlice result = (*action).get("result");
      if (!(result.isInteger() && check->_result == result.getInt())) {
        std::cerr << "Result mismatch: action has " << result.getInt()
                  << " expected " << check->_result << std::endl;
        good = false;
      } // if

      VPackSlice state = (*action).get("state");
      if (!(state.isInteger() && check->_state == state.getInt())) {
        std::cerr << "State mismatch: action has " << state.getInt()
                  << " expected " << check->_state << std::endl;
        good = false;
      } // if

      VPackSlice progress = (*action).get("progress");
      if (!(progress.isInteger() && check->_progress == progress.getInt())) {
        std::cerr << "Progress mismatch: action has " << progress.getInt()
                  << " expected " << check->_progress << std::endl;
        good = false;
      } // if
    } // for

    return good;

  } // verifyRegistryState


  /// @brief poll registry until all actions finish
  void waitRegistryComplete() {
    bool again;

    do {
      again = false;
      std::this_thread::sleep_for(std::chrono::seconds(1));

      VPackBuilder registryBuilder(toVelocyPack());
      VPackArrayIterator registry(registryBuilder.slice());
      for (auto action : registry) {
        VPackSlice state = action.get("state");
        again = again || (COMPLETE != state.getInt() && FAILED != state.getInt());
      } // for
    } while (again);
  } // waitRegistryComplete

public:
  std::shared_ptr<Action> _recentAction;
  TestProgressHandler _progressHandler;

};// TestMaintenanceFeature


//
// TestActionBasic simulates a multistep action by counting down
//  on each call to first() and next() until iteration counter is zero.
//  Returns false upon reaching zero
//
class TestActionBasic : public ActionBase {
public:
  TestActionBasic(
    arangodb::MaintenanceFeature& feature, ActionDescription const& description)
    : ActionBase(feature, description), _iteration(1), _resultCode(0) {

    std::string value, iterate_count;
    auto gres = description.get("iterate_count", iterate_count);

    if (gres.ok()) {
      _iteration = std::atol(iterate_count.c_str());
      // safety check
      if (_iteration < 0) {
        _iteration = 1;
      } // if
    } // if

    if (description.get(FAST_TRACK, value).ok()) {
      _labels.emplace(FAST_TRACK);
    }

    if (description.get("result_code", value).ok()) {
      _resultCode = std::atol(value.c_str());
    } // if

    if (description.get("preaction_result_code", value).ok()) {
      std::map<std::string, std::string> pred {
        {"name","TestActionBasic"}, {"result_code",value}};
      if (gres.ok()) {
        pred.insert({"iterate_count",iterate_count});
      }
      _preAction = std::make_shared<ActionDescription>(pred);
    } // if


    if (description.get("postaction_result_code", value).ok()) {
      std::map<std::string, std::string> postd {
        {"name","TestActionBasic"}, {"result_code",value}};
      if (gres.ok()) {
        postd.insert({"iterate_count",iterate_count});
      }
      _postAction = std::make_shared<ActionDescription>(postd);
    } // if
  };

  virtual ~TestActionBasic() {};

  bool first() override {

    // a pre action needs to push before setting _result
    if (_preDesc) {
      createPreAction(_preDesc);
    } else if (0==_iteration) {
      // time to set result?
      _result.reset(_resultCode);
    } // if

    // verify first() called once
    if (0!=getProgress()) {
      _result.reset(2);
    } // if

    return(iteratorEndTest());
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

    return(iteratorEndTest());
  };


protected:
  bool iteratorEndTest() {
    bool more;

    //
    if (_result.ok()) {
      more = 0 < _iteration--;

      // if going to stop, see if a postAction is needed
      if (!more && _postDesc) {
        createPostAction(_postDesc);
      } // if
    } else {
      // !ok() ... always stop iteration
      more = false;
    }
    return more;
  } // iteratorEndTest

public:
  int _iteration;
  int _resultCode;
  std::shared_ptr<ActionDescription> _preDesc;
  std::shared_ptr<ActionDescription> _postDesc;

};// TestActionBasic

//
//
// Unit Tests start here
//
//
TEST_CASE("MaintenanceFeatureUnthreaded", "[cluster][maintenance][devel]") {

  std::chrono::system_clock::time_point baseTime(std::chrono::system_clock::now());
  std::chrono::system_clock::time_point noTime;

  SECTION("Iterate Action 0 times - ok") {
    std::shared_ptr<arangodb::options::ProgramOptions> po =
      std::make_shared<arangodb::options::ProgramOptions>(
        "test", std::string(), std::string(), "path");
    arangodb::application_features::ApplicationServer as(po, nullptr);

    TestMaintenanceFeature tf(as);
    tf.setSecondsActionsBlock(0);  // disable retry wait for now

    std::unique_ptr<ActionBase> action_base_ptr;
    action_base_ptr.reset(
      (ActionBase*) new TestActionBasic(
        tf, ActionDescription(std::map<std::string,std::string>{
            {"name","TestActionBasic"},{"iterate_count","0"}})));
    arangodb::Result result = tf.addAction(
      std::make_shared<Action>(std::move(action_base_ptr)), true);

    REQUIRE(result.ok());
    REQUIRE(tf._recentAction->result().ok());
    REQUIRE(0==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == COMPLETE);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());

    REQUIRE(baseTime <= tf._recentAction->getCreateTime());
    REQUIRE(baseTime <= tf._recentAction->getStartTime());
    REQUIRE(baseTime <= tf._recentAction->getDoneTime());
    REQUIRE(noTime == tf._recentAction->getLastStatTime());
    REQUIRE(tf._recentAction->getCreateTime() <= tf._recentAction->getStartTime());
    REQUIRE(tf._recentAction->getStartTime() <= tf._recentAction->getDoneTime());

  }

  SECTION("Iterate Action 0 times - fail") {
    std::shared_ptr<arangodb::options::ProgramOptions> po =
      std::make_shared<arangodb::options::ProgramOptions>(
        "test", std::string(), std::string(), "path");
    arangodb::application_features::ApplicationServer as(po, nullptr);
    TestMaintenanceFeature tf(as);
    tf.setSecondsActionsBlock(0);  // disable retry wait for now

    std::unique_ptr<ActionBase> action_base_ptr;
    action_base_ptr.reset(
      (ActionBase*) new TestActionBasic(
        tf, ActionDescription(std::map<std::string,std::string>{
            {"name","TestActionBasic"},{"iterate_count","0"},{"result_code","1"}
          })));
    arangodb::Result result = tf.addAction(
      std::make_shared<Action>(std::move(action_base_ptr)), true);

    REQUIRE(!result.ok());
    REQUIRE(!tf._recentAction->result().ok());
    REQUIRE(0==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == FAILED);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());

    REQUIRE(baseTime <= tf._recentAction->getCreateTime());
    REQUIRE(baseTime <= tf._recentAction->getStartTime());
    REQUIRE(baseTime <= tf._recentAction->getDoneTime());
    REQUIRE(noTime == tf._recentAction->getLastStatTime());
    REQUIRE(tf._recentAction->getCreateTime() <= tf._recentAction->getStartTime());
    REQUIRE(tf._recentAction->getStartTime() <= tf._recentAction->getDoneTime());
}

  SECTION("Iterate Action 1 time - ok") {
    std::shared_ptr<arangodb::options::ProgramOptions> po =
      std::make_shared<arangodb::options::ProgramOptions>(
        "test", std::string(), std::string(), "path");
    arangodb::application_features::ApplicationServer as(po, nullptr);
    TestMaintenanceFeature tf(as);
    tf.setSecondsActionsBlock(0);  // disable retry wait for now

    std::unique_ptr<ActionBase> action_base_ptr;
    action_base_ptr.reset(
      (ActionBase*) new TestActionBasic(
        tf, ActionDescription(std::map<std::string,std::string>{
            {"name","TestActionBasic"},{"iterate_count","1"}})));
    arangodb::Result result = tf.addAction(
      std::make_shared<Action>(std::move(action_base_ptr)), true);

    REQUIRE(result.ok());
    REQUIRE(tf._recentAction->result().ok());
    REQUIRE(1==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == COMPLETE);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());

    REQUIRE(baseTime <= tf._recentAction->getCreateTime());
    REQUIRE(baseTime <= tf._recentAction->getStartTime());
    REQUIRE(baseTime <= tf._recentAction->getDoneTime());
    REQUIRE(baseTime <= tf._recentAction->getLastStatTime());
    REQUIRE(tf._recentAction->getCreateTime() <= tf._recentAction->getStartTime());
    REQUIRE(tf._recentAction->getStartTime() <= tf._recentAction->getDoneTime());
}

  SECTION("Iterate Action 1 time - fail") {
    std::shared_ptr<arangodb::options::ProgramOptions> po =
      std::make_shared<arangodb::options::ProgramOptions>(
        "test", std::string(), std::string(), "path");
    arangodb::application_features::ApplicationServer as(po, nullptr);
    TestMaintenanceFeature tf(as);
    tf.setSecondsActionsBlock(0);  // disable retry wait for now

    std::unique_ptr<ActionBase> action_base_ptr;
    action_base_ptr.reset(
      (ActionBase*) new TestActionBasic(
        tf, ActionDescription(std::map<std::string,std::string>{
            {"name","TestActionBasic"},{"iterate_count","1"},{"result_code","1"}
          })));
    arangodb::Result result = tf.addAction(
      std::make_shared<Action>(std::move(action_base_ptr)), true);

    REQUIRE(!result.ok());
    REQUIRE(!tf._recentAction->result().ok());
    REQUIRE(1==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == FAILED);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());

    REQUIRE(baseTime <= tf._recentAction->getCreateTime());
    REQUIRE(baseTime <= tf._recentAction->getStartTime());
    REQUIRE(baseTime <= tf._recentAction->getDoneTime());
    REQUIRE(baseTime <= tf._recentAction->getLastStatTime());
    REQUIRE(tf._recentAction->getCreateTime() <= tf._recentAction->getStartTime());
    REQUIRE(tf._recentAction->getStartTime() <= tf._recentAction->getDoneTime());
    REQUIRE(tf._recentAction->getLastStatTime() <= tf._recentAction->getDoneTime());
  }

  SECTION("Iterate Action 2 times - ok") {
    std::shared_ptr<arangodb::options::ProgramOptions> po =
      std::make_shared<arangodb::options::ProgramOptions>(
        "test", std::string(), std::string(), "path");
    arangodb::application_features::ApplicationServer as(po, nullptr);
    TestMaintenanceFeature tf(as);
    tf.setSecondsActionsBlock(0);  // disable retry wait for now

    std::unique_ptr<ActionBase> action_base_ptr;
    action_base_ptr.reset(
      (ActionBase*) new TestActionBasic(
        tf, ActionDescription(std::map<std::string,std::string>{
            {"name","TestActionBasic"},{"iterate_count","2"}
          })));
    arangodb::Result result = tf.addAction(
      std::make_shared<Action>(std::move(action_base_ptr)), true);

    REQUIRE(result.ok());
    REQUIRE(tf._recentAction->result().ok());
    REQUIRE(2==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == COMPLETE);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());

    REQUIRE(baseTime <= tf._recentAction->getCreateTime());
    REQUIRE(baseTime <= tf._recentAction->getStartTime());
    REQUIRE(baseTime <= tf._recentAction->getDoneTime());
    REQUIRE(baseTime <= tf._recentAction->getLastStatTime());
    REQUIRE(tf._recentAction->getCreateTime() <= tf._recentAction->getStartTime());
    REQUIRE(tf._recentAction->getStartTime() <= tf._recentAction->getDoneTime());
    REQUIRE(tf._recentAction->getLastStatTime() <= tf._recentAction->getDoneTime());
  }

  SECTION("Iterate Action 100 times - ok") {
    std::shared_ptr<arangodb::options::ProgramOptions> po =
      std::make_shared<arangodb::options::ProgramOptions>(
        "test", std::string(), std::string(), "path");
    arangodb::application_features::ApplicationServer as(po, nullptr);
    TestMaintenanceFeature tf(as);
    tf.setSecondsActionsBlock(0);  // disable retry wait for now

    std::unique_ptr<ActionBase> action_base_ptr;
    action_base_ptr.reset(
      (ActionBase*) new TestActionBasic(
        tf, ActionDescription(std::map<std::string,std::string>{
            {"name","TestActionBasic"},{"iterate_count","100"}})));
    arangodb::Result result = tf.addAction(
      std::make_shared<Action>(std::move(action_base_ptr)), true);

    REQUIRE(result.ok());
    REQUIRE(tf._recentAction->result().ok());
    REQUIRE(100==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == COMPLETE);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());

    REQUIRE(baseTime <= tf._recentAction->getCreateTime());
    REQUIRE(baseTime <= tf._recentAction->getStartTime());
    REQUIRE(baseTime <= tf._recentAction->getDoneTime());
    REQUIRE(baseTime <= tf._recentAction->getLastStatTime());
    REQUIRE(tf._recentAction->getCreateTime() <= tf._recentAction->getStartTime());
    REQUIRE(tf._recentAction->getStartTime() <= tf._recentAction->getDoneTime());
    REQUIRE(tf._recentAction->getLastStatTime() <= tf._recentAction->getDoneTime());
  }

  SECTION("Iterate Action 100 times - fail") {
    std::shared_ptr<arangodb::options::ProgramOptions> po =
      std::make_shared<arangodb::options::ProgramOptions>(
        "test", std::string(), std::string(), "path");
    arangodb::application_features::ApplicationServer as(po, nullptr);
    TestMaintenanceFeature tf(as);
    tf.setSecondsActionsBlock(0);  // disable retry wait for now

    std::unique_ptr<ActionBase> action_base_ptr;
    action_base_ptr.reset(
      (ActionBase*) new TestActionBasic(
        tf, ActionDescription(std::map<std::string,std::string>{
            {"name","TestActionBasic"},{"iterate_count","100"},{"result_code","1"}
          })));
    arangodb::Result result = tf.addAction(
      std::make_shared<Action>(std::move(action_base_ptr)), true);

    REQUIRE(!result.ok());
    REQUIRE(!tf._recentAction->result().ok());
    REQUIRE(100==tf._recentAction->getProgress());
    REQUIRE(tf._recentAction->getState() == FAILED);
    REQUIRE(tf._recentAction->done());
    REQUIRE(1==tf._recentAction->id());

    REQUIRE(baseTime <= tf._recentAction->getCreateTime());
    REQUIRE(baseTime <= tf._recentAction->getStartTime());
    REQUIRE(baseTime <= tf._recentAction->getDoneTime());
    REQUIRE(baseTime <= tf._recentAction->getLastStatTime());
    REQUIRE(tf._recentAction->getCreateTime() <= tf._recentAction->getStartTime());
    REQUIRE(tf._recentAction->getStartTime() <= tf._recentAction->getDoneTime());
    REQUIRE(tf._recentAction->getLastStatTime() <= tf._recentAction->getDoneTime());
  }
} // MaintenanceFeatureUnthreaded

TEST_CASE("MaintenanceFeatureThreaded", "[cluster][maintenance][devel]") {

  SECTION("Populate action queue and validate") {
    std::vector<Expected> pre_thread, post_thread;

    std::shared_ptr<arangodb::options::ProgramOptions> po =
      std::make_shared<arangodb::options::ProgramOptions>(
        "test", std::string(), std::string(), "path");
    arangodb::application_features::ApplicationServer as(po, nullptr);
    TestMaintenanceFeature * tf = new TestMaintenanceFeature(as);
    as.addFeature(tf);
    std::thread th(
      &arangodb::application_features::ApplicationServer::run, &as, 0, nullptr);

    //
    // 1. load up the queue without threads running
    //   a. 100 iterations then fail

    std::unique_ptr<ActionBase> action_base_ptr;
    action_base_ptr.reset(
      (ActionBase*) new TestActionBasic(
        *tf, ActionDescription(std::map<std::string,std::string>{
            {"name","TestActionBasic"},{"iterate_count","100"},{"result_code","1"}
          })));
    arangodb::Result result = tf->addAction(
      std::make_shared<Action>(std::move(action_base_ptr)), false);

    REQUIRE(result.ok());   // has not executed, ok() is about parse and list add
    REQUIRE(tf->_recentAction->result().ok());
    pre_thread.push_back({1,0,READY,0});
    post_thread.push_back({1,1,FAILED,100});

    action_base_ptr.reset(
      (ActionBase*) new TestActionBasic(
        *tf, ActionDescription(std::map<std::string,std::string>{
            {"name","TestActionBasic"},{"iterate_count","2"}
          })));
    result = tf->addAction(
      std::make_shared<Action>(std::move(action_base_ptr)), false);

    REQUIRE(result.ok());   // has not executed, ok() is about parse and list add
    REQUIRE(tf->_recentAction->result().ok());
    pre_thread.push_back({2,0,READY,0});
    post_thread.push_back({2,0,COMPLETE,2});

    //   c. duplicate of 'a', should fail to add
    action_base_ptr.reset(
      (ActionBase*) new TestActionBasic(
        *tf, ActionDescription(std::map<std::string,std::string>{
            {"name","TestActionBasic"},{"iterate_count","100"},{"result_code","1"}
          })));
    result = tf->addAction(
      std::make_shared<Action>(std::move(action_base_ptr)), false);

    REQUIRE(!result.ok());   // has not executed, ok() is about parse and list add
    // _recentAction will NOT contain the aborted object ... don't test it

    //
    // 2. see if happy about queue prior to threads running
    REQUIRE(tf->verifyRegistryState(pre_thread));

    //
    // 3. start threads AFTER ApplicationServer known to be running
    tf->setMaintenanceThreadsMax(arangodb::MaintenanceFeature::minThreadLimit);

    //
    // 4. loop while waiting for threads to complete all actions
    tf->waitRegistryComplete();

    //
    // 5. verify completed actions
    REQUIRE(tf->verifyRegistryState(post_thread));

#if 0   // for debugging
    std::cout << tf->toVelocyPack().toJson() << std::endl;
#endif

    //
    // 6. bring down the ApplicationServer, i.e. clean up
    as.beginShutdown();

    th.join();

  }


  SECTION("Action that generates a pre-action") {
    std::vector<Expected> pre_thread, post_thread;

    std::shared_ptr<arangodb::options::ProgramOptions> po =
      std::make_shared<arangodb::options::ProgramOptions>(
        "test", std::string(), std::string(), "path");
    arangodb::application_features::ApplicationServer as(po, nullptr);
    TestMaintenanceFeature * tf = new TestMaintenanceFeature(as);
    as.addFeature(tf);
    std::thread th(
      &arangodb::application_features::ApplicationServer::run, &as, 0, nullptr);

    //
    // 1. load up the queue without threads running
    //   a. 100 iterations then fail
    std::unique_ptr<ActionBase> action_base_ptr;
    action_base_ptr.reset(
      (ActionBase*) new TestActionBasic(
        *tf, ActionDescription(std::map<std::string,std::string>{
            {"name","TestActionBasic"},{"iterate_count","100"},{"preaction_result_code","0"}
          })));
    arangodb::Result result = tf->addAction(
      std::make_shared<Action>(std::move(action_base_ptr)), false);

    REQUIRE(result.ok());   // has not executed, ok() is about parse and list add
    REQUIRE(tf->_recentAction->result().ok());
    pre_thread.push_back({1,0,READY,0});
    post_thread.push_back({1,0,COMPLETE,100});
    post_thread.push_back({2,0,COMPLETE,100});  // preaction results

    //
    // 2. see if happy about queue prior to threads running
    REQUIRE(tf->verifyRegistryState(pre_thread));

    //
    // 3. start threads AFTER ApplicationServer known to be running
    tf->setMaintenanceThreadsMax(arangodb::MaintenanceFeature::minThreadLimit);

    //
    // 4. loop while waiting for threads to complete all actions
    tf->waitRegistryComplete();

    //
    // 5. verify completed actions
    REQUIRE(tf->verifyRegistryState(post_thread));

#if 0   // for debugging
    std::cout << tf->toVelocyPack().toJson() << std::endl;
#endif

    //
    // 6. bring down the ApplicationServer, i.e. clean up
    as.beginShutdown();
    th.join();
  }


 SECTION("Action that generates a post-action") {
    std::vector<Expected> pre_thread, post_thread;

    std::shared_ptr<arangodb::options::ProgramOptions> po = std::make_shared<arangodb::options::ProgramOptions>("test", std::string(), std::string(), "path");
    arangodb::application_features::ApplicationServer as(po, nullptr);
    TestMaintenanceFeature * tf = new TestMaintenanceFeature(as);
    as.addFeature(tf);
    std::thread th(&arangodb::application_features::ApplicationServer::run, &as, 0, nullptr);

    //
    // 1. load up the queue without threads running
    //   a. 100 iterations then fail
    std::unique_ptr<ActionBase> action_base_ptr;
    action_base_ptr.reset(
      (ActionBase*) new TestActionBasic(
        *tf, ActionDescription(std::map<std::string,std::string>{
            {"name","TestActionBasic"},{"iterate_count","100"},{"postaction_result_code","0"}
          })));
    arangodb::Result result = tf->addAction(
      std::make_shared<Action>(std::move(action_base_ptr)), false);

    REQUIRE(result.ok());   // has not executed, ok() is about parse and list add
    REQUIRE(tf->_recentAction->result().ok());
    pre_thread.push_back({1,0,READY,0});
    post_thread.push_back({1,0,COMPLETE,100});
    post_thread.push_back({2,0,COMPLETE,100});  // postaction results

    //
    // 2. see if happy about queue prior to threads running
    REQUIRE(tf->verifyRegistryState(pre_thread));

    //
    // 3. start threads AFTER ApplicationServer known to be running
    tf->setMaintenanceThreadsMax(arangodb::MaintenanceFeature::minThreadLimit);

    //
    // 4. loop while waiting for threads to complete all actions
    tf->waitRegistryComplete();

    //
    // 5. verify completed actions
    REQUIRE(tf->verifyRegistryState(post_thread));

#if 0   // for debugging
    std::cout << tf->toVelocyPack().toJson() << std::endl;
#endif

    //
    // 6. bring down the ApplicationServer, i.e. clean up
    as.beginShutdown();
    th.join();
  }

  SECTION("Priority queue should be able to process fast tracked action") {
    std::vector<Expected> pre_thread, post_thread;

    std::shared_ptr<arangodb::options::ProgramOptions> po =
      std::make_shared<arangodb::options::ProgramOptions>(
        "test", std::string(), std::string(), "path");
    
    arangodb::application_features::ApplicationServer as(po, nullptr);
    TestMaintenanceFeature * tf = new TestMaintenanceFeature(as);
    as.addFeature(tf);
    std::thread th(
      &arangodb::application_features::ApplicationServer::run, &as, 0, nullptr);

    //
    // 1. load up the queue without threads running
    //   a. 100 iterations then fail
    std::unique_ptr<ActionBase> action_base_ptr;
    action_base_ptr.reset(
      (ActionBase*) new TestActionBasic(
        *tf, ActionDescription(std::map<std::string,std::string>{
            {"name","TestActionBasic"},{"iterate_count","100"},
            {TestActionBasic::FAST_TRACK, ""}})));
    arangodb::Result result = tf->addAction(
      std::make_shared<Action>(std::move(action_base_ptr)), false);

    REQUIRE(result.ok());   // has not executed, ok() is about parse and list add
    REQUIRE(tf->_recentAction->result().ok());

    //
    // 2. start threads AFTER ApplicationServer known to be running
    tf->setMaintenanceThreadsMax(arangodb::MaintenanceFeature::minThreadLimit-1);

    //
    // 3. loop while waiting for threads to complete all actions
    tf->waitRegistryComplete();

#if 0   // for debugging
    std::cout << tf->toVelocyPack().toJson() << std::endl;
#endif

    //
    // 4. bring down the ApplicationServer, i.e. clean up
    as.beginShutdown();
    th.join();
  }

  
  SECTION("Action delete") {
    std::vector<Expected> pre_thread, post_thread;

    std::shared_ptr<arangodb::options::ProgramOptions> po = std::make_shared<arangodb::options::ProgramOptions>("test", std::string(), std::string(), "path");
    arangodb::application_features::ApplicationServer as(po, nullptr);
    TestMaintenanceFeature * tf = new TestMaintenanceFeature(as);
    as.addFeature(tf);
    std::thread th(&arangodb::application_features::ApplicationServer::run, &as, 0, nullptr);

    //
    // 1. load up the queue without threads running
    //   a. 100 iterations then fail
    std::unique_ptr<ActionBase> action_base_ptr;
    action_base_ptr.reset(
      (ActionBase*) new TestActionBasic(
        *tf, ActionDescription(std::map<std::string,std::string>{
            {"name","TestActionBasic"},{"iterate_count","100"},{"postaction_result_code","0"}
          })));
    arangodb::Result result = tf->addAction(
      std::make_shared<Action>(std::move(action_base_ptr)), false);

    REQUIRE(result.ok());   // has not executed, ok() is about parse and list add
    REQUIRE(tf->_recentAction->result().ok());
    pre_thread.push_back({1,0,READY,0});
    post_thread.push_back({1,0,FAILED,0});

    //
    // 2. see if happy about queue prior to threads running
    REQUIRE(tf->verifyRegistryState(pre_thread));
    tf->deleteAction(1);

    //
    // 3. start threads AFTER ApplicationServer known to be running
    tf->setMaintenanceThreadsMax(arangodb::MaintenanceFeature::minThreadLimit);

    //
    // 4. loop while waiting for threads to complete all actions
    tf->waitRegistryComplete();

    //
    // 5. verify completed actions
    REQUIRE(tf->verifyRegistryState(post_thread));

#if 0   // for debugging
    std::cout << tf->toVelocyPack().toJson() << std::endl;
#endif

    //
    // 6. bring down the ApplicationServer, i.e. clean up
    as.beginShutdown();
    th.join();
  }

  

 } // MaintenanceFeatureThreaded
