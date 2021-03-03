////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_SERVER_METRICS_FEATURE_H
#define ARANGODB_REST_SERVER_METRICS_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/LoggerFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/Metrics.h"
#include "Statistics/ServerStatistics.h"

#define DECLARE_METRIC(x) constexpr char x[] = "x"

namespace arangodb {
struct metrics_key;
}

namespace std {
template <>
struct hash<arangodb::metrics_key> {
  typedef arangodb::metrics_key argument_t;
  typedef std::size_t result_t;
  result_t operator()(argument_t const& r) const noexcept;
};
}  // namespace std

namespace arangodb {

struct metrics_key {
  std::string name;
  std::string labels;
  std::size_t _hash;
  metrics_key() {}
  metrics_key(std::string const& name);
  metrics_key(std::string const& name, std::string const& labels);
  metrics_key(std::initializer_list<std::string> const& il);
  metrics_key(std::string const& name, std::initializer_list<std::string> const& il);
  std::size_t hash() const noexcept;
  bool operator==(metrics_key const& other) const;
};

class MetricsFeature final : public application_features::ApplicationFeature {
 public:
  using registry_type = std::unordered_map<metrics_key, std::shared_ptr<Metric>>;

  static double time();

  explicit MetricsFeature(application_features::ApplicationServer& server);

  bool exportAPI() const;
  bool exportReadWriteMetrics() const;

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;

  template <char const* name, typename Scale>
  Histogram<Scale>& histogram(Scale const& scale, std::string const& help = std::string()) {
    return histogram<name, Scale>(metrics_key(name), scale, help);
  }

  template <char const* name, typename Scale>
  Histogram<Scale>& histogram(std::initializer_list<std::string> const& il,
                              Scale const& scale,
                              std::string const& help = std::string()) {
    return histogram<name, Scale>(metrics_key(name, il), scale, help);
  }

  template <char const* name, typename Scale>
  Histogram<Scale>& histogram(metrics_key const& mk, Scale const& scale,
                              std::string const& help = std::string()) {
    std::string labels = mk.labels;
    if (ServerState::instance() != nullptr &&
        ServerState::instance()->getRole() != ServerState::ROLE_UNDEFINED) {
      if (!labels.empty()) {
        labels += ",";
      }
      labels +=
          "role=\"" + ServerState::roleToString(ServerState::instance()->getRole()) +
          "\",shortname=\"" + ServerState::instance()->getShortName() + "\"";
    }

    auto metric = std::make_shared<Histogram<Scale>>(scale, name, help, labels);
    bool success = false;
    {
      std::lock_guard<std::recursive_mutex> guard(_lock);
      success =
          _registry.try_emplace(mk, std::dynamic_pointer_cast<Metric>(metric)).second;
    }
    if (!success) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     std::string("histogram ") + name +
                                         " alredy exists");
    }
    return *metric;
  };

  template <char const* name, typename Scale>
  Histogram<Scale>& histogram(std::initializer_list<std::string> const& key) {
    metrics_key mk(name,key);
    std::shared_ptr<Histogram<Scale>> metric = nullptr;
    std::string error;
    {
      std::lock_guard<std::recursive_mutex> guard(_lock);
      registry_type::const_iterator it = _registry.find(mk);
      if (it == _registry.end()) {
        it = _registry.find(mk);
        if (it == _registry.end()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         std::string("No gauge booked as ") + name);
        }
        try {
          metric = std::dynamic_pointer_cast<Histogram<Scale>>(it->second);
        } catch (std::exception const& e) {
          error = std::string("Failed to retrieve histogram ") + name + ": " + e.what();
        }
        if (metric == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            std::string("Non matching scale classes for cloning ") + name);
        }
        return histogram<name>(mk, metric->scale(), metric->help());
      }

      try {
        metric = std::dynamic_pointer_cast<Histogram<Scale>>(it->second);
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

  template <char const* name, typename Scale>
  Histogram<Scale>& histogram() {
    return histogram<name,Scale>(std::initializer_list<std::string>{});
  }

  template<const char* name>
  Counter& counter(
    std::initializer_list<std::string> const& key, uint64_t const& val,
    std::string const& help) {
    return counter<name>(metrics_key(name, key), val, help);
  }

  template<const char* name>
  Counter& counter(
    metrics_key const& mk, uint64_t const& val, std::string const& help) {

    std::string labels = mk.labels;
    if (ServerState::instance() != nullptr &&
        ServerState::instance()->getRole() != ServerState::ROLE_UNDEFINED) {
      if (!labels.empty()) {
        labels += ",";
      }
      labels += "role=\"" + ServerState::roleToString(ServerState::instance()->getRole()) +
        "\",shortname=\"" + ServerState::instance()->getShortName() + "\"";
    }
    auto metric = std::make_shared<Counter>(val, name, help, labels);
    bool success = false;
    {
      std::lock_guard<std::recursive_mutex> guard(_lock);
      success = _registry.emplace(mk, std::dynamic_pointer_cast<Metric>(metric)).second;
    }
    if (!success) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, std::string("counter ") + name + " already exists");
    }
    return *metric;
  }

  template<const char* name>
  Counter& counter(uint64_t const& val, std::string const& help) {
    return counter<name>(metrics_key(name), val, help);
  }

  template<const char* name>
  Counter& counter() {
    return counter<name>({});
  }

  template<const char* name>
  Counter& counter(std::initializer_list<std::string> const& key) {
    metrics_key mk(name, key);
    std::shared_ptr<Counter> metric = nullptr;
    std::string error;
    {
      std::lock_guard<std::recursive_mutex> guard(_lock);
      registry_type::const_iterator it = _registry.find(mk);
      if (it == _registry.end()) {
        it = _registry.find(mk.name);
        if (it == _registry.end()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, std::string("No counter booked as ") + mk.name);
        } else {
          auto tmp = std::dynamic_pointer_cast<Counter>(it->second);
          return counter<name>(mk, 0, tmp->help());
        }
      }
      try {
        metric = std::dynamic_pointer_cast<Counter>(it->second);
        if (metric == nullptr) {
          error = std::string("Failed to retrieve counter ") + mk.name;
        }
      } catch (std::exception const& e) {
        error = std::string("Failed to retrieve counter ") + mk.name +  ": " + e.what();
      }
    }
    if (!error.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, error);
    }
    return *metric;
  }

  template <const char* name, typename T>
  Gauge<T>& gauge(T const& t, std::string const& help) {
    return gauge<name>(metrics_key(name), t, help);
  }

  template <const char* name, typename T>
  Gauge<T>& gauge(std::initializer_list<std::string> const& il, T const& t,
                  std::string const& help) {
    return gauge<name>(metrics_key(name, il), t, help);
  }

  template <const char* name, typename T>
  Gauge<T>& gauge(metrics_key const& key, T const& t,
                  std::string const& help = std::string()) {
    metrics_key mk(key);
    std::string labels = mk.labels;
    if (ServerState::instance() != nullptr &&
        ServerState::instance()->getRole() != ServerState::ROLE_UNDEFINED) {
      if (!labels.empty()) {
        labels += ",";
      }
      labels +=
          "role=\"" + ServerState::roleToString(ServerState::instance()->getRole()) +
          "\",shortname=\"" + ServerState::instance()->getShortName() + "\"";
    }
    auto metric = std::make_shared<Gauge<T>>(t, name, help, labels);
    bool success = false;
    {
      std::lock_guard<std::recursive_mutex> guard(_lock);
      success =
          _registry.try_emplace(mk, std::dynamic_pointer_cast<Metric>(metric)).second;
    }
    if (!success) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::string("gauge ") + name +
                                                             " alredy exists");
    }
    return *metric;
  }

  template <char const* name, typename T>
  Gauge<T>& gauge() {
    return gauge<name,T>(metrics_key(name));
  }

  template <char const* name, typename T>
  Gauge<T>& gauge(std::initializer_list<std::string> const& il) {
    return gauge<name,T>(metrics_key(name, il));
  }

  template <char const* name, typename T>
  Gauge<T>& gauge(metrics_key const& key) {
    metrics_key mk(key);
    std::shared_ptr<Gauge<T>> metric = nullptr;
    std::string error;
    {
      std::lock_guard<std::recursive_mutex> guard(_lock);
      registry_type::const_iterator it = _registry.find(mk);
      if (it == _registry.end()) {
        it = _registry.find(mk.name);
        if (it == _registry.end()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         std::string("No gauge booked as ") + mk.name);
        }
        try {
          metric = std::dynamic_pointer_cast<Gauge<T>>(it->second);
        } catch (std::exception const& e) {
          error = std::string("Failed to retrieve gauge ") + mk.name + ": " + e.what();
        }
        if (metric == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_INTERNAL,
              std::string("Non matching type for cloning ") + mk.name);
        }
        return gauge<name>(mk, T(0), metric->help());
      }

      try {
        metric = std::dynamic_pointer_cast<Gauge<T>>(it->second);
        if (metric == nullptr) {
          error = std::string("Failed to retrieve gauge ") + mk.name;
        }
      } catch (std::exception const& e) {
        error = std::string("Failed to retrieve gauge ") + mk.name + ": " + e.what();
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

  mutable std::recursive_mutex _lock;

  std::unique_ptr<ServerStatistics> _serverStatistics;

  bool _export;
  bool _exportReadWriteMetrics;
};

}  // namespace arangodb

#endif
