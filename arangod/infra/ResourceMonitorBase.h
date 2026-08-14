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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <cstdint>

namespace arangodb::infra {

struct ResourceMonitorBase {
  ResourceMonitorBase(ResourceMonitorBase const&) = delete;
  ResourceMonitorBase& operator=(ResourceMonitorBase const&) = delete;

  [[nodiscard]] auto isStatic() const noexcept -> bool { return _static; }

  [[nodiscard]] auto requestedBytes() const noexcept -> uint64_t {
    return _requestedBytes.load(std::memory_order_relaxed);
  }

  [[nodiscard]] auto actualBytes() const noexcept -> uint64_t {
    return _actualBytes.load(std::memory_order_relaxed);
  }

  [[nodiscard]] auto limit() const noexcept -> uint64_t { return _limit; }

  void setLimit(uint64_t limit) noexcept { _limit = limit; }

  [[nodiscard]] auto next() const noexcept -> ResourceMonitorBase* {
    return _next.load(std::memory_order_relaxed);
  }

  void setNext(ResourceMonitorBase* next) noexcept {
    _next.store(next, std::memory_order_relaxed);
  }

 protected:
  struct StaticTag {};
  ResourceMonitorBase() = default;
  explicit ResourceMonitorBase(StaticTag) : _static(true) {}

  [[nodiscard]] bool increase(uint64_t requested, uint64_t actual) noexcept {
    if (_limit != 0) {
      auto current = _actualBytes.load(std::memory_order_relaxed);
      do {
        if (current + actual > _limit) {
          return false;
        }
      } while (!_actualBytes.compare_exchange_weak(current, current + actual,
                                                   std::memory_order_relaxed));
    } else {
      _actualBytes.fetch_add(actual, std::memory_order_relaxed);
    }
    _requestedBytes.fetch_add(requested, std::memory_order_relaxed);
    return true;
  }

  void decrease(uint64_t requested, uint64_t actual) noexcept {
    _requestedBytes.fetch_sub(requested, std::memory_order_relaxed);
    _actualBytes.fetch_sub(actual, std::memory_order_relaxed);
  }

 private:
  std::atomic<uint64_t> _requestedBytes{0};
  std::atomic<uint64_t> _actualBytes{0};
  uint64_t _limit{0};
  std::atomic<ResourceMonitorBase*> _next{nullptr};
  bool _static{false};
};

}  // namespace arangodb::infra
