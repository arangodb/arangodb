////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "Metrics/Gauge.h"

namespace arangodb::metrics {

template<typename Derived, typename T>
class GaugeBuilder : public GenericBuilder<Derived> {
 public:
  using MetricT = std::conditional_t<std::is_base_of_v<Metric, T>, T, Gauge<T>>;
  using MetricV = typename MetricT::Value;

  [[nodiscard]] std::string_view type() const noexcept final { return "gauge"; }
  [[nodiscard]] std::shared_ptr<Metric> build() const final {
    return std::make_shared<MetricT>(MetricV{}, this->_name, this->_help,
                                     this->_labels);
  }
};

}  // namespace arangodb::metrics

#define DECLARE_GAUGE(x, type, help)                    \
  struct x : arangodb::metrics::GaugeBuilder<x, type> { \
    x() {                                               \
      _name = #x;                                       \
      _help = help;                                     \
    }                                                   \
  }
