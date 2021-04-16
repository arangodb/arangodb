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

#define DECLARE_COUNTER(x, help)                \
  struct x : arangodb::metrics::CounterBuilder<x> { \
    x() { _name = #x; _help = help; } \
    }

#define DECLARE_GAUGE(x, type, help)    \
  struct x : arangodb::metrics::GaugeBuilder<x, type> { \
    x() { _name = #x; _help = help; } \
    }
    
#define DECLARE_LEGACY_GAUGE(x, type, help) DECLARE_GAUGE(x, type, help)

#define DECLARE_HISTOGRAM(x, scale, help)                   \
  struct x : arangodb::metrics::HistogramBuilder<x, scale> { \
    x() { _name = #x; _help = help; } \
    }

// The following is only needed in 3.8 for the case of duplicate
// metrics which will be removed in a future version:
#define DECLARE_LEGACY_COUNTER(x, help)                \
  struct x : arangodb::metrics::CounterBuilder<x> { \
    x() { _name = #x; _help = help; } \
    }

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
  bool operator<(metrics_key const& other) const;
};

namespace metrics {

struct Builder {
  virtual ~Builder() = default;
  virtual char const* type() const = 0;
  virtual std::shared_ptr<::Metric> build() const = 0;

  std::string const& name() const { return _name; }
  metrics_key key() const { return metrics_key(name(), _labels); }

  void addLabel(std::string const& key, std::string const& value) {
    if (!_labels.empty()) {
      _labels.append(",");
    }
    _labels += key + "=\"" + value + "\"";
  }

 protected:
  std::string _name;
  std::string _help;
  std::string _labels;
};

template <class Derived>
struct GenericBuilder : Builder {
  Derived&& self() { return static_cast<Derived&&>(std::move(*this)); }

  Derived&& withLabel(std::string const& key, std::string const& value) && {
    Builder::addLabel(key, value);
    return self();
  }
};

template <class Derived>
struct CounterBuilder : GenericBuilder<Derived> {
  using metric_t = ::Counter;

  std::shared_ptr<::Metric> build() const override {
    return std::make_shared<::Counter>(0, this->name(), this->_help, this->_labels);
  }

  char const* type() const override { return "counter"; }
};

template <class Derived, typename T>
struct GaugeBuilder : GenericBuilder<Derived> {
  using metric_t = ::Gauge<T>;

  std::shared_ptr<::Metric> build() const override {
    return std::make_shared<::Gauge<T>>(T{}, this->name(), this->_help, this->_labels);
  }

  char const* type() const override { return "gauge"; }
};

template <class Derived, class Scale>
struct HistogramBuilder : GenericBuilder<Derived> {
  using metric_t = ::Histogram<decltype(Scale::scale())>;

  std::shared_ptr<::Metric> build() const override {
    return std::make_shared<metric_t>(Scale::scale(), this->name(), this->_help, this->_labels);
  }

  char const* type() const override { return "histogram"; }
};
}  // namespace metrics

class MetricsFeature final : public application_features::ApplicationFeature {
 public:
  using registry_type = std::map<metrics_key, std::shared_ptr<Metric>>;

  static double time();

  explicit MetricsFeature(application_features::ApplicationServer& server);

  bool exportAPI() const;
  bool exportReadWriteMetrics() const;

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;

  template <typename MetricBuilder>
  auto& add(MetricBuilder&& builder) {
    return static_cast<typename MetricBuilder::metric_t&>(doAdd(builder));
  }

  void toPrometheus(std::string& result, bool V2) const;

  ServerStatistics& serverStatistics();

 private:
  Metric& doAdd(metrics::Builder& builder);


  registry_type _registry;

  mutable std::unordered_map<std::string,std::string> _globalLabels;
  mutable std::string _globalLabelsStr;

  mutable std::recursive_mutex _lock;

  std::unique_ptr<ServerStatistics> _serverStatistics;

  bool _export;
  bool _exportReadWriteMetrics;

  std::unordered_map<std::string, std::string> nameVersionTable;
  std::unordered_set<std::string> v2suppressions;
  std::unordered_set<std::string> v1suppressions;
};

}  // namespace arangodb

#endif
