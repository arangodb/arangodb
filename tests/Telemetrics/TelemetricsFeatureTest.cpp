////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include <gmock/gmock.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Metrics/TelemetricsFeature.h"
#include "Mocks/Servers.h"

using namespace arangodb;
using cast = std::chrono::duration<std::uint64_t>;

class TelemetricsFeatureTest : public ::testing::Test {
 protected:
  TelemetricsFeatureTest() {}
};

struct MockTelemetricsSender : public metrics::ITelemetricsSender {
  MOCK_METHOD(void, send, (arangodb::velocypack::Slice), (const, override));
};

struct MockLastUpdateHandler : public metrics::LastUpdateHandler {
  MockLastUpdateHandler(ArangodServer& server, uint64_t prepareDeadline)
      : metrics::LastUpdateHandler(server, prepareDeadline) {}
  MOCK_METHOD(bool, handleLastUpdatePersistance,
              (bool, std::string&, uint64_t&, uint64_t), (override));
  MOCK_METHOD(void, sendTelemetrics, (), (override));
};

TEST_F(TelemetricsFeatureTest, test_log_telemetrics) {
  auto sender = std::make_unique<MockTelemetricsSender>();

  ON_CALL(*sender, send).WillByDefault([&](velocypack::Slice result) {
    ASSERT_FALSE(result.get("OK").isNone());
    ASSERT_EQ(result.get("OK").getBoolean(), true);
  });
  EXPECT_CALL(*sender, send).Times(7);

  arangodb::tests::mocks::MockRestServer server(false);

  auto updateHandler =
      std::make_unique<MockLastUpdateHandler>(server.server(), 30);

  ON_CALL(*updateHandler, handleLastUpdatePersistance)
      .WillByDefault([&](bool isCoordinator, std::string& oldRev,
                         uint64_t& lastUpdate, uint64_t interval) {
        std::uint64_t rightNowSecs =
            std::chrono::duration_cast<cast>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();
        if (rightNowSecs - lastUpdate >= interval) {
          lastUpdate = rightNowSecs;
          updateHandler->sendTelemetrics();
          return true;
        } else {
          return false;
        }
      });

  ON_CALL(*updateHandler, sendTelemetrics).WillByDefault([&]() {
    VPackBuilder result;
    result.openObject();
    result.add("OK", VPackValue(true));
    result.close();
    updateHandler->getSender()->send(result.slice());
  });

  EXPECT_CALL(*updateHandler, sendTelemetrics).Times(7);

  updateHandler->setTelemetricsSender(std::move(sender));

  uint64_t lastUpdate = 0;
  std::string mockOldRev = "abc";
  uint64_t maxValue = std::numeric_limits<uint64_t>::max();
  ASSERT_TRUE(updateHandler->handleLastUpdatePersistance(false, mockOldRev,
                                                         lastUpdate, 5));
  ASSERT_FALSE(updateHandler->handleLastUpdatePersistance(false, mockOldRev,
                                                          lastUpdate, 5));
  ASSERT_FALSE(updateHandler->handleLastUpdatePersistance(false, mockOldRev,
                                                          lastUpdate, 5));
  ASSERT_TRUE(updateHandler->handleLastUpdatePersistance(false, mockOldRev,
                                                         lastUpdate, 0));
  lastUpdate = 128;
  ASSERT_TRUE(updateHandler->handleLastUpdatePersistance(false, mockOldRev,
                                                         lastUpdate, 12));
  lastUpdate = 1;

  ASSERT_TRUE(updateHandler->handleLastUpdatePersistance(false, mockOldRev,
                                                         lastUpdate, 1));
  ASSERT_FALSE(updateHandler->handleLastUpdatePersistance(
      false, mockOldRev, lastUpdate, maxValue));
  lastUpdate = std::chrono::duration_cast<cast>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();
  ASSERT_TRUE(updateHandler->handleLastUpdatePersistance(false, mockOldRev,
                                                         lastUpdate, 0));
  ASSERT_FALSE(updateHandler->handleLastUpdatePersistance(false, mockOldRev,
                                                          lastUpdate, 1));
  lastUpdate += 10;
  ASSERT_TRUE(updateHandler->handleLastUpdatePersistance(false, mockOldRev,
                                                         lastUpdate, 10));
  lastUpdate = std::chrono::duration_cast<cast>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count() -
               10;
  ASSERT_TRUE(updateHandler->handleLastUpdatePersistance(false, mockOldRev,
                                                         lastUpdate, 10));
  lastUpdate = std::chrono::duration_cast<cast>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count() -
               10;
  ASSERT_FALSE(updateHandler->handleLastUpdatePersistance(false, mockOldRev,
                                                          lastUpdate, 11));
  lastUpdate = std::chrono::duration_cast<cast>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count() -
               10;
  ASSERT_FALSE(updateHandler->handleLastUpdatePersistance(
      false, mockOldRev, lastUpdate, maxValue));
}
