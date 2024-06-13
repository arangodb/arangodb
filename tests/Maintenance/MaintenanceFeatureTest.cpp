////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "gtest/gtest.h"

#include <iostream>

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/Result.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/Action.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/Maintenance.h"
#include "Cluster/MaintenanceFeature.h"
#include "Metrics/ClusterMetricsFeature.h"
#include "Metrics/MetricsFeature.h"
#include "Mocks/Servers.h"
#include "RestServer/UpgradeFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

#include "MaintenanceFeatureMock.h"

static arangodb::ServerID const DBSERVER_ID;

//
// TestActionBasic simulates a multistep action by counting down
//  on each call to first() and next() until iteration counter is zero.
//  Returns false upon reaching zero
//
class TestActionBasic : public ActionBase {
 public:
  TestActionBasic(arangodb::MaintenanceFeature& feature,
                  ActionDescription const& description)
      : ActionBase(feature, description),
        _iteration(1),
        _resultCode(0),
        _reschedule(false) {
    std::string value, iterate_count;
    auto gres = description.get("iterate_count", iterate_count);

    if (gres.ok()) {
      _iteration = std::atol(iterate_count.c_str());
      // safety check
      if (_iteration < 0) {
        _iteration = 1;
      }  // if
    }    // if

    if (description.get(FAST_TRACK, value).ok()) {
      _labels.emplace(FAST_TRACK);
    }

    if (description.get("result_code", value).ok()) {
      _resultCode = ErrorCode{std::atoi(value.c_str())};
    }  // if

    if (description.get("preaction_result_code", value).ok()) {
      std::map<std::string, std::string> pred{{"name", "TestActionBasic"},
                                              {"result_code", value}};
      if (gres.ok()) {
        pred.insert({"iterate_count", iterate_count});
      }
      _preAction = std::make_shared<ActionDescription>(
          std::move(pred), arangodb::maintenance::NORMAL_PRIORITY, false);
    }  // if

    if (description.get("postaction_result_code", value).ok()) {
      std::map<std::string, std::string> postd{{"name", "TestActionBasic"},
                                               {"result_code", value}};
      if (gres.ok()) {
        postd.insert({"iterate_count", iterate_count});
      }
      _postAction = std::make_shared<ActionDescription>(
          std::move(postd), arangodb::maintenance::NORMAL_PRIORITY, false);
    }  // if

    if (description.get("reschedule", value).ok()) {
      if (value == "true") {
        _reschedule = true;
      }
    }
  };

  virtual ~TestActionBasic() = default;

  bool first() override {
    // a pre action needs to push before setting _result
    if (_preDesc) {
      createPreAction(_preDesc);
    } else if (0 == _iteration) {
      // time to set result?
      result(_resultCode);
    }  // if

    // verify first() called once
    if (0 != getProgress()) {
      result(TRI_ERROR_INTERNAL);
    }  // if

    return (iteratorEndTest());
  };

  bool next() override {
    // time to set result?
    if (0 == _iteration) {
      result(_resultCode);
    }  // if

    // verify next() called properly
    if (0 == getProgress()) {
      result(TRI_ERROR_INTERNAL);
    }  // if

    return (iteratorEndTest());
  };

 protected:
  bool iteratorEndTest() {
    bool more;

    //
    if (result().ok()) {
      more = 0 < _iteration--;

      // if going to stop, see if a postAction is needed
      if (!more && _postDesc) {
        createPostAction(_postDesc);
      }  // if
    } else {
      // !ok() ... always stop iteration
      more = false;
    }
    // Check if we need to reschedule:
    if (!more && _priority > 0 && _reschedule) {
      requeueMe(0);
    }
    return more;
  }  // iteratorEndTest

 public:
  int _iteration;
  ErrorCode _resultCode;
  std::shared_ptr<ActionDescription> _preDesc;
  std::shared_ptr<ActionDescription> _postDesc;
  bool _reschedule;

};  // TestActionBasic

class MaintenanceFeatureTestDBServer
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AGENCY,
                                            arangodb::LogLevel::FATAL>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER,
                                            arangodb::LogLevel::FATAL> {
 protected:
  arangodb::tests::mocks::MockDBServer server;

  MaintenanceFeatureTestDBServer() : server(DBSERVER_ID, false) {
    arangodb::ServerState::instance()->setRebootId(
        arangodb::RebootId{1});  // Hack.
    server.untrackFeature<arangodb::UpgradeFeature>();
    server.startFeatures();
  }
};

//
//
// Unit Tests start here
//
//
class MaintenanceFeatureTestUnthreaded : public ::testing::Test {
 protected:
  std::chrono::system_clock::time_point baseTime;
  std::chrono::system_clock::time_point noTime;
  MaintenanceFeatureTestUnthreaded()
      : baseTime(std::chrono::system_clock::now()) {}

  void SetUp() override {
    as.addFeature<arangodb::application_features::GreetingsFeaturePhase>(
        std::true_type{});
    as.addFeature<arangodb::metrics::MetricsFeature>(
        arangodb::LazyApplicationFeatureReference<
            arangodb::QueryRegistryFeature>(nullptr),
        arangodb::LazyApplicationFeatureReference<arangodb::StatisticsFeature>(
            nullptr),
        arangodb::LazyApplicationFeatureReference<
            arangodb::EngineSelectorFeature>(nullptr),
        arangodb::LazyApplicationFeatureReference<
            arangodb::metrics::ClusterMetricsFeature>(nullptr),
        arangodb::LazyApplicationFeatureReference<arangodb::ClusterFeature>(
            nullptr));
  }

  std::shared_ptr<arangodb::options::ProgramOptions> po =
      std::make_shared<arangodb::options::ProgramOptions>(
          "test", std::string(), std::string(), "path");
  arangodb::ArangodServer as{po, nullptr};
};

TEST_F(MaintenanceFeatureTestUnthreaded, iterate_action_0_times_ok) {
  TestMaintenanceFeature tf(as);
  tf.setSecondsActionsBlock(0);  // disable retry wait for now

  std::unique_ptr<ActionBase> action_base_ptr;
  action_base_ptr.reset(new TestActionBasic(
      tf, ActionDescription(
              std::map<std::string, std::string>{{"name", "TestActionBasic"},
                                                 {"iterate_count", "0"}},
              arangodb::maintenance::NORMAL_PRIORITY, false)));
  arangodb::Result result =
      tf.addAction(std::make_shared<Action>(std::move(action_base_ptr)), true);

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(tf._recentAction->result().ok());
  ASSERT_EQ(0, tf._recentAction->getProgress());
  ASSERT_EQ(tf._recentAction->getState(), COMPLETE);
  ASSERT_TRUE(tf._recentAction->done());
  ASSERT_EQ(1, tf._recentAction->id());
  ASSERT_LE(baseTime, tf._recentAction->getCreateTime());
  ASSERT_LE(baseTime, tf._recentAction->getStartTime());
  ASSERT_LE(baseTime, tf._recentAction->getDoneTime());
  ASSERT_EQ(noTime, tf._recentAction->getLastStatTime());
  ASSERT_LE(tf._recentAction->getCreateTime(),
            tf._recentAction->getStartTime());
  ASSERT_LE(tf._recentAction->getStartTime(), tf._recentAction->getDoneTime());
}

TEST_F(MaintenanceFeatureTestUnthreaded, iterate_action_0_times_fail) {
  TestMaintenanceFeature tf(as);
  tf.setSecondsActionsBlock(0);  // disable retry wait for now

  std::unique_ptr<ActionBase> action_base_ptr;
  action_base_ptr.reset(new TestActionBasic(
      tf, ActionDescription(
              std::map<std::string, std::string>{{"name", "TestActionBasic"},
                                                 {"iterate_count", "0"},
                                                 {"result_code", "1"}},
              arangodb::maintenance::NORMAL_PRIORITY, false)));
  arangodb::Result result =
      tf.addAction(std::make_shared<Action>(std::move(action_base_ptr)), true);

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(tf._recentAction->result().ok());
  ASSERT_EQ(0, tf._recentAction->getProgress());
  ASSERT_EQ(tf._recentAction->getState(), FAILED);
  ASSERT_TRUE(tf._recentAction->done());
  ASSERT_EQ(1, tf._recentAction->id());

  ASSERT_TRUE(baseTime <= tf._recentAction->getCreateTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getStartTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getDoneTime());
  ASSERT_EQ(noTime, tf._recentAction->getLastStatTime());
  ASSERT_TRUE(tf._recentAction->getCreateTime() <=
              tf._recentAction->getStartTime());
  ASSERT_TRUE(tf._recentAction->getStartTime() <=
              tf._recentAction->getDoneTime());
}

TEST_F(MaintenanceFeatureTestUnthreaded, iterate_action_1_time_ok) {
  TestMaintenanceFeature tf(as);
  tf.setSecondsActionsBlock(0);  // disable retry wait for now

  std::unique_ptr<ActionBase> action_base_ptr;
  action_base_ptr.reset(new TestActionBasic(
      tf, ActionDescription(
              std::map<std::string, std::string>{{"name", "TestActionBasic"},
                                                 {"iterate_count", "1"}},
              arangodb::maintenance::NORMAL_PRIORITY, false)));
  arangodb::Result result =
      tf.addAction(std::make_shared<Action>(std::move(action_base_ptr)), true);

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(tf._recentAction->result().ok());
  ASSERT_EQ(1, tf._recentAction->getProgress());
  ASSERT_EQ(tf._recentAction->getState(), COMPLETE);
  ASSERT_TRUE(tf._recentAction->done());
  ASSERT_EQ(1, tf._recentAction->id());

  ASSERT_TRUE(baseTime <= tf._recentAction->getCreateTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getStartTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getDoneTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getLastStatTime());
  ASSERT_TRUE(tf._recentAction->getCreateTime() <=
              tf._recentAction->getStartTime());
  ASSERT_TRUE(tf._recentAction->getStartTime() <=
              tf._recentAction->getDoneTime());
}

TEST_F(MaintenanceFeatureTestUnthreaded, iterate_action_1_time_fail) {
  TestMaintenanceFeature tf(as);
  tf.setSecondsActionsBlock(0);  // disable retry wait for now

  std::unique_ptr<ActionBase> action_base_ptr;
  action_base_ptr.reset(new TestActionBasic(
      tf, ActionDescription(
              std::map<std::string, std::string>{{"name", "TestActionBasic"},
                                                 {"iterate_count", "1"},
                                                 {"result_code", "1"}},
              arangodb::maintenance::NORMAL_PRIORITY, false)));
  arangodb::Result result =
      tf.addAction(std::make_shared<Action>(std::move(action_base_ptr)), true);

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(tf._recentAction->result().ok());
  ASSERT_EQ(1, tf._recentAction->getProgress());
  ASSERT_EQ(tf._recentAction->getState(), FAILED);
  ASSERT_TRUE(tf._recentAction->done());
  ASSERT_EQ(1, tf._recentAction->id());

  ASSERT_TRUE(baseTime <= tf._recentAction->getCreateTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getStartTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getDoneTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getLastStatTime());
  ASSERT_TRUE(tf._recentAction->getCreateTime() <=
              tf._recentAction->getStartTime());
  ASSERT_TRUE(tf._recentAction->getStartTime() <=
              tf._recentAction->getDoneTime());
  ASSERT_TRUE(tf._recentAction->getLastStatTime() <=
              tf._recentAction->getDoneTime());
}

TEST_F(MaintenanceFeatureTestUnthreaded, iterate_action_2_times_ok) {
  TestMaintenanceFeature tf(as);
  tf.setSecondsActionsBlock(0);  // disable retry wait for now

  std::unique_ptr<ActionBase> action_base_ptr;
  action_base_ptr.reset(new TestActionBasic(
      tf, ActionDescription(
              std::map<std::string, std::string>{{"name", "TestActionBasic"},
                                                 {"iterate_count", "2"}},
              arangodb::maintenance::NORMAL_PRIORITY, false)));
  arangodb::Result result =
      tf.addAction(std::make_shared<Action>(std::move(action_base_ptr)), true);

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(tf._recentAction->result().ok());
  ASSERT_EQ(2, tf._recentAction->getProgress());
  ASSERT_EQ(tf._recentAction->getState(), COMPLETE);
  ASSERT_TRUE(tf._recentAction->done());
  ASSERT_EQ(1, tf._recentAction->id());

  ASSERT_TRUE(baseTime <= tf._recentAction->getCreateTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getStartTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getDoneTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getLastStatTime());
  ASSERT_TRUE(tf._recentAction->getCreateTime() <=
              tf._recentAction->getStartTime());
  ASSERT_TRUE(tf._recentAction->getStartTime() <=
              tf._recentAction->getDoneTime());
  ASSERT_TRUE(tf._recentAction->getLastStatTime() <=
              tf._recentAction->getDoneTime());
}

TEST_F(MaintenanceFeatureTestUnthreaded, iterate_action_100_times_ok) {
  TestMaintenanceFeature tf(as);
  tf.setSecondsActionsBlock(0);  // disable retry wait for now

  std::unique_ptr<ActionBase> action_base_ptr;
  action_base_ptr.reset(new TestActionBasic(
      tf, ActionDescription(
              std::map<std::string, std::string>{{"name", "TestActionBasic"},
                                                 {"iterate_count", "100"}},
              arangodb::maintenance::NORMAL_PRIORITY, false)));
  arangodb::Result result =
      tf.addAction(std::make_shared<Action>(std::move(action_base_ptr)), true);

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(tf._recentAction->result().ok());
  ASSERT_EQ(100, tf._recentAction->getProgress());
  ASSERT_EQ(tf._recentAction->getState(), COMPLETE);
  ASSERT_TRUE(tf._recentAction->done());
  ASSERT_EQ(1, tf._recentAction->id());

  ASSERT_TRUE(baseTime <= tf._recentAction->getCreateTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getStartTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getDoneTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getLastStatTime());
  ASSERT_TRUE(tf._recentAction->getCreateTime() <=
              tf._recentAction->getStartTime());
  ASSERT_TRUE(tf._recentAction->getStartTime() <=
              tf._recentAction->getDoneTime());
  ASSERT_TRUE(tf._recentAction->getLastStatTime() <=
              tf._recentAction->getDoneTime());
}

TEST_F(MaintenanceFeatureTestUnthreaded, iterate_action_100_times_fail) {
  TestMaintenanceFeature tf(as);
  tf.setSecondsActionsBlock(0);  // disable retry wait for now

  std::unique_ptr<ActionBase> action_base_ptr;
  action_base_ptr.reset(new TestActionBasic(
      tf, ActionDescription(
              std::map<std::string, std::string>{{"name", "TestActionBasic"},
                                                 {"iterate_count", "100"},
                                                 {"result_code", "1"}},
              arangodb::maintenance::NORMAL_PRIORITY, false)));
  arangodb::Result result =
      tf.addAction(std::make_shared<Action>(std::move(action_base_ptr)), true);

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(tf._recentAction->result().ok());
  ASSERT_EQ(100, tf._recentAction->getProgress());
  ASSERT_EQ(tf._recentAction->getState(), FAILED);
  ASSERT_TRUE(tf._recentAction->done());
  ASSERT_EQ(1, tf._recentAction->id());

  ASSERT_TRUE(baseTime <= tf._recentAction->getCreateTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getStartTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getDoneTime());
  ASSERT_TRUE(baseTime <= tf._recentAction->getLastStatTime());
  ASSERT_TRUE(tf._recentAction->getCreateTime() <=
              tf._recentAction->getStartTime());
  ASSERT_TRUE(tf._recentAction->getStartTime() <=
              tf._recentAction->getDoneTime());
  ASSERT_TRUE(tf._recentAction->getLastStatTime() <=
              tf._recentAction->getDoneTime());
}

struct MaintenanceFeatureTestThreaded : ::testing::Test {
  std::shared_ptr<arangodb::options::ProgramOptions> po =
      std::make_shared<arangodb::options::ProgramOptions>(
          "test", std::string(), std::string(), "path");
  arangodb::ArangodServer as{po, nullptr};

  void SetUp() override {
    using namespace arangodb;
    as.addFeature<application_features::GreetingsFeaturePhase>(
        std::false_type{});
    as.addFeature<metrics::MetricsFeature>(
        LazyApplicationFeatureReference<QueryRegistryFeature>(nullptr),
        LazyApplicationFeatureReference<StatisticsFeature>(nullptr),
        LazyApplicationFeatureReference<EngineSelectorFeature>(nullptr),
        LazyApplicationFeatureReference<metrics::ClusterMetricsFeature>(
            nullptr),
        LazyApplicationFeatureReference<ClusterFeature>(nullptr));
  }
};

TEST_F(MaintenanceFeatureTestThreaded, populate_action_queue_and_validate) {
  TestMaintenanceFeature& tf =
      as.addFeature<arangodb::MaintenanceFeature, TestMaintenanceFeature>();

  std::thread th(&arangodb::ArangodServer::run, &as, 0, nullptr);

  auto threadGuard = arangodb::scopeGuard([&]() noexcept {
    as.beginShutdown();
    th.join();
  });

  //
  // 1. load up the queue without threads running
  //   a. 100 iterations then fail

  std::unique_ptr<ActionBase> action_base_ptr;
  action_base_ptr.reset(new TestActionBasic(
      tf, ActionDescription(
              std::map<std::string, std::string>{{"name", "TestActionBasic"},
                                                 {"iterate_count", "100"},
                                                 {"result_code", "1"}},
              arangodb::maintenance::NORMAL_PRIORITY, false)));
  arangodb::Result result =
      tf.addAction(std::make_shared<Action>(std::move(action_base_ptr)), false);

  ASSERT_TRUE(
      result.ok());  // has not executed, ok() is about parse and list add
  ASSERT_TRUE(tf._recentAction->result().ok());

  std::vector<Expected> pre_thread, post_thread;
  pre_thread.push_back({1, 0, READY, 0});
  post_thread.push_back({1, 1, FAILED, 100});

  action_base_ptr.reset(new TestActionBasic(
      tf, ActionDescription(
              std::map<std::string, std::string>{{"name", "TestActionBasic"},
                                                 {"iterate_count", "2"}},
              arangodb::maintenance::NORMAL_PRIORITY, false)));
  result =
      tf.addAction(std::make_shared<Action>(std::move(action_base_ptr)), false);

  ASSERT_TRUE(
      result.ok());  // has not executed, ok() is about parse and list add
  ASSERT_TRUE(tf._recentAction->result().ok());
  pre_thread.push_back({2, 0, READY, 0});
  post_thread.push_back({2, 0, COMPLETE, 2});

  //   c. duplicate of 'a', should fail to add
  action_base_ptr.reset(new TestActionBasic(
      tf, ActionDescription(
              std::map<std::string, std::string>{{"name", "TestActionBasic"},
                                                 {"iterate_count", "100"},
                                                 {"result_code", "1"}},
              arangodb::maintenance::NORMAL_PRIORITY, false)));
  result =
      tf.addAction(std::make_shared<Action>(std::move(action_base_ptr)), false);

  ASSERT_FALSE(
      result.ok());  // has not executed, ok() is about parse and list add
  // _recentAction will NOT contain the aborted object ... don't test it

  //
  // 2. see if happy about queue prior to threads running
  ASSERT_TRUE(tf.verifyRegistryState(pre_thread));

  //
  // 3. start threads AFTER ApplicationServer known to be running
  tf.setMaintenanceThreadsMax(arangodb::MaintenanceFeature::minThreadLimit);

  //
  // 4. loop while waiting for threads to complete all actions
  tf.waitRegistryComplete();

  //
  // 5. verify completed actions
  ASSERT_TRUE(tf.verifyRegistryState(post_thread));

#if 0  // for debugging
    std::cout << tf.toVelocyPack().toJson() << std::endl;
#endif
}

TEST_F(MaintenanceFeatureTestThreaded, action_that_generates_a_preaction) {
  TestMaintenanceFeature& tf =
      as.addFeature<arangodb::MaintenanceFeature, TestMaintenanceFeature>();

  std::thread th(&arangodb::ArangodServer::run, &as, 0, nullptr);

  auto threadGuard = arangodb::scopeGuard([&]() noexcept {
    as.beginShutdown();
    th.join();
  });

  //
  // 1. load up the queue without threads running
  //   a. 100 iterations then fail
  std::unique_ptr<ActionBase> action_base_ptr;
  action_base_ptr.reset(new TestActionBasic(
      tf,
      ActionDescription(
          std::map<std::string, std::string>{{"name", "TestActionBasic"},
                                             {"iterate_count", "100"},
                                             {"preaction_result_code", "0"}},
          arangodb::maintenance::NORMAL_PRIORITY, false)));
  arangodb::Result result =
      tf.addAction(std::make_shared<Action>(std::move(action_base_ptr)), false);

  ASSERT_TRUE(
      result.ok());  // has not executed, ok() is about parse and list add
  ASSERT_TRUE(tf._recentAction->result().ok());

  std::vector<Expected> pre_thread, post_thread;
  pre_thread.push_back({1, 0, READY, 0});
  post_thread.push_back({1, 0, COMPLETE, 100});
  // The following is somehow expected, but it is not possible since we
  // fixed `verifyRegistryState`. This means that probably the pre actions
  // do not work. They are scheduled to be removed.
  // post_thread.push_back({2, 0, COMPLETE, 100});  // preaction results

  //
  // 2. see if happy about queue prior to threads running
  ASSERT_TRUE(tf.verifyRegistryState(pre_thread));

  //
  // 3. start threads AFTER ApplicationServer known to be running
  tf.setMaintenanceThreadsMax(arangodb::MaintenanceFeature::minThreadLimit);

  //
  // 4. loop while waiting for threads to complete all actions
  tf.waitRegistryComplete();

  //
  // 5. verify completed actions
  ASSERT_TRUE(tf.verifyRegistryState(post_thread));

#if 0  // for debugging
    std::cout << tf.toVelocyPack().toJson() << std::endl;
#endif
}

TEST_F(MaintenanceFeatureTestThreaded, action_that_generates_a_postaction) {
  TestMaintenanceFeature& tf =
      as.addFeature<arangodb::MaintenanceFeature, TestMaintenanceFeature>();

  std::thread th(&arangodb::ArangodServer::run, &as, 0, nullptr);

  auto threadGuard = arangodb::scopeGuard([&]() noexcept {
    as.beginShutdown();
    th.join();
  });

  //
  // 1. load up the queue without threads running
  //   a. 100 iterations then fail
  std::unique_ptr<ActionBase> action_base_ptr;
  action_base_ptr.reset(new TestActionBasic(
      tf,
      ActionDescription(
          std::map<std::string, std::string>{{"name", "TestActionBasic"},
                                             {"iterate_count", "100"},
                                             {"postaction_result_code", "0"}},
          arangodb::maintenance::NORMAL_PRIORITY, false)));
  arangodb::Result result =
      tf.addAction(std::make_shared<Action>(std::move(action_base_ptr)), false);

  ASSERT_TRUE(
      result.ok());  // has not executed, ok() is about parse and list add
  ASSERT_TRUE(tf._recentAction->result().ok());
  std::vector<Expected> pre_thread, post_thread;
  pre_thread.push_back({1, 0, READY, 0});
  post_thread.push_back({1, 0, COMPLETE, 100});
  // The following is somehow expected, but it is not possible since we
  // fixed `verifyRegistryState`. This means that probably the post actions
  // do not work. They are scheduled to be removed.
  // post_thread.push_back({2, 0, COMPLETE, 100});  // postaction results

  //
  // 2. see if happy about queue prior to threads running
  ASSERT_TRUE(tf.verifyRegistryState(pre_thread));

  //
  // 3. start threads AFTER ApplicationServer known to be running
  tf.setMaintenanceThreadsMax(arangodb::MaintenanceFeature::minThreadLimit);

  //
  // 4. loop while waiting for threads to complete all actions
  tf.waitRegistryComplete();

  //
  // 5. verify completed actions
  ASSERT_TRUE(tf.verifyRegistryState(post_thread));

#if 0  // for debugging
    std::cout << tf.toVelocyPack().toJson() << std::endl;
#endif
}

TEST_F(MaintenanceFeatureTestThreaded,
       priority_queue_should_be_able_to_process_fast_tracked_action) {
  TestMaintenanceFeature& tf =
      as.addFeature<arangodb::MaintenanceFeature, TestMaintenanceFeature>();

  std::thread th(&arangodb::ArangodServer::run, &as, 0, nullptr);

  auto threadGuard = arangodb::scopeGuard([&]() noexcept {
    as.beginShutdown();
    th.join();
  });

  //
  // 1. load up the queue without threads running
  //   a. 100 iterations then fail
  std::unique_ptr<ActionBase> action_base_ptr;
  action_base_ptr.reset(new TestActionBasic(
      tf,
      ActionDescription(
          std::map<std::string, std::string>{{"name", "TestActionBasic"},
                                             {"iterate_count", "100"},
                                             {TestActionBasic::FAST_TRACK, ""}},
          arangodb::maintenance::NORMAL_PRIORITY, false)));
  arangodb::Result result =
      tf.addAction(std::make_shared<Action>(std::move(action_base_ptr)), false);

  ASSERT_TRUE(
      result.ok());  // has not executed, ok() is about parse and list add
  ASSERT_TRUE(tf._recentAction->result().ok());

  //
  // 2. start threads AFTER ApplicationServer known to be running
  tf.setMaintenanceThreadsMax(arangodb::MaintenanceFeature::minThreadLimit - 1);

  //
  // 3. loop while waiting for threads to complete all actions
  tf.waitRegistryComplete();

#if 0  // for debugging
    std::cout << tf.toVelocyPack().toJson() << std::endl;
#endif
}

TEST_F(MaintenanceFeatureTestThreaded, action_delete) {
  TestMaintenanceFeature& tf =
      as.addFeature<arangodb::MaintenanceFeature, TestMaintenanceFeature>();

  std::thread th(&arangodb::ArangodServer::run, &as, 0, nullptr);

  auto threadGuard = arangodb::scopeGuard([&]() noexcept {
    as.beginShutdown();
    th.join();
  });

  //
  // 1. load up the queue without threads running
  //   a. 100 iterations then fail
  std::unique_ptr<ActionBase> action_base_ptr;
  action_base_ptr.reset(new TestActionBasic(
      tf,
      ActionDescription(
          std::map<std::string, std::string>{{"name", "TestActionBasic"},
                                             {"iterate_count", "100"},
                                             {"postaction_result_code", "0"}},
          arangodb::maintenance::NORMAL_PRIORITY, false)));
  arangodb::Result result =
      tf.addAction(std::make_shared<Action>(std::move(action_base_ptr)), false);

  ASSERT_TRUE(
      result.ok());  // has not executed, ok() is about parse and list add
  ASSERT_TRUE(tf._recentAction->result().ok());
  std::vector<Expected> pre_thread, post_thread;
  pre_thread.push_back({1, 0, READY, 0});
  post_thread.push_back({1, 0, FAILED, 0});

  //
  // 2. see if happy about queue prior to threads running
  ASSERT_TRUE(tf.verifyRegistryState(pre_thread));
  tf.deleteAction(1);

  //
  // 3. start threads AFTER ApplicationServer known to be running
  tf.setMaintenanceThreadsMax(arangodb::MaintenanceFeature::minThreadLimit);

  //
  // 4. loop while waiting for threads to complete all actions
  tf.waitRegistryComplete();

  //
  // 5. verify completed actions
  ASSERT_TRUE(tf.verifyRegistryState(post_thread));

#if 0  // for debugging
    std::cout << tf.toVelocyPack().toJson() << std::endl;
#endif
}

TEST_F(MaintenanceFeatureTestThreaded,
       populate_action_queue_reschedule_and_validate) {
  TestMaintenanceFeature& tf =
      as.addFeature<arangodb::MaintenanceFeature, TestMaintenanceFeature>();

  std::thread th(&arangodb::ArangodServer::run, &as, 0, nullptr);

  auto threadGuard = arangodb::scopeGuard([&]() noexcept {
    as.beginShutdown();
    th.join();
  });

  // 0. Add factory for our TestActionBasic:
  auto factory = [](arangodb::MaintenanceFeature& f,
                    ActionDescription const& a) {
    return std::make_unique<TestActionBasic>(f, a);
  };
  Action::addNewFactoryForTest("TestActionBasic", std::move(factory));

  //
  // 1. load up the queue without threads running
  //   a. 1 iteration then fail

  std::unique_ptr<ActionBase> action_base_ptr;
  action_base_ptr.reset(new TestActionBasic(
      tf, ActionDescription(
              std::map<std::string, std::string>{{"name", "TestActionBasic"},
                                                 {"iterate_count", "1"},
                                                 {"result_code", "1"},
                                                 {"reschedule", "true"}},
              arangodb::maintenance::NORMAL_PRIORITY, false)));
  arangodb::Result result =
      tf.addAction(std::make_shared<Action>(std::move(action_base_ptr)), false);

  ASSERT_TRUE(
      result.ok());  // has not executed, ok() is about parse and list add
  ASSERT_TRUE(tf._recentAction->result().ok());
  std::vector<Expected> pre_thread, post_thread;
  pre_thread.push_back({1, 0, READY, 0});
  post_thread.push_back({1, 1, FAILED, 1});
  post_thread.push_back({2, 1, FAILED, 1});

  // 2. see if happy about queue prior to threads running
  ASSERT_TRUE(tf.verifyRegistryState(pre_thread));

  //
  // 3. start threads AFTER ApplicationServer known to be running
  tf.setMaintenanceThreadsMax(arangodb::MaintenanceFeature::minThreadLimit);

  //
  // 4. loop while waiting for threads to complete all actions
  // Since the rescheduled actions is not visible _before_ the old action
  // is moved to FINISHED, it is not enough to wait for all actions to be done.
  // Instead we have to wait for two actions to be done.
  while (tf.waitRegistryComplete() != 2)
    ;

  //
  // 5. verify completed actions
  ASSERT_TRUE(tf.verifyRegistryState(post_thread));

#if 0  // for debugging
  std::cout << tf.toVelocyPack().toJson() << std::endl;
#endif
}

// temporarily disabled since it may hang
TEST_F(MaintenanceFeatureTestDBServer, test_synchronize_shard_abort) {
  auto& mf = server.getFeature<arangodb::MaintenanceFeature>();
  mf.start();

  std::shared_ptr<ActionDescription> description =
      std::make_shared<ActionDescription>(
          std::map<std::string, std::string>{{NAME, SYNCHRONIZE_SHARD},
                                             {DATABASE, "_system"},
                                             {COLLECTION, "tmp"},
                                             {SHARD, "s1"},
                                             {THE_LEADER, "PRMR-1"},
                                             {SHARD_VERSION, "1"},
                                             {FORCED_RESYNC, "false"}},
          SYNCHRONIZE_PRIORITY, true);

  // The following will execute the action right away:
  auto res = mf.addAction(description, true /* executeNow */);
  ASSERT_FALSE(res.ok());  // must have been aborted
  mf.beginShutdown();
  mf.stop();
}
