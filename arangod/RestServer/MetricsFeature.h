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

  using registry_type = std::unordered_map<std::string, std::shared_ptr<Metric>>;

  static double time();

  explicit MetricsFeature(application_features::ApplicationServer& server);

  bool exportAPI() const;

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;



  template<typename T> Histogram<T>&
  histogram (std::string const& name, size_t const& buckets, T const& low,
             T const& high, std::string const& help = std::string()) {

    auto metric = std::make_shared<Histogram<T>>(buckets, low, high, name, help);
    bool success = false;
    {
      std::lock_guard<std::mutex> guard(_lock);
      success = _registry.try_emplace(
        name, std::dynamic_pointer_cast<Metric>(metric)).second;
    }
    if (!success) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, std::string("histogram ") + name + " alredy exists");
    }
    return *metric;

  };

  template<typename T> Histogram<T>& histogram (std::string const& name) {

    std::shared_ptr<Histogram<T>> metric = nullptr;
    std::string error;
    {
      std::lock_guard<std::mutex> guard(_lock);
      registry_type::const_iterator it = _registry.find(name);

      try {
        metric = std::dynamic_pointer_cast<Histogram<T>>(*it->second);
        if (metric == nullptr) {
          error = std::string("Failed to retrieve histogram ") + name;
        }
      } catch (std::exception const& e) {
        error = std::string("Failed to retrieve histogram ") + name + ": " + e.what();
      }
    }
    if (!error.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, error);
    }
    return *metric;

  }

  Counter& counter(std::string const& name, uint64_t const& val, std::string const& help);

  template<typename T>
  Gauge<T>& gauge(std::string const& name, T const& t, std::string const& help) {
    auto metric = std::make_shared<Gauge<T>>(t, name, help);
    bool success = false;
    {
      std::lock_guard<std::mutex> guard(_lock);
      success = _registry.try_emplace(
        name, std::dynamic_pointer_cast<Metric>(metric)).second;
    }
    if (!success) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, std::string("gauge ") + name + " alredy exists");
    }
    return *metric;
  }

  template<typename T> Gauge<T>& gauge(std::string const& name) {

    std::shared_ptr<Gauge<T>> metric = nullptr;
    std::string error;
    {
      std::lock_guard<std::mutex> guard(_lock);
      registry_type::const_iterator it = _registry.find(name);
      if (it == _registry.end()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, std::string("No gauge booked as ") + name);
      }
      try {
        metric = std::dynamic_pointer_cast<Gauge<T>>(*it->second);
        if (metric == nullptr) {
          error = std::string("Failed to retrieve gauge ") + name;
        }
      } catch (std::exception const& e) {
        error = std::string("Failed to retrieve gauge ") + name +  ": " + e.what();
      }
    }
    if (!error.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, error);
    }
    return *metric;
  }

  void toPrometheus(std::string& result) const;

  ServerStatistics& serverStatistics();

 private:
  registry_type _registry;

  mutable std::mutex _lock;

  std::unique_ptr<ServerStatistics> _serverStatistics;

  bool _export;

};

}  // namespace arangodb

#endif
