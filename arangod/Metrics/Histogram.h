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

#include "Metrics/Metric.h"

#include <atomic>
#include <ostream>  // TODO(MBkkt) replace to iosfwd, compile error now
#include <vector>

namespace arangodb::metrics {

/**
 * @brief Histogram functionality
 */
template<typename Scale>
class Histogram : public Metric {
 public:
  using ValueType = typename Scale::Value;

  Histogram(Scale&& scale, std::string_view name, std::string_view help,
            std::string_view labels)
      : Metric(name, help, labels),
        _c(HistType(scale.n())),
        _scale(std::move(scale)),
        _n(_scale.n() - 1),
        _sum(0) {}

  Histogram(Scale const& scale, std::string_view name, std::string_view help,
            std::string_view labels)
      : Metric(name, help, labels),
        _c(HistType(scale.n())),
        _scale(scale),
        _n(_scale.n() - 1),
        _sum(0) {}

  void track_extremes(ValueType val) noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // the value extremes are not actually required and therefore only tracked
    // in maintainer mode so they can be used when debugging.
    auto expected = _lowr.load(std::memory_order_relaxed);
    while (val < expected) {
      if (_lowr.compare_exchange_weak(expected, val,
                                      std::memory_order_relaxed)) {
        return;
      }
    }
    expected = _highr.load(std::memory_order_relaxed);
    while (val > expected) {
      if (_highr.compare_exchange_weak(expected, val,
                                       std::memory_order_relaxed)) {
        return;
      }
    }
#endif
  }

  std::string_view type() const noexcept final { return "histogram"; }

  Scale const& scale() const { return _scale; }

  size_t pos(ValueType t) const { return _scale.pos(t); }

  void count(ValueType t) noexcept { count(t, 1); }

  void count(ValueType t, uint64_t n) noexcept {
    if (t < _scale.delims().front()) {
      _c[0] += n;
    } else if (t >= _scale.delims().back()) {
      _c[_n] += n;
    } else {
      _c[pos(t)] += n;
    }
    if constexpr (std::is_integral_v<ValueType>) {
      _sum.fetch_add(static_cast<ValueType>(n) * t);
    } else {
      ValueType tmp = _sum.load(std::memory_order_relaxed);
      do {
      } while (!_sum.compare_exchange_weak(
          tmp, tmp + static_cast<ValueType>(n) * t, std::memory_order_relaxed,
          std::memory_order_relaxed));
    }
    track_extremes(t);
  }

  ValueType low() const { return _scale.low(); }
  ValueType high() const { return _scale.high(); }

  HistType::value_type& operator[](size_t n) { return _c[n]; }

  std::vector<uint64_t> load() const {
    std::vector<uint64_t> v(size());
    for (size_t i = 0; i < size(); ++i) {
      v[i] = load(i);
    }
    return v;
  }

  uint64_t load(size_t i) const { return _c.load(i); }

  size_t size() const { return _c.size(); }

  void toPrometheus(std::string& result, std::string_view globals,
                    std::string_view alternativeName) const final {
    uint64_t sum(0);
    std::string ls;
    bool haveGlobals = !globals.empty();
    if (haveGlobals) {
      ls += globals;
    }
    if (!labels().empty()) {
      if (haveGlobals) {
        ls += ",";
      }
      ls += labels();
    }

    std::string theName{alternativeName.empty() ? name() : alternativeName};

    for (size_t i = 0; i < size(); ++i) {
      uint64_t n = load(i);
      sum += n;
      result += theName;
      result += "_bucket{";
      if (!ls.empty()) {
        result += ls + ",";
      }
      auto v = !alternativeName.empty() ? n : sum;
      result += "le=\"" + _scale.delim(i) + "\"} " + std::to_string(v) + "\n";
    }
    result += theName + "_count";
    if (!ls.empty()) {
      result += "{" + ls + "}";
    }
    result += " " + std::to_string(sum) + "\n";
    if (alternativeName.empty()) {  // This is version 2 of the API
      result += theName + "_sum";
      if (!ls.empty()) {
        result += "{" + ls + "}";
      }
      result +=
          " " + std::to_string(_sum.load(std::memory_order_relaxed)) + "\n";
    }
  }

  std::ostream& print(std::ostream& o) const {
    o << name() << " scale: " << _scale;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    o << " extremes: [" << _lowr << ", " << _highr << "]";
#endif
    return o;
  }

 private:
  HistType _c;
  Scale const _scale;
  size_t const _n;
  std::atomic<ValueType> _sum;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  std::atomic<ValueType> _lowr{std::numeric_limits<ValueType>::max()};
  std::atomic<ValueType> _highr{std::numeric_limits<ValueType>::min()};
#endif
};

template<typename T>
std::ostream& operator<<(std::ostream& o, Histogram<T> const& h) {
  return h.print(o);
}

}  // namespace arangodb::metrics
