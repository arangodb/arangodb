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
/// @author Kaveh Vahedipour
/// @author Copyright 2017-2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "RestServer/MetricsFeature.h"
#include "MetricsFeatureTest.h"

using namespace arangodb;

auto opts = std::make_shared<arangodb::options::ProgramOptions>(
  "metrics_feature_test", std::string(), std::string(), "path");
application_features::ApplicationServer server(opts, nullptr);
MetricsFeature feature(server);

class MetricsFeatureTest : public ::testing::Test {
protected:
  MetricsFeatureTest() {}
};

Metric* thisMetric;
Metric* thatMetric;

TEST_F(MetricsFeatureTest, test_counter) {

  auto& counter = feature.add(COUNTER{});
  auto& labeledCounter = feature.add(COUNTER{}.withLabel("label", "label"));

  ASSERT_EQ(counter.load(), 0);
  std::string s;
  counter.toPrometheus(s, "", "");
  std::cout << s << std::endl;
  s.clear();
  labeledCounter.toPrometheus(s, "", "");
  std::cout << s << std::endl;

  thisMetric = &counter;
  thatMetric = &labeledCounter;

}


TEST_F(MetricsFeatureTest, fail_recreate_counter) {
  try {
    auto& counterFail = feature.add(COUNTER{});
    ASSERT_TRUE(false);
    std::cout << counterFail.name() << std::endl;
  } catch (...) {
    ASSERT_TRUE(true);
  }
}


TEST_F(MetricsFeatureTest, test_histogram) {

  auto& histogram = feature.add(HISTOGRAMLIN{});
  auto& labeledHistogram = feature.add(HISTOGRAMLIN{}.withLabel("label", "label"));

  std::string s;
  histogram.toPrometheus(s, "", "");
  std::cout << s << std::endl;
  s.clear();
  labeledHistogram.toPrometheus(s, "", "");
  std::cout << s << std::endl;

  thisMetric = &histogram;
  thatMetric = &labeledHistogram;

}


TEST_F(MetricsFeatureTest, fail_recreate_histogram) {
  try {
    auto& histogramFail = feature.add(HISTOGRAMLIN{});
    ASSERT_TRUE(false);
    std::cout << histogramFail << std::endl;
  } catch (...) {
    ASSERT_TRUE(true);
  }
}


TEST_F(MetricsFeatureTest, test_gauge) {

  auto& gauge = feature.add(GAUGE{});
  auto& labeledGauge = feature.add(GAUGE{}.withLabel("label", "label"));

  std::string s;
  gauge.toPrometheus(s, "", "");
  std::cout << s << std::endl;
  s.clear();
  labeledGauge.toPrometheus(s, "", "");
  std::cout << s << std::endl;

  thisMetric = &gauge;
  thatMetric = &labeledGauge;

}


