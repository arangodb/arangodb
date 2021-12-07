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
#pragma once

#include "Metrics/Builder.h"
#include "Metrics/Histogram.h"

namespace arangodb::metrics {

template <typename Derived, typename Scale>
class HistogramBuilder : public GenericBuilder<Derived> {
 public:
  using MetricT = Histogram<decltype(Scale::scale())>;
  [[nodiscard]] std::string_view type() const noexcept final {
    return "histogram";
  }
  [[nodiscard]] std::shared_ptr<Metric> build() const final {
    return std::make_shared<MetricT>(Scale::scale(), this->_name, this->_help, this->_labels);
  }
};

}  // namespace arangodb::metrics

#define DECLARE_HISTOGRAM(name, scale, help)                       \
  struct name : arangodb::metrics::HistogramBuilder<name, scale> { \
    name() {                                                       \
      _name = #name;                                               \
      _help = help;                                                \
    }                                                              \
  }
