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
#include "Logger/LogMacros.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/Metrics.h"
#include "Statistics/ServerStatistics.h"

#include <any>

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

  template<typename T> Histogram<T>&
  histogram (std::string const& name, size_t const& buckets, T const& low,
             T const& high, std::string const& help = std::string()) {
    
    std::lock_guard<std::mutex> guard(_lock);
    auto const it = _registry.find(name);
    if (it != _registry.end()) {
      LOG_TOPIC("32e85", ERR, Logger::STATISTICS) << "histogram "  << name << " alredy exists";
      TRI_ASSERT(false);
    }
    auto h = std::make_shared<Histogram<T>>(buckets, low, high, help);
    _registry.emplace(name, std::dynamic_pointer_cast<HistBase>(h));
    return *h;
  };

  template<typename T> Histogram<T>& histogram (std::string const& name) {
    std::lock_guard<std::mutex> guard(_lock);
    auto const it = _registry.find(name);
    if (it == _registry.end()) {
      LOG_TOPIC("32d85", ERR, Logger::STATISTICS) << "No histogram booked as " << name;
      TRI_ASSERT(false);
      throw std::exception();
    } 
    std::shared_ptr<Histogram<T>> h;
    try {
      h = std::dynamic_pointer_cast<Histogram<T>>(*it->second);
    } catch (std::exception const& e) {
      LOG_TOPIC("8532d", ERR, Logger::STATISTICS) << "Failed to retrieve histogram " << name;
      TRI_ASSERT(false);
    }
    return *h;
  };

  Counter& counter(std::string const& name, uint64_t const& val, std::string const& help);
  Counter& counter(std::string const& name);

  void toPrometheus(std::string& result) const;

  static MetricsFeature* metrics() {
    return METRICS;
  }

  ServerStatistics& serverStatistics();
  
 private:
  static MetricsFeature* METRICS;

  bool _enabled;
  
  std::unordered_map<std::string, std::shared_ptr<HistBase>> _registry;

  mutable std::mutex _lock;

  ServerStatistics* _serverStatistics;
  
};

}  // namespace arangodb

#endif 
