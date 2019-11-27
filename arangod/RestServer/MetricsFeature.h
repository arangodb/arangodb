////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
  void unprepare() override final;

  template<typename T> Histogram<T>&
  histogram (std::string const& name, size_t const& buckets, T const& low,
             T const& high, std::string const& help = std::string()) {
    
    std::lock_guard<std::mutex> guard(_lock);
    auto const it = _registry.find(name);
    if (it != _registry.end()) {
      LOG_TOPIC("32e85", ERR, Logger::STATISTICS) << "histogram "  << name << " alredy exists";
      TRI_ASSERT(false);
    }
    auto h = std::make_shared<Histogram<T>>(buckets, low, high, name, help);
    _registry.emplace(name, std::dynamic_pointer_cast<Metric>(h));
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
    std::shared_ptr<Histogram<T>> h = nullptr;
    try {
      h = std::dynamic_pointer_cast<Histogram<T>>(*it->second);
      if (h == nullptr) {
        LOG_TOPIC("d2358", ERR, Logger::STATISTICS) << "Failed to retrieve histogram " << name;
      }
    } catch (std::exception const& e) {
      LOG_TOPIC("32d75", ERR, Logger::STATISTICS)
        << "Failed to retrieve histogram " << name << ": " << e.what();
    }
    if (h == nullptr) {
      TRI_ASSERT(false);
    }

    return *h;
  };

  Counter& counter(std::string const& name, uint64_t const& val, std::string const& help);
  Counter& counter(std::string const& name);

  template<typename T>
  Gauge<T>& gauge(std::string const& name, T const& t, std::string const& help) {
    std::lock_guard<std::mutex> guard(_lock);
    auto it = _registry.find(name);
    if (it != _registry.end()) {
      LOG_TOPIC("c7b37", ERR, Logger::STATISTICS) << "gauge "  << name << " alredy exists";
      TRI_ASSERT(false);
      throw(std::exception());
    }
    auto g = std::make_shared<Gauge>(t, name, help);
    _registry.emplace(name, std::dynamic_pointer_cast<Metric>(g));
    return *g;
  }

  template<typename T> Gauge<T>& gauge(std::string const& name) {
    std::lock_guard<std::mutex> guard(_lock);
    auto it = _registry.find(name);
    if (it == _registry.end()) {
      LOG_TOPIC("5832d", ERR, Logger::STATISTICS) << "No metric booked as " << name;
      TRI_ASSERT(false);
      throw std::exception();
    }
    std::shared_ptr<Gauge<T>> g = nullptr;
    try {
      g = std::dynamic_pointer_cast<Gauge<T>>(it->second);
      if (g == nullptr) {
        LOG_TOPIC("d4368", ERR, Logger::STATISTICS) << "Failed to retrieve gauge metric " << name;
        TRI_ASSERT(false);
        throw std::exception();
      }
    } catch (std::exception const& e) {
      LOG_TOPIC("c2348", ERR, Logger::STATISTICS)
        << "Failed to retrieve gauge metric " << name << ": " << e.what();
      TRI_ASSERT(false);
      throw(e);
    }
  }

  void toPrometheus(std::string& result) const;

  static MetricsFeature* metrics() {
    return METRICS;
  }

  ServerStatistics& serverStatistics();
  
 private:
  static MetricsFeature* METRICS;

  bool _enabled;
  
  std::unordered_map<std::string, std::shared_ptr<Metric>> _registry;

  mutable std::mutex _lock;

  ServerStatistics* _serverStatistics;
  
};

}  // namespace arangodb

#endif 
