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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/StorageEngineMock.h"

#include "Cluster/ClusterFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "RestServer/FlushFeature.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/arangod.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Cluster/ClusterFeature.h"
#include "Metrics/ClusterMetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class FlushFeatureTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::WARN>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER,
                                            arangodb::LogLevel::FATAL>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::ENGINES,
                                            arangodb::LogLevel::FATAL> {
 protected:
  arangodb::ArangodServer server;
  StorageEngineMock engine;
  std::vector<
      std::pair<arangodb::application_features::ApplicationFeature&, bool>>
      features;

  FlushFeatureTest() : server(nullptr, nullptr), engine(server) {
    auto& metrics = server.addFeature<arangodb::metrics::MetricsFeature>(
        arangodb::LazyApplicationFeatureReference<
            arangodb::QueryRegistryFeature>(server),
        arangodb::LazyApplicationFeatureReference<arangodb::DatabaseFeature>(
            server),
        arangodb::LazyApplicationFeatureReference<
            arangodb::metrics::ClusterMetricsFeature>(nullptr),
        arangodb::LazyApplicationFeatureReference<arangodb::ClusterFeature>(
            nullptr));
    features.emplace_back(metrics, false);
    features.emplace_back(server.addFeature<arangodb::AuthenticationFeature>(),
                          false);  // required for ClusterFeature::prepare()
    features.emplace_back(server.addFeature<arangodb::ClusterFeature>(metrics),
                          false);  // required for V8DealerFeature::prepare()
    auto& dbFeature = server.addFeature<arangodb::DatabaseFeature>();
    features.emplace_back(dbFeature, false);
    dbFeature.setEngineTesting(&engine);
    features.emplace_back(
        server.addFeature<arangodb::QueryRegistryFeature>(
            server.getFeature<arangodb::metrics::MetricsFeature>()),
        false);  // required for TRI_vocbase_t

    for (auto& f : features) {
      f.first.prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first.start();
      }
    }
  }

  ~FlushFeatureTest() {
    server.getFeature<arangodb::DatabaseFeature>().setEngineTesting(nullptr);

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first.stop();
      }
    }

    for (auto& f : features) {
      f.first.unprepare();
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(FlushFeatureTest, test_subscription_retention) {
  struct TestFlushSubscription : arangodb::FlushSubscription {
    TestFlushSubscription() : _name("test") {}
    TRI_voc_tick_t tick() const noexcept override { return _tick; }
    std::string const& name() const override { return _name; }

    TRI_voc_tick_t _tick{};
    std::string const _name{};
  };

  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  TRI_vocbase_t* vocbase;
  ASSERT_TRUE(dbFeature.createDatabase(testDBInfo(server), vocbase).ok());
  ASSERT_NE(nullptr, vocbase);

  auto& metrics = server.getFeature<arangodb::metrics::MetricsFeature>();
  arangodb::FlushFeature feature(server, metrics);
  feature.prepare();

  {
    auto subscription = std::make_shared<TestFlushSubscription>();
    ASSERT_EQ("test", subscription->name());
    feature.registerFlushSubscription(subscription);

    auto const subscriptionTick = engine.currentTick();
    engine.incrementTick(1);
    auto const currentTick = engine.currentTick();
    ASSERT_LT(subscriptionTick, currentTick);
    subscription->_tick = subscriptionTick;

    {
      auto [active, removed, releasedTick] = feature.releaseUnusedTicks();
      ASSERT_EQ(1, active);
      ASSERT_EQ(0, removed);                         // reference is being held
      ASSERT_EQ(subscription->_tick, releasedTick);  // min tick released
    }

    auto const newSubscriptionTick = currentTick;
    engine.incrementTick(1);
    auto const newCurrentTick = engine.currentTick();
    ASSERT_LT(subscriptionTick, newCurrentTick);
    subscription->_tick = newSubscriptionTick;

    {
      auto [active, removed, releasedTick] = feature.releaseUnusedTicks();
      ASSERT_EQ(1, active);
      ASSERT_EQ(0, removed);                         // reference is being held
      ASSERT_EQ(subscription->_tick, releasedTick);  // min tick released
    }
  }

  auto [active, removed, releasedTick] = feature.releaseUnusedTicks();
  ASSERT_EQ(0, active);
  ASSERT_EQ(1, removed);  // stale subscription was removed
  ASSERT_EQ(engine.currentTick(), releasedTick);  // min tick released
}
