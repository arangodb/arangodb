////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/debugging.h"
#include "Metrics/Counter.h"
#include "Metrics/Gauge.h"

#include <algorithm>
#include <cstdint>

namespace arangodb {

/// Bytes of request bodies being received, summed over all HTTP/1.1 and
/// HTTP/2 connections of the process. A limit of 0 means unlimited.
class RequestBodyBudget {
 public:
  enum class Admission { kFits, kNeverFits, kNotNow };

  RequestBodyBudget(metrics::Gauge<std::uint64_t>& used,
                    metrics::Counter& rejected) noexcept
      : _used(used), _rejected(rejected) {}

  /// for a body whose size is known before reading it, reserves nothing
  [[nodiscard]] Admission admit(std::uint64_t len,
                                std::uint64_t limit) noexcept {
    if (limit == 0) {
      return Admission::kFits;
    }
    if (len > limit) {
      ++_rejected;
      return Admission::kNeverFits;
    }
    if (_used.load() + len > limit) {
      ++_rejected;
      return Admission::kNotNow;
    }
    return Admission::kFits;
  }

  [[nodiscard]] bool reserve(std::uint64_t len, std::uint64_t limit) noexcept {
    if (limit == 0) {
      _used.fetch_add(len);
      return true;
    }
    // a rejected reservation must never be visible to concurrent ones
    std::uint64_t cur = _used.load();
    do {
      if (cur + len > limit) {
        ++_rejected;
        return false;
      }
    } while (!_used.compare_exchange_weak(cur, cur + len));
    return true;
  }

  void release(std::uint64_t len) noexcept {
    // clamped: an accounting bug must not wrap around and block every body
    std::uint64_t cur = _used.load();
    do {
      TRI_ASSERT(cur >= len);
    } while (!_used.compare_exchange_weak(cur, cur - std::min(cur, len)));
  }

  [[nodiscard]] std::uint64_t used() const noexcept { return _used.load(); }

 private:
  metrics::Gauge<std::uint64_t>& _used;
  metrics::Counter& _rejected;
};

}  // namespace arangodb
