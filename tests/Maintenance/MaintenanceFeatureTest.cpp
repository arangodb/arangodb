////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for MaintenanceFeature
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
#include "Basics/Result.h"
#include "Cluster/Action.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/Maintenance.h"

#include "MaintenanceFeatureMock.h"

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
      _preAction = std::make_shared<ActionDescription>(pred, arangodb::maintenance::NORMAL_PRIORITY);
    } // if


    if (description.get("postaction_result_code", value).ok()) {
      std::map<std::string, std::string> postd {
        {"name","TestActionBasic"}, {"result_code",value}};
      if (gres.ok()) {
        postd.insert({"iterate_count",iterate_count});
      }
      _postAction = std::make_shared<ActionDescription>(postd, arangodb::maintenance::NORMAL_PRIORITY);
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
            {"name","TestActionBasic"},{"iterate_count","0"}}, arangodb::maintenance::NORMAL_PRIORITY)));
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
          }, arangodb::maintenance::NORMAL_PRIORITY)));
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
            {"name","TestActionBasic"},{"iterate_count","1"}}, arangodb::maintenance::NORMAL_PRIORITY)));
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
          }, arangodb::maintenance::NORMAL_PRIORITY)));
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
          }, arangodb::maintenance::NORMAL_PRIORITY)));
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
            {"name","TestActionBasic"},{"iterate_count","100"}}, arangodb::maintenance::NORMAL_PRIORITY)));
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
          }, arangodb::maintenance::NORMAL_PRIORITY)));
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
          }, arangodb::maintenance::NORMAL_PRIORITY)));
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
          }, arangodb::maintenance::NORMAL_PRIORITY)));
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
          }, arangodb::maintenance::NORMAL_PRIORITY)));
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
          }, arangodb::maintenance::NORMAL_PRIORITY)));
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
          }, arangodb::maintenance::NORMAL_PRIORITY)));
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
            {TestActionBasic::FAST_TRACK, ""}}, arangodb::maintenance::NORMAL_PRIORITY)));
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
          }, arangodb::maintenance::NORMAL_PRIORITY)));
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
