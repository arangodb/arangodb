#pragma once

#include <atomic>
#include <cstdint>

namespace arangodb::infra {

struct ResourceMonitor {
  std::atomic<uint64_t> requested_bytes{0};
  std::atomic<uint64_t> actual_bytes{0};

  void increase(uint64_t requested, uint64_t actual) noexcept {
    requested_bytes.fetch_add(requested, std::memory_order_relaxed);
    actual_bytes.fetch_add(actual, std::memory_order_relaxed);
  }

  void decrease(uint64_t requested, uint64_t actual) noexcept {
    requested_bytes.fetch_sub(requested, std::memory_order_relaxed);
    actual_bytes.fetch_sub(actual, std::memory_order_relaxed);
  }
};

}  // namespace arangodb::infra
