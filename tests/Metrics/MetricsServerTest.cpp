////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2017-2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Metrics/MetricsFeature.h"
#include "Mocks/Servers.h"
#include "MetricsFeatureTest.h"

using namespace arangodb;

TEST(MetricsServerTest, test_setup) {
  tests::mocks::MockMetricsServer server;
  auto& feature = server.getFeature<metrics::MetricsFeature>();

  auto& counter = feature.add(COUNTER{});
  ASSERT_EQ(counter.load(), 0);
  counter.count();
  ASSERT_EQ(counter.load(), 1);
}
