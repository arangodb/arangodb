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

#include "Basics/debugging.h"
#include "Metrics/Metric.h"

#include <atomic>

namespace arangodb::metrics {

template <typename T>
class Gauge final : public Metric {
 public:
  Gauge(T t, std::string_view name, std::string_view help, std::string_view labels)
      : Metric{name, help, labels}, _g{t} {}

  [[nodiscard]] std::string_view type() const noexcept final { return "gauge"; }

  void toPrometheus(std::string& result, std::string_view globalLabels,
                    std::string_view alternativeName) const final {
    result.append(alternativeName.empty() ? name() : alternativeName);
    result.push_back('{');
    bool haveGlobals = !globalLabels.empty();
    if (haveGlobals) {
      result.append(globalLabels);
    }
    if (!labels().empty()) {
      if (haveGlobals) {
        result.push_back(',');
      }
      result.append(labels());
    }
    result.push_back('}');
    result.push_back(' ');
    result.append(std::to_string(load()));
    result.push_back('\n');
  }

  [[nodiscard]] T load(std::memory_order mo = std::memory_order_relaxed) const noexcept {
    return _g.load(mo);
  }

  void store(T value, std::memory_order mo) noexcept {
    return _g.store(value, mo);
  }

  T fetch_add(T t, std::memory_order mo = std::memory_order_relaxed) noexcept {
    if constexpr (std::is_integral_v<T>) {
      return _g.fetch_add(t, mo);
    } else {
      T tmp(_g.load(std::memory_order_relaxed));
      while (!_g.compare_exchange_weak(tmp, tmp + t, mo, std::memory_order_relaxed)) {
      }
      return tmp;
    }
  }

  T fetch_sub(T t, std::memory_order mo = std::memory_order_relaxed) noexcept {
    if constexpr (std::is_integral_v<T>) {
      return _g.fetch_sub(t, mo);
    } else {
      T tmp(_g.load(std::memory_order_relaxed));
      while (!_g.compare_exchange_weak(tmp, tmp - t, mo, std::memory_order_relaxed)) {
      }
      return tmp;
    }
  }
  T fetch_mul(T t, std::memory_order mo) noexcept {
    T tmp(_g.load(std::memory_order_relaxed));
    while (!_g.compare_exchange_weak(tmp, tmp * t, mo, std::memory_order_relaxed)) {
    }
    return tmp;
  }

  T fetch_div(T t, std::memory_order mo) noexcept {
    TRI_ASSERT(t != T(0));
    T tmp(_g.load(std::memory_order_relaxed));
    while (!_g.compare_exchange_weak(tmp, tmp / t, mo, std::memory_order_relaxed)) {
    }
    return tmp;
  }

  /// DEPRECATED

  Gauge<T>& operator=(T t) noexcept {
    _g.store(t, std::memory_order_relaxed);
    return *this;
  }

  Gauge<T>& operator+=(T t) noexcept {
    fetch_add(t, std::memory_order_relaxed);
    return *this;
  }

  Gauge<T>& operator-=(T t) noexcept {
    fetch_sub(t, std::memory_order_relaxed);
    return *this;
  }

  Gauge<T>& operator*=(T t) noexcept {
    fetch_mul(t, std::memory_order_seq_cst);
    return *this;
  }

  Gauge<T>& operator/=(T t) noexcept {
    fetch_div(t, std::memory_order_seq_cst);
    return *this;
  }

  Gauge<T>& operator++() noexcept {
    fetch_add(1, std::memory_order_relaxed);
    return *this;
  }

  Gauge<T>& operator--() noexcept {
    fetch_sub(1, std::memory_order_relaxed);
    return *this;
  }

 private:
  std::atomic<T> _g;
};

}  // namespace arangodb::metrics
