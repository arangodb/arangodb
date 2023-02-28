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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <memory>
#include <chrono>
#include "Metrics/Fwd.h"

namespace arangodb::replication2 {

struct MeasureTimeGuard {
  explicit MeasureTimeGuard(
      metrics::Histogram<metrics::LogScale<std::uint64_t>>& histogram) noexcept;
  MeasureTimeGuard(MeasureTimeGuard const&) = delete;
  MeasureTimeGuard(MeasureTimeGuard&&) = default;
  ~MeasureTimeGuard();

  void fire();

 private:
  std::chrono::steady_clock::time_point const _start;
  struct noop {
    template<typename T>
    void operator()(T*) {}
  };

  std::unique_ptr<metrics::Histogram<metrics::LogScale<std::uint64_t>>, noop>
      _histogram;
};

template<typename N>
struct GaugeScopedCounter {
  explicit GaugeScopedCounter(metrics::Gauge<N>& metric) noexcept
      : _metric(&metric) {
    _metric->fetch_add(1);
  }

  GaugeScopedCounter(GaugeScopedCounter const&) = delete;
  GaugeScopedCounter(GaugeScopedCounter&&) noexcept = default;
  GaugeScopedCounter& operator=(GaugeScopedCounter const&) = delete;
  GaugeScopedCounter& operator=(GaugeScopedCounter&&) noexcept = default;
  ~GaugeScopedCounter() { fire(); }

  void fire() noexcept {
    if (_metric != nullptr) {
      _metric->fetch_sub(1);
      _metric.reset();
    }
  }

 private:
  struct noop {
    template<typename T>
    void operator()(T*) {}
  };
  std::unique_ptr<metrics::Gauge<N>, noop> _metric;
};

}  // namespace arangodb::replication2
