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

#include "Metrics/TelemetricsFeature.h"
#include "Mocks/Servers.h"
#include "Logger/LogMacros.h"

#include <chrono>
#include <thread>

using namespace arangodb;

class TelemetricsFeatureTest : public ::testing::Test {
 protected:
  TelemetricsFeatureTest() {}
};

struct MockTelemetricsSender : public arangodb::metrics::ITelemetricsSender {
  void send(velocypack::Builder& builder) override {
    velocypack::Slice msgSlice = builder.slice();
    ASSERT_FALSE(msgSlice.get("deployment").isNone());
    ASSERT_FALSE(msgSlice.get("type").isNone());
    ASSERT_FALSE(msgSlice.get("host").isNone());
    // assert for all fields that would be present in the support-info
  }
};

TEST_F(TelemetricsFeatureTest, test_log_telemetrics) {
  arangodb::tests::mocks::MockRestServer server(false);
  server.addFeature<metrics::TelemetricsFeature>(
      false, std::make_unique<MockTelemetricsSender>());
  //  auto feature = server.getFeature<metrics::TelemetricsFeature>();
  // TODO: make a copy constructor
  server.getFeature<metrics::TelemetricsFeature>().setInterval(
      10);  // 30 seconsd
  server.getFeature<metrics::TelemetricsFeature>().setRescheduleInterval(
      5);  // 10 seconds
  server.startFeatures();
  std::this_thread::sleep_for(std::chrono::seconds(15));
}
