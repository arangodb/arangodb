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

#include "Basics/debugging.h"
#include "Metrics/Metric.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Value.h>
#include <velocypack/Builder.h>

#include <atomic>

namespace arangodb::metrics {

template<typename T>
class Gauge : public Metric {
 public:
  using Value = T;

  Gauge(T t, std::string_view name, std::string_view help,
        std::string_view labels)
      : Metric{name, help, labels}, _g{t} {}

  [[nodiscard]] std::string_view type() const noexcept final { return "gauge"; }

  void toPrometheus(std::string& result, std::string_view globals,
                    bool ensureWhitespace) const final {
    Metric::addMark(result, name(), globals, labels());
    if constexpr (std::is_integral_v<T>) {
      absl::StrAppend(&result, ensureWhitespace ? " " : "", load(), "\n");
    } else {
      // must use std::to_string() here because it produces a different
      // string representation of large floating-point numbers than absl
      // does. absl uses scientific notation for numbers that exceed 6
      // digits, and std::to_string() doesn't.
      absl::StrAppend(&result, ensureWhitespace ? " " : "",
                      std::to_string(load()), "\n");
    }
  }

  void toVPack(velocypack::Builder& builder) const final {
    builder.add(velocypack::Value{name()});
    builder.add(velocypack::Value{labels()});
    builder.add(velocypack::Value{load(std::memory_order_relaxed)});
  }

  [[nodiscard]] T load(
      std::memory_order mo = std::memory_order_relaxed) const noexcept {
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
      while (!_g.compare_exchange_weak(tmp, tmp + t, mo,
                                       std::memory_order_relaxed)) {
      }
      return tmp;
    }
  }

  T fetch_sub(T t, std::memory_order mo = std::memory_order_relaxed) noexcept {
    if constexpr (std::is_integral_v<T>) {
      return _g.fetch_sub(t, mo);
    } else {
      T tmp(_g.load(std::memory_order_relaxed));
      while (!_g.compare_exchange_weak(tmp, tmp - t, mo,
                                       std::memory_order_relaxed)) {
      }
      return tmp;
    }
  }
  T fetch_mul(T t, std::memory_order mo) noexcept {
    T tmp(_g.load(std::memory_order_relaxed));
    while (!_g.compare_exchange_weak(tmp, tmp * t, mo,
                                     std::memory_order_relaxed)) {
    }
    return tmp;
  }

  T fetch_div(T t, std::memory_order mo) noexcept {
    TRI_ASSERT(t != T(0));
    T tmp(_g.load(std::memory_order_relaxed));
    while (!_g.compare_exchange_weak(tmp, tmp / t, mo,
                                     std::memory_order_relaxed)) {
    }
    return tmp;
  }

  bool compare_exchange_weak(
      T& expected, T desired,
      std::memory_order success = std::memory_order_relaxed,
      std::memory_order fail = std::memory_order_relaxed) noexcept {
    return _g.compare_exchange_weak(expected, desired, success, fail);
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
