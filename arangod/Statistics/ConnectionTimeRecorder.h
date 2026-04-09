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

#pragma once

#include <chrono>
#include "Metrics/Histogram.h"

namespace arangodb {

// RAII guard that records connection duration to the MetricsFeature histogram
// on destruction (i.e. when the connection closes).
struct ConnectionTimeRecorder {
  ConnectionTimeRecorder() = default;

  explicit ConnectionTimeRecorder(
      metrics::HistogramBase<double>& histogram) noexcept
      : _start(std::chrono::steady_clock::now()), _histogram(&histogram) {}

  ConnectionTimeRecorder(ConnectionTimeRecorder const&) = delete;
  ConnectionTimeRecorder& operator=(ConnectionTimeRecorder const&) = delete;
  ConnectionTimeRecorder(ConnectionTimeRecorder&&) = default;
  ConnectionTimeRecorder& operator=(ConnectionTimeRecorder&&) = default;

  ~ConnectionTimeRecorder() {
    if (_histogram != nullptr) {
      double const dt = std::chrono::duration<double>(
                            std::chrono::steady_clock::now() - _start)
                            .count();
      _histogram->count(dt);
    }
  }

 private:
  std::chrono::steady_clock::time_point _start;
  metrics::HistogramBase<double>* _histogram = nullptr;
};

}  // namespace arangodb
