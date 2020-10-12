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

  auto& counter = feature.counter("counter", 0, "one counter");
  auto& labeledCounter = feature.counter({"counter", "label=\"label\""}, 0, "another counter");

  ASSERT_DOUBLE_EQ(counter.load(), 0);
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
    auto& counterFail = feature.counter({"counter"}, 0, "one counter");
    ASSERT_TRUE(false);
    std::cout << counterFail.name() << std::endl;
  } catch (...) {
    ASSERT_TRUE(true);
  }
}


TEST_F(MetricsFeatureTest, test_same_counter_retrieve) {

  auto& counter1 = feature.counter("counter");
  ASSERT_EQ(&counter1, thisMetric);

  auto& counter2 = feature.counter({"counter"});
  ASSERT_EQ(&counter2, thisMetric);

  auto& counter3 = feature.counter({"counter", "label=\"label\""});
  std::string s;
  counter3.toPrometheus(s);
  std::cout << s << std::endl;
  ASSERT_EQ(&counter3, thatMetric);

  auto& counter4 = feature.counter({"counter", "label=\"other_label\""});
  s.clear();
  counter4.toPrometheus(s);
  std::cout << s << std::endl;

}

TEST_F(MetricsFeatureTest, test_histogram) {

  auto& histogram = feature.histogram("hist", lin_scale_t(0.,1.,10), "linear histogram");
  auto& labeledHistogram = feature.histogram(
    {"hist", "label=\"label\""}, log_scale_t(2.,0.,1.,10), "labeled logarithmic histogram");

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
    auto& histogramFail = feature.histogram({"hist"}, lin_scale_t(0.,1.,10), "linear histogram");
    ASSERT_TRUE(false);
    std::cout << histogramFail << std::endl;
  } catch (...) {
    ASSERT_TRUE(true);
  }
}


TEST_F(MetricsFeatureTest, test_same_histogram_retrieve) {

  auto& histogram1 = feature.histogram<lin_scale_t<double>>("hist");
  ASSERT_EQ(&histogram1, thisMetric);

  try {
    auto& histogramFail = feature.histogram<lin_scale_t<float>>("hist");
    ASSERT_TRUE(false);
    std::cout << histogramFail << std::endl;
  } catch (...) {
    ASSERT_TRUE(true);
  }

  auto& histogram2 = feature.histogram<lin_scale_t<double>>({"hist"});
  ASSERT_EQ(&histogram2, thisMetric);

  auto& histogram3 = feature.histogram<log_scale_t<double>>({"hist", "label=\"label\""});
  std::string s;
  histogram3.toPrometheus(s);
  std::cout << s << std::endl;
  ASSERT_EQ(&histogram3, thatMetric);


  auto& histogram4 = feature.histogram<lin_scale_t<double>>({"hist", "label=\"other_label\""});
  s.clear();
  histogram4.toPrometheus(s);
  std::cout << s << std::endl;

}


TEST_F(MetricsFeatureTest, test_gauge) {

  auto& gauge = feature.gauge("gauge", 2.3, "one gauge");
  auto& labeledGauge = feature.gauge(
    {"gauge", "label=\"label\""}, 17., "labeled gauge");

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

  auto& gauge1 = feature.gauge<double>("gauge");
  ASSERT_EQ(&gauge1, thisMetric);

  try {
    auto& gaugeFail = feature.gauge<float>("gauge");
    ASSERT_TRUE(false);
    std::cout << gaugeFail.name() << std::endl;
  } catch (...) {
    ASSERT_TRUE(true);
  }

  auto& gauge2 = feature.gauge<double>({"gauge"});
  ASSERT_EQ(&gauge2, thisMetric);

  auto& gauge3 = feature.gauge<double>({"gauge", "label=\"label\""});
  std::string s;
  gauge3.toPrometheus(s);
  std::cout << s << std::endl;
  ASSERT_EQ(&gauge3, thatMetric);


  auto& gauge4 = feature.gauge<double>({"gauge", "label=\"other_label\""});
  s.clear();
  gauge4.toPrometheus(s);
  std::cout << s << std::endl;

}
