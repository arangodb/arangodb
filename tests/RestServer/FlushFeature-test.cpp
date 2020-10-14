////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "velocypack/Parser.h"

#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/StorageEngineMock.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/encoding.h"
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBRecoveryHelper.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/FlushThread.h"
#include "V8Server/V8DealerFeature.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class FlushFeatureTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::WARN>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER, arangodb::LogLevel::FATAL>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::ENGINES, arangodb::LogLevel::FATAL> {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature&, bool>> features;

  FlushFeatureTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    features.emplace_back(server.addFeature<arangodb::MetricsFeature>(), false); 
    features.emplace_back(server.addFeature<arangodb::AuthenticationFeature>(),
                          false);  // required for ClusterFeature::prepare()
    features.emplace_back(server.addFeature<arangodb::ClusterFeature>(), false);  // required for V8DealerFeature::prepare()
    features.emplace_back(server.addFeature<arangodb::DatabaseFeature>(), false);  // required for MMFilesWalRecoverState constructor
    features.emplace_back(server.addFeature<arangodb::QueryRegistryFeature>(), false);  // required for TRI_vocbase_t
    features.emplace_back(server.addFeature<arangodb::V8DealerFeature>(), false);  // required for DatabaseFeature::createDatabase(...)

#if USE_ENTERPRISE
    features.emplace_back(server.addFeature<arangodb::LdapFeature>(), false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

    arangodb::DatabaseFeature::DATABASE =
        &server.getFeature<arangodb::DatabaseFeature>();

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
    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first.stop();
      }
    }

    for (auto& f : features) {
      f.first.unprepare();
    }

    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(FlushFeatureTest, test_subscription_retention) {
  struct TestFlushSubscripion : arangodb::FlushSubscription {
    TRI_voc_tick_t tick() const noexcept { return _tick; }

    TRI_voc_tick_t _tick{};
  };

  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
  TRI_vocbase_t* vocbase;
  ASSERT_TRUE(dbFeature.createDatabase(testDBInfo(server), vocbase).ok());
  ASSERT_NE(nullptr, vocbase);

  arangodb::FlushFeature feature(server);
  feature.prepare();

  {
    auto subscription = std::make_shared<TestFlushSubscripion>();
    feature.registerFlushSubscription(subscription);

    auto const subscriptionTick = engine.currentTick();
    auto const currentTick = TRI_NewTickServer();
    ASSERT_EQ(currentTick, engine.currentTick());
    ASSERT_LT(subscriptionTick, engine.currentTick());
    subscription->_tick = subscriptionTick;

    {
      size_t removed = 42;
      TRI_voc_tick_t releasedTick = 42;
      feature.releaseUnusedTicks(removed, releasedTick);
      ASSERT_EQ(0, removed);                         // reference is being held
      ASSERT_EQ(subscription->_tick, releasedTick);  // min tick released
    }

    auto const newSubscriptionTick = currentTick;
    auto const newCurrentTick = TRI_NewTickServer();
    ASSERT_EQ(newCurrentTick, engine.currentTick());
    ASSERT_LT(subscriptionTick, engine.currentTick());
    subscription->_tick = newSubscriptionTick;

    {
      size_t removed = 42;
      TRI_voc_tick_t releasedTick = 42;
      feature.releaseUnusedTicks(removed, releasedTick);
      ASSERT_EQ(0, removed);                         // reference is being held
      ASSERT_EQ(subscription->_tick, releasedTick);  // min tick released
    }
  }

  size_t removed = 42;
  TRI_voc_tick_t releasedTick = 42;
  feature.releaseUnusedTicks(removed, releasedTick);
  ASSERT_EQ(1, removed);  // stale subscription was removed
  ASSERT_EQ(engine.currentTick(), releasedTick);  // min tick released
}
