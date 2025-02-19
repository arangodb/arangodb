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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "Metrics/Histogram.h"
#include "Metrics/Gauge.h"
#include "Metrics/LogScale.h"
#include "MetricsHelper.h"

using namespace arangodb::replication2;

MeasureTimeGuard::MeasureTimeGuard(
    metrics::Histogram<metrics::LogScale<std::uint64_t>>& histogram) noexcept
    : _start(std::chrono::steady_clock::now()), _histogram(&histogram) {}

void MeasureTimeGuard::fire() {
  if (_histogram) {
    auto const endTime = std::chrono::steady_clock::now();
    auto const duration =
        std::chrono::duration_cast<std::chrono::microseconds>(endTime - _start);
    _histogram->count(duration.count());
    _histogram.reset();
  }
}

MeasureTimeGuard::~MeasureTimeGuard() { fire(); }
