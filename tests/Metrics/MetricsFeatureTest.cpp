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
  auto& labeledCounter = feature.add(COUNTER{}.withLabels("label=\"label\""));

  ASSERT_EQ(counter.load(), 0);
  std::string s;
  counter.toPrometheus(s);
  std::cout << s << std::endl;
  s.clear();
  labeledCounter.toPrometheus(s);
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


TEST_F(MetricsFeatureTest, test_same_counter_retrieve) {

  auto& counter1 = feature.add(COUNTER{});
  ASSERT_EQ(&counter1, thisMetric);

  auto& counter2 = feature.add(COUNTER{});
  ASSERT_EQ(&counter2, thisMetric);

  auto& counter3 = feature.add(COUNTER{}.withLabels("label=\"label\""));
  std::string s;
  counter3.toPrometheus(s);
  std::cout << s << std::endl;
  ASSERT_EQ(&counter3, thatMetric);

  auto& counter4 = feature.add(COUNTER{}.withLabels("label=\"other_label\""));
  s.clear();
  counter4.toPrometheus(s);
  std::cout << s << std::endl;

}

TEST_F(MetricsFeatureTest, test_histogram) {

  auto& histogram = feature.add(HISTOGRAMLIN{});
  auto& labeledHistogram = feature.add(HISTOGRAMLIN{}.withLabels("label=\"label\""));

  std::string s;
  histogram.toPrometheus(s);
  std::cout << s << std::endl;
  s.clear();
  labeledHistogram.toPrometheus(s);
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


TEST_F(MetricsFeatureTest, test_same_histogram_retrieve) {

  auto& histogram1 = feature.add(HISTOGRAMLIN{});
  ASSERT_EQ(&histogram1, thisMetric);

  try {
    auto& histogramFail = feature.add(HISTOGRAMLIN{});
    ASSERT_TRUE(false);
    std::cout << histogramFail << std::endl;
  } catch (...) {
    ASSERT_TRUE(true);
  }

  auto& histogram2 = feature.add(HISTOGRAMLIN{});
  ASSERT_EQ(&histogram2, thisMetric);

  auto& histogram3 = feature.add(HISTOGRAMLIN{});
  std::string s;
  histogram3.toPrometheus(s);
  std::cout << s << std::endl;
  ASSERT_EQ(&histogram3, thatMetric);

}

TEST_F(MetricsFeatureTest, test_gauge) {

  auto& gauge = feature.add(GAUGE{});
  auto& labeledGauge = feature.add(GAUGE{}.withLabels("label=\"label\""));

  std::string s;
  gauge.toPrometheus(s);
  std::cout << s << std::endl;
  s.clear();
  labeledGauge.toPrometheus(s);
  std::cout << s << std::endl;

  thisMetric = &gauge;
  thatMetric = &labeledGauge;

}


TEST_F(MetricsFeatureTest, test_same_gauge_retrieve) {

  auto& gauge1 = feature.add(GAUGE{});
  ASSERT_EQ(&gauge1, thisMetric);

  try {
    auto& gaugeFail = feature.add(GAUGE{});
    ASSERT_TRUE(false);
    std::cout << gaugeFail.name() << std::endl;
  } catch (...) {
    ASSERT_TRUE(true);
  }

  auto& gauge2 = feature.add(GAUGE{});
  ASSERT_EQ(&gauge2, thisMetric);

  auto& gauge3 = feature.add(GAUGE{}.withLabels("label=\"label\""));
  std::string s;
  gauge3.toPrometheus(s);
  std::cout << s << std::endl;
  ASSERT_EQ(&gauge3, thatMetric);

  auto& gauge4 = feature.add(GAUGE{}.withLabels("label=\"other_label\""));
  s.clear();
  gauge4.toPrometheus(s);
  std::cout << s << std::endl;

}
