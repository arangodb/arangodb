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
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "Gauge.h"
#include "Logger/LogMacros.h"

namespace arangodb::metrics {

struct InstrumentedBool {
  struct Metrics {
    Gauge<uint64_t>* true_counter = nullptr;
    Gauge<uint64_t>* false_counter = nullptr;
  };

  explicit InstrumentedBool(Metrics metrics, bool initialValue = false)
      : _metrics(metrics), _value(initialValue) {
    addToGauge(_value ? _metrics.true_counter : _metrics.false_counter, 1);
  }

  InstrumentedBool(InstrumentedBool const& other)
      : InstrumentedBool(other._metrics, other._value) {}
  InstrumentedBool& operator=(InstrumentedBool const& other) {
    set(other._value);
    return *this;
  }

  ~InstrumentedBool() {
    addToGauge(_value ? _metrics.true_counter : _metrics.false_counter, -1);
  }

  operator bool() const { return _value; }

  InstrumentedBool& operator=(bool value) {
    set(value);
    return *this;
  }

  void set(bool value) {
    if (value == _value) {
      return;
    }
    _value = value;
    if (value) {
      addToGauge(_metrics.true_counter, 1);
      addToGauge(_metrics.false_counter, -1);
    } else {
      addToGauge(_metrics.true_counter, -1);
      addToGauge(_metrics.false_counter, 1);
    }
  }

 private:
  static void addToGauge(Gauge<uint64_t>* gauge, int64_t v) {
    if (gauge) {
      if (v > 0) {
        gauge->fetch_add(1);
      } else {
        gauge->fetch_sub(1);
      }
    }
  }

  Metrics _metrics;
  bool _value = false;
};

}  // namespace arangodb::metrics
