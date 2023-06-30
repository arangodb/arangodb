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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Gauge.h"
#include "Logger/LogMacros.h"

namespace arangodb::metrics {

template<typename T>
struct GaugeCounterGuard {
  GaugeCounterGuard(GaugeCounterGuard const&) = delete;
  GaugeCounterGuard& operator=(GaugeCounterGuard const&) = delete;

  GaugeCounterGuard(GaugeCounterGuard&& other) noexcept {
    std::swap(other._totalValue, _totalValue);
  }
  GaugeCounterGuard& operator=(GaugeCounterGuard&& other) noexcept {
    reset();
    std::swap(other._totalValue, _totalValue);
  }

  ~GaugeCounterGuard() { fire(); }

  explicit GaugeCounterGuard(Gauge<T>& metric, T initialValue = {})
      : _metric(metric) {
    add(initialValue);
  }

  void add(T delta) noexcept {
    _metric.fetch_add(delta);
    _totalValue += delta;
  }

  void sub(T delta) noexcept {
    TRI_ASSERT(_metric != nullptr);
    _metric.fetch_sub(delta);
    _totalValue -= delta;
  }

  void fire() noexcept { reset(); }

  void reset(std::uint64_t newValue = {}) noexcept {
    _metric.fetch_sub(_totalValue - newValue);
    _totalValue = newValue;
  }

 private:
  T _totalValue{0};
  Gauge<T>& _metric;
};

}  // namespace arangodb::metrics
