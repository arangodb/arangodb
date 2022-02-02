////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#pragma once

#include "Metrics/Builder.h"
#include "Metrics/Metric.h"
#include "Metrics/MetricKey.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/arangod.h"
#include "Statistics/ServerStatistics.h"

#include <map>
#include <shared_mutex>

namespace arangodb::metrics {

class MetricsFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Metrics"; }

  explicit MetricsFeature(Server& server);

  bool exportAPI() const;

  void collectOptions(std::shared_ptr<options::ProgramOptions>) final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) final;

  template<typename MetricBuilder>
  auto add(MetricBuilder&& builder) -> typename MetricBuilder::MetricT& {
    return static_cast<typename MetricBuilder::MetricT&>(*doAdd(builder));
  }
  template<typename MetricBuilder>
  auto addShared(MetricBuilder&& builder)  // TODO(MBkkt) Remove this method
      -> std::shared_ptr<typename MetricBuilder::MetricT> {
    return std::static_pointer_cast<typename MetricBuilder::MetricT>(
        doAdd(builder));
  }
  Metric* get(MetricKey const& key);
  bool remove(Builder const& builder);

  void toPrometheus(std::string& result) const;

  ServerStatistics& serverStatistics() noexcept;

 private:
  std::shared_ptr<Metric> doAdd(Builder& builder);
  void initGlobalLabels() const;

  // TODO(MBkkt) abseil btree map? or hashmap<name, hashmap<labels, Metric>>?
  std::map<MetricKey, std::shared_ptr<Metric>> _registry;

  mutable bool hasShortname = false;
  mutable bool hasRole = false;
  mutable std::string _globals;

  mutable std::shared_mutex _mutex;

  std::unique_ptr<ServerStatistics> _serverStatistics;

  bool _export;
  bool _exportReadWriteMetrics;
};

}  // namespace arangodb::metrics
