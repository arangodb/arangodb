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

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace arangodb::infra {

// Diagnostics: every allocation made on a monitor, grouped by requested size.
// Only added to, never subtracted, so it counts allocations made, not live
// memory.
struct AllocSizeStats {
  // Upper bounds (exclusive) of all buckets but the last one.
  static constexpr std::array<uint64_t, 4> kLimits{100, 1024, 10 * 1024,
                                                   1024 * 1024};
  static constexpr std::size_t kBuckets = kLimits.size() + 1;

  struct Bucket {
    std::atomic<uint64_t> count{0};
    std::atomic<uint64_t> requested{0};
    std::atomic<uint64_t> noHeader{0};
    std::atomic<uint64_t> actual{0};
  };

  static constexpr auto bucketOf(uint64_t requested) noexcept -> std::size_t {
    std::size_t i = 0;
    while (i < kLimits.size() && requested >= kLimits[i]) {
      ++i;
    }
    return i;
  }

  void record(uint64_t requested, uint64_t noHeader, uint64_t actual) noexcept {
    auto& b = buckets[bucketOf(requested)];
    b.count.fetch_add(1, std::memory_order_relaxed);
    b.requested.fetch_add(requested, std::memory_order_relaxed);
    b.noHeader.fetch_add(noHeader, std::memory_order_relaxed);
    b.actual.fetch_add(actual, std::memory_order_relaxed);
  }

  std::array<Bucket, kBuckets> buckets;
};

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

  [[nodiscard]] uint64_t peakBytes() const noexcept {
    return _peakBytes.load(std::memory_order_relaxed);
  }

  // Requested bytes at the moment the actual peak was reached.
  [[nodiscard]] uint64_t peakRequestedBytes() const noexcept {
    return _peakRequestedBytes.load(std::memory_order_relaxed);
  }

  // Number of live allocations; each carries one allocation header.
  [[nodiscard]] uint64_t liveAllocations() const noexcept {
    return _liveAllocations.load(std::memory_order_relaxed);
  }

  // Live allocations at the moment the actual peak was reached.
  [[nodiscard]] uint64_t peakLiveAllocations() const noexcept {
    return _peakLiveAllocations.load(std::memory_order_relaxed);
  }

  // Diagnostics: what the live allocations would cost without our header
  // (the size class of the requested size alone).
  [[nodiscard]] uint64_t noHeaderBytes() const noexcept {
    return _noHeaderBytes.load(std::memory_order_relaxed);
  }

  [[nodiscard]] uint64_t peakNoHeaderBytes() const noexcept {
    return _peakNoHeaderBytes.load(std::memory_order_relaxed);
  }

  void addNoHeader(uint64_t bytes) noexcept {
    auto now = _noHeaderBytes.fetch_add(bytes, std::memory_order_relaxed) + bytes;
    auto peak = _peakNoHeaderBytes.load(std::memory_order_relaxed);
    while (now > peak && !_peakNoHeaderBytes.compare_exchange_weak(
                             peak, now, std::memory_order_relaxed)) {
    }
  }

  void subNoHeader(uint64_t bytes) noexcept {
    _noHeaderBytes.fetch_sub(bytes, std::memory_order_relaxed);
  }

  [[nodiscard]] auto allocSizeStats() const noexcept -> AllocSizeStats const& {
    return _allocSizeStats;
  }

  void recordAllocation(uint64_t requested, uint64_t noHeader,
                        uint64_t actual) noexcept {
    _allocSizeStats.record(requested, noHeader, actual);
  }

  void resetPeak() noexcept {
    _peakBytes.store(actualBytes(), std::memory_order_relaxed);
    _peakRequestedBytes.store(requestedBytes(), std::memory_order_relaxed);
    _peakLiveAllocations.store(liveAllocations(), std::memory_order_relaxed);
    _peakNoHeaderBytes.store(noHeaderBytes(), std::memory_order_relaxed);
  }

  void setLimitHit() noexcept {
    _limitHit.store(true, std::memory_order_relaxed);
  }

  [[nodiscard]] bool limitHit() const noexcept {
    return _limitHit.load(std::memory_order_relaxed);
  }

 protected:
  struct StaticTag {};
  ResourceMonitorBase() = default;
  explicit ResourceMonitorBase(StaticTag) : _static(true) {}

  [[nodiscard]] bool increase(uint64_t requested, uint64_t actual) noexcept {
    uint64_t now;
    if (_limit != 0) {
      auto current = _actualBytes.load(std::memory_order_relaxed);
      do {
        if (current + actual > _limit) {
          setLimitHit();
          return false;
        }
      } while (!_actualBytes.compare_exchange_weak(current, current + actual,
                                                   std::memory_order_relaxed));
      now = current + actual;
    } else {
      now = _actualBytes.fetch_add(actual, std::memory_order_relaxed) + actual;
    }
    auto requestedNow =
        _requestedBytes.fetch_add(requested, std::memory_order_relaxed) +
        requested;
    // Every increase is paired with one decrease (also for realloc/recharge),
    // so this counts live allocations.
    auto liveNow = _liveAllocations.fetch_add(1, std::memory_order_relaxed) + 1;
    updatePeak(now, requestedNow, liveNow);
    return true;
  }

  void decrease(uint64_t requested, uint64_t actual) noexcept {
    _requestedBytes.fetch_sub(requested, std::memory_order_relaxed);
    _actualBytes.fetch_sub(actual, std::memory_order_relaxed);
    _liveAllocations.fetch_sub(1, std::memory_order_relaxed);
  }

 private:
  void updatePeak(uint64_t now, uint64_t requestedNow,
                  uint64_t liveNow) noexcept {
    auto peak = _peakBytes.load(std::memory_order_relaxed);
    while (now > peak) {
      if (_peakBytes.compare_exchange_weak(peak, now,
                                           std::memory_order_relaxed)) {
        // Diagnostics only: may race with a concurrent peak update.
        _peakRequestedBytes.store(requestedNow, std::memory_order_relaxed);
        _peakLiveAllocations.store(liveNow, std::memory_order_relaxed);
        break;
      }
    }
  }

  std::atomic<uint64_t> _requestedBytes{0};
  std::atomic<uint64_t> _actualBytes{0};
  uint64_t _limit{0};
  std::atomic<ResourceMonitorBase*> _next{nullptr};
  bool _static{false};
  std::atomic<uint64_t> _peakBytes{0};
  std::atomic<uint64_t> _peakRequestedBytes{0};
  std::atomic<uint64_t> _liveAllocations{0};
  std::atomic<uint64_t> _peakLiveAllocations{0};
  std::atomic<uint64_t> _noHeaderBytes{0};
  std::atomic<uint64_t> _peakNoHeaderBytes{0};
  AllocSizeStats _allocSizeStats;
  std::atomic<bool> _limitHit{false};
};

}  // namespace arangodb::infra
