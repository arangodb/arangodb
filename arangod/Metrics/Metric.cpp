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

#include "Metrics/Metric.h"

#include <ostream>

namespace arangodb::metrics {

Metric::Metric(std::string_view name, std::string_view help,
               std::string_view labels) noexcept
    : _name{name}, _help{help}, _labels{labels} {}

std::string_view Metric::help() const noexcept { return _help; }

std::string_view Metric::name() const noexcept { return _name; }

std::string_view Metric::labels() const noexcept { return _labels; }

void Metric::toPrometheusBegin(std::string& result,
                               std::string_view name) const {
  result.append("# HELP ").append(name).append(" ").append(help()).append("\n");
  result.append("# TYPE ").append(name).append(" ").append(type()).append("\n");
}

Metric::~Metric() = default;

std::ostream& operator<<(std::ostream& output, CounterType const& s) {
  return output << s.load();
}

std::ostream& operator<<(std::ostream& output, HistType const& v) {
  output << '[';
  for (size_t i = 0; i < v.size(); ++i) {
    if (i > 0) {
      output << ", ";
    }
    output << v.load(i);
  }
  return output << ']';
}

}  // namespace arangodb::metrics
