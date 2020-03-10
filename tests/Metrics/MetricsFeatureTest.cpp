////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for metrics
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2017-2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "RestServer/MetricsFeature.h"

using namespace arangodb;

auto opts = std::make_shared<arangodb::options::ProgramOptions>(
  "metrics_feature_test", std::string(), std::string(), "path");
application_features::ApplicationServer server(opts, nullptr);
MetricsFeature feature(server);

class MetricsFeatureTest : public ::testing::Test {
protected:
  MetricsFeatureTest () {}
};

Counter *thisCounter, *thatCounter;

TEST_F(MetricsFeatureTest, test_counter) {

  auto& counter = feature.counter("counter", 0, "one counter");
  auto& labeledCounter = feature.counter({"counter", "label=label"}, 0, "another counter");

  ASSERT_DOUBLE_EQ(counter.load(), 0);
  std::string s;
  counter.toPrometheus(s);
  std::cout << s << std::endl;
  s.clear();
  labeledCounter.toPrometheus(s);
  std::cout << s << std::endl;

  thisCounter = &counter;
  thatCounter = &labeledCounter;

}


TEST_F(MetricsFeatureTest, fail_recreate_counter) {
  try { 
    auto& counterFail = feature.counter({"counter"}, 0, "one counter");
    ASSERT_TRUE(false);
    LOG_DEVEL << counterFail.load();
  } catch (...) {
    ASSERT_TRUE(true);
  }
}


TEST_F(MetricsFeatureTest, test_same_counter_retrieve) {

  auto& counter1 = feature.counter("counter");
  ASSERT_EQ(&counter1, thisCounter);
  
  auto& counter2 = feature.counter({"counter"});
  ASSERT_EQ(&counter2, thisCounter);
  
  auto& counter3 = feature.counter({"counter", "label=label"});
  std::string s;
  counter3.toPrometheus(s);
  std::cout << s << std::endl;
  ASSERT_EQ(&counter3, thatCounter);
  
}
