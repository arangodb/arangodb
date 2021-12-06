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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Metrics/Metric.h"

#include <mutex>

namespace arangodb::metrics {

template <typename T>
class Batch final : public Metric {
 public:
  Batch(T&& metric, std::string_view name, std::string_view help, std::string_view labels)
      : Metric{name, help, labels}, _metric{std::move(metric)} {}

  [[nodiscard]] std::string_view type() const noexcept final {
    return "untyped";
  }
  void toPrometheus(std::string& result, std::string_view globals, std::string_view) const final {
    load().toPrometheus(result, globals, labels());
  }
  void toPrometheusBegin(std::string& result, std::string_view name) const final {
    std::lock_guard guard{_m};
    _metric.needName();
  }

  void store(T&& metric) {
    std::lock_guard guard{_m};
    _metric = std::move(metric);
  }

 private:
  T load() const {
    std::lock_guard guard{_m};
    return _metric;
  }

  mutable std::mutex _m;
  T _metric;
};

}  // namespace arangodb::metrics
