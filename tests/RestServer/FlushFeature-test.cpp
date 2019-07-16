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

#include "../Mocks/StorageEngineMock.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/encoding.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterComm.h"
#include "gtest/gtest.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "GeneralServer/AuthenticationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBRecoveryHelper.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/FlushThread.h"
#include "V8Server/V8DealerFeature.h"
#include "velocypack/Parser.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class FlushFeatureTest : public ::testing::Test {
 protected:
  struct ClusterCommControl : arangodb::ClusterComm {
    static void reset() {
      arangodb::ClusterComm::_theInstanceInit.store(0);
    }
  };

  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  FlushFeatureTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::WARN);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::ENGINES.name(),
                                    arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(),
                                    arangodb::LogLevel::FATAL);

    features.emplace_back(new arangodb::AuthenticationFeature(server), false);  // required for ClusterFeature::prepare()
    features.emplace_back(new arangodb::ClusterFeature(server),
                          false);  // required for V8DealerFeature::prepare()
    features.emplace_back(arangodb::DatabaseFeature::DATABASE =
                              new arangodb::DatabaseFeature(server),
                          false);  // required for MMFilesWalRecoverState constructor
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);  // required for TRI_vocbase_t
    features.emplace_back(new arangodb::V8DealerFeature(server),
                          false);  // required for DatabaseFeature::createDatabase(...)

#if USE_ENTERPRISE
    features.emplace_back(new arangodb::LdapFeature(server),
                          false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first->start();
      }
    }
  }

  ~FlushFeatureTest() {
    arangodb::application_features::ApplicationServer::server = nullptr;

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f : features) {
      f.first->unprepare();
    }

    ClusterCommControl::reset();

    arangodb::LogTopic::setLogLevel(arangodb::Logger::ENGINES.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(FlushFeatureTest, test_subscription_retention) {
  struct TestFlushSubscripion : arangodb::FlushSubscription {
    TRI_voc_tick_t tick() const noexcept {
      return _tick;
    }

    TRI_voc_tick_t _tick{};
  };

  auto* dbFeature =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
          "Database");
  ASSERT_TRUE((dbFeature));
  TRI_vocbase_t* vocbase;
  ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testDatabase", vocbase)));
  ASSERT_NE(nullptr, vocbase);

  arangodb::FlushFeature feature(server);
  feature.prepare();

  {
    auto subscription = std::make_shared<TestFlushSubscripion>();
    auto const subscriptionTick = engine.currentTick();
    auto const currentTick = TRI_NewTickServer();
    ASSERT_EQ(currentTick, engine.currentTick());
    ASSERT_LT(subscriptionTick, engine.currentTick());
    subscription->_tick = subscriptionTick;

    {
      size_t removed = 42;
      TRI_voc_tick_t releasedTick = 42;
      feature.releaseUnusedTicks(removed, releasedTick);
      ASSERT_EQ(0, removed); // reference is being held
      ASSERT_EQ(0, releasedTick); // min tick released
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
      ASSERT_EQ(0, removed); // reference is being held
      ASSERT_EQ(subscriptionTick, releasedTick); // min tick released
    }
  }

  size_t removed = 42;
  TRI_voc_tick_t releasedTick = 42;
  feature.releaseUnusedTicks(removed, releasedTick);
  ASSERT_EQ(1, removed); // stale subscription was removed
  ASSERT_EQ(engine.currentTick(), releasedTick); // min tick released
}
