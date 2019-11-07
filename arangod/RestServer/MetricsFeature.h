////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_SERVER_METRICS_FEATURE_H
#define ARANGODB_REST_SERVER_METRICS_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Logger/LoggerFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/Metrics.h"

namespace arangodb {

class MetricsFeature final : public application_features::ApplicationFeature {
 public:
  bool enabled() const {
    return _enabled;
  }

  static double time();

  explicit MetricsFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void stop() override final;

  template<typename T> Histogram<T>
  histogram (std::string const& name, size_t buckets, T low, T high) {
    return _metrics.registerHistogram(name, buckets, low, high);
  };

  Counter counter(std::string const& name);
  
 private:
  static MetricsFeature* METRICS;

  Metrics _metrics;
  bool _enabled;
  

};

}  // namespace arangodb

#endif 
