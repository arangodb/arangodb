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

#include "Gauge.h"

namespace arangodb::metrics {

template<typename T>
struct GaugeCounterGuard {
  GaugeCounterGuard(GaugeCounterGuard const&) = delete;
  GaugeCounterGuard& operator=(GaugeCounterGuard const&) = delete;

  GaugeCounterGuard(GaugeCounterGuard&& other) noexcept {
    *this = std::move(other);
  }
  GaugeCounterGuard& operator=(GaugeCounterGuard&& other) noexcept {
    reset();
    std::swap(other._totalValue, _totalValue);
    std::swap(other._metric, _metric);
    return *this;
  }

  ~GaugeCounterGuard() { reset(); }
  GaugeCounterGuard() = default;

  explicit GaugeCounterGuard(Gauge<T>* metric, T initialValue = {})
      : _metric(metric) {
    if (metric != nullptr) {
      add(initialValue);
    }
  }

  explicit GaugeCounterGuard(Gauge<T>& metric, T initialValue = {})
      : _metric(&metric) {
    add(initialValue);
  }

  void add(T delta) noexcept {
    if (_metric) {
      _metric->fetch_add(delta);
      _totalValue += delta;
    }
  }

  void sub(T delta) noexcept {
    if (_metric) {
      _metric->fetch_sub(delta);
      _totalValue -= delta;
    }
  }

  void reset(std::uint64_t newValue = {}) noexcept {
    if (_metric) {
      _metric->fetch_sub(_totalValue - newValue);
      _totalValue = newValue;
      _metric = nullptr;
    }
  }

 private:
  T _totalValue{0};
  Gauge<T>* _metric = nullptr;
};

}  // namespace arangodb::metrics
