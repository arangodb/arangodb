////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Metrics/Counter.h"

namespace arangodb::metrics {

template<typename Derived>
class CounterBuilder : public GenericBuilder<Derived> {
 public:
  using MetricT = Counter;

  [[nodiscard]] std::string_view type() const noexcept final {
    return "counter";
  }

  [[nodiscard]] std::shared_ptr<Metric> build() const final {
    return std::make_shared<MetricT>(0, this->_name, this->_help,
                                     this->_labels);
  }
};

}  // namespace arangodb::metrics

#define DECLARE_COUNTER(name, help)                       \
  struct name : arangodb::metrics::CounterBuilder<name> { \
    name() {                                              \
      _name = #name;                                      \
      _help = help;                                       \
    }                                                     \
  }
