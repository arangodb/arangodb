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

  auto& counter = feature.counter<COUNTER>(0, "one counter");
  auto& labeledCounter = feature.counter<COUNTER>({"label=\"label\""}, 0, "another counter");

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
    auto& counterFail = feature.counter<COUNTER>({}, 0, "one counter");
    ASSERT_TRUE(false);
    std::cout << counterFail.name() << std::endl;
  } catch (...) {
    ASSERT_TRUE(true);
  }
}


TEST_F(MetricsFeatureTest, test_histogram) {

  auto& histogram = feature.histogram<HISTOGRAM>(lin_scale_t(0.,1.,10), "linear histogram");
  auto& labeledHistogram = feature.histogram<HISTOGRAM>(
    {"label=\"label\""}, log_scale_t(2.,0.,1.,10), "labeled logarithmic histogram");

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
    auto& histogramFail = feature.histogram<HISTOGRAM>(lin_scale_t(0.,1.,10), "linear histogram");
    ASSERT_TRUE(false);
    std::cout << histogramFail << std::endl;
  } catch (...) {
    ASSERT_TRUE(true);
  }
}


TEST_F(MetricsFeatureTest, test_gauge) {

  auto& gauge = feature.gauge<GAUGE>(2.3, "one gauge");
  auto& labeledGauge = feature.gauge<GAUGE>(
    {"label=\"label\""}, 17., "labeled gauge");

  std::string s;
  gauge.toPrometheus(s);
  std::cout << s << std::endl;
  s.clear();
  labeledGauge.toPrometheus(s);
  std::cout << s << std::endl;

  thisMetric = &gauge;
  thatMetric = &labeledGauge;

}


