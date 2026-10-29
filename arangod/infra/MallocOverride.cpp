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

#include "MallocOverride.h"

#include "Assertions/Assert.h"
#include "infra/AllocHeader.h"
#include "infra/ResourceMonitorScope.h"
#include "infra/ResizeAccounting.h"
#include "infra/ResourceMonitor.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#define JEMALLOC_NO_DEMANGLE
#include <jemalloc/jemalloc.h>

using namespace arangodb::infra;

namespace {

StaticResourceMonitor g_global_monitor;

auto effectiveMonitor() -> ResourceMonitorBase* {
  auto* m = detail::currentResourceMonitor();
  return m ? m : &g_global_monitor;
}

// Computes base + extra without wrapping. Returns false on overflow, so an
// oversized request is rejected here rather than silently wrapping to a small
// total that je_nallocx would accept (which would under-allocate). The builtin
// folds the add and the overflow test into a single add + carry-flag check.
auto checkedAdd(size_t base, size_t extra, size_t& result) -> bool {
  return !__builtin_add_overflow(base, extra, &result);
}

auto isPowerOfTwo(size_t value) -> bool {
  return value != 0 && (value & (value - 1)) == 0;
}

// Rejected by posix_memalign's contract, and the over-alignment path's round-up
// is only correct for powers of two.
auto isValidAlignment(size_t alignment) -> bool {
  return isPowerOfTwo(alignment) && alignment % sizeof(void*) == 0;
}

auto pageSize() -> size_t { return static_cast<size_t>(sysconf(_SC_PAGESIZE)); }

auto tryIncrease(ResourceMonitorBase* base, uint64_t requested, uint64_t actual)
    -> bool {
  if (base->isStatic()) {
    return static_cast<StaticResourceMonitor*>(base)->increase(requested,
                                                               actual);
  }
  return static_cast<ResourceMonitor*>(base)->increase(requested, actual);
}

void doDecrease(ResourceMonitorBase* base, uint64_t requested,
                uint64_t actual) {
  if (base->isStatic()) {
    static_cast<StaticResourceMonitor*>(base)->decrease(requested, actual);
  } else {
    static_cast<ResourceMonitor*>(base)->decrease(requested, actual);
  }
}

void trackFree(AllocHeader* header) {
  auto requested = header->requested_size;
  auto actual = je_malloc_usable_size(getRealPtr(header));
  doDecrease(getMonitor(header), requested, actual);
}

}  // namespace

namespace arangodb::infra {

auto getGlobalResourceMonitor() noexcept -> StaticResourceMonitor& {
  return g_global_monitor;
}

}  // namespace arangodb::infra

// memory function overrides that track allocations

extern "C" {

void* malloc(size_t size) noexcept {
  size_t total = 0;
  if (!checkedAdd(size, sizeof(AllocHeader), total)) [[unlikely]] {
    return nullptr;
  }
  size_t actual = je_nallocx(total, 0);
  if (actual == 0) [[unlikely]] {
    return nullptr;
  }
  auto* monitor = effectiveMonitor();
  if (!tryIncrease(monitor, size, actual)) [[unlikely]] {
    return nullptr;
  }
  void* real = je_malloc(total);
  if (!real) [[unlikely]] {
    doDecrease(monitor, size, actual);
    return nullptr;
  }
  auto* header = static_cast<AllocHeader*>(real);
  initHeader(header, monitor, size);
  return userPtr(header);
}

void free(void* ptr) noexcept {
  if (!ptr) {
    return;
  }
  auto* header = headerPtr(ptr);
  trackFree(header);
  je_free(getRealPtr(header));
}

void* calloc(size_t nmemb, size_t size) noexcept {
  if (nmemb != 0 && size > SIZE_MAX / nmemb) [[unlikely]] {
    return nullptr;
  }
  size_t total_user = nmemb * size;
  size_t total = 0;
  if (!checkedAdd(total_user, sizeof(AllocHeader), total)) [[unlikely]] {
    return nullptr;
  }
  size_t actual = je_nallocx(total, 0);
  if (actual == 0) [[unlikely]] {
    return nullptr;
  }
  auto* monitor = effectiveMonitor();
  if (!tryIncrease(monitor, total_user, actual)) [[unlikely]] {
    return nullptr;
  }
  void* real = je_malloc(total);
  if (!real) [[unlikely]] {
    doDecrease(monitor, total_user, actual);
    return nullptr;
  }
  auto* header = static_cast<AllocHeader*>(real);
  initHeader(header, monitor, total_user);
  void* user = userPtr(header);
  std::memset(user, 0, total_user);
  return user;
}

void* realloc(void* ptr, size_t size) noexcept {
  if (!ptr) {
    return malloc(size);
  }

  size_t new_total = 0;
  if (!checkedAdd(size, sizeof(AllocHeader), new_total)) [[unlikely]] {
    return nullptr;
  }
  size_t new_actual = je_nallocx(new_total, 0);
  if (new_actual == 0) [[unlikely]] {
    return nullptr;
  }

  auto* old_header = headerPtr(ptr);
  void* old_real = getRealPtr(old_header);
  bool was_aligned = isAlignedAlloc(old_header);
  auto* old_monitor = getMonitor(old_header);
  auto old_requested = old_header->requested_size;
  auto old_actual = je_malloc_usable_size(old_real);

  auto* new_monitor = effectiveMonitor();
  TRI_ASSERT(new_monitor == old_monitor);

  // Net the resize against the still-counted old allocation when it stays on
  // the same monitor, so a small growth (or a shrink) is not checked against
  // the limit as if it were a full new allocation on top of the old one. See
  // computeResizeDelta for the rationale. Reserve happens on new_monitor before
  // the resize; release happens on old_monitor after (the two are the same
  // monitor in the netted case).
  auto delta = arangodb::infra::computeResizeDelta(
      new_monitor == old_monitor, old_requested, old_actual, size, new_actual);

  if (!tryIncrease(new_monitor, delta.reserveRequested, delta.reserveActual))
      [[unlikely]] {
    return nullptr;
  }

  if (was_aligned) [[unlikely]] {
    void* new_real = je_malloc(new_total);
    if (!new_real) [[unlikely]] {
      doDecrease(new_monitor, delta.reserveRequested, delta.reserveActual);
      return nullptr;
    }
    auto* new_header = static_cast<AllocHeader*>(new_real);
    initHeader(new_header, new_monitor, size);
    void* new_user = userPtr(new_header);
    std::memcpy(new_user, ptr, size < old_requested ? size : old_requested);
    doDecrease(old_monitor, delta.releaseRequested, delta.releaseActual);
    je_free(old_real);
    return new_user;
  }

  void* new_real = je_realloc(old_real, new_total);
  if (!new_real) [[unlikely]] {
    doDecrease(new_monitor, delta.reserveRequested, delta.reserveActual);
    return nullptr;
  }

  doDecrease(old_monitor, delta.releaseRequested, delta.releaseActual);
  auto* new_header = static_cast<AllocHeader*>(new_real);
  initHeader(new_header, new_monitor, size);
  return userPtr(new_header);
}

int posix_memalign(void** memptr, size_t alignment, size_t size) noexcept {
  if (!isValidAlignment(alignment)) [[unlikely]] {
    return EINVAL;
  }

  if (alignment <= sizeof(AllocHeader)) {
    size_t total = 0;
    if (!checkedAdd(size, sizeof(AllocHeader), total)) [[unlikely]] {
      return ENOMEM;
    }
    size_t actual = je_nallocx(total, 0);
    if (actual == 0) [[unlikely]] {
      return ENOMEM;
    }
    auto* monitor = effectiveMonitor();
    if (!tryIncrease(monitor, size, actual)) [[unlikely]] {
      return ENOMEM;
    }
    void* real = nullptr;
    int result = je_posix_memalign(&real, alignment, total);
    if (result != 0) [[unlikely]] {
      doDecrease(monitor, size, actual);
      return result;
    }
    auto* header = static_cast<AllocHeader*>(real);
    initHeader(header, monitor, size);
    *memptr = userPtr(header);
    return 0;
  }

  // For large alignments we over-allocate to guarantee alignment.
  size_t alloc_size = 0;
  if (!checkedAdd(size, alignment, alloc_size) ||
      !checkedAdd(alloc_size, sizeof(AllocHeader) + sizeof(void*), alloc_size))
      [[unlikely]] {
    return ENOMEM;
  }
  size_t actual = je_nallocx(alloc_size, 0);
  if (actual == 0) [[unlikely]] {
    return ENOMEM;
  }
  auto* monitor = effectiveMonitor();
  if (!tryIncrease(monitor, size, actual)) [[unlikely]] {
    return ENOMEM;
  }
  void* real = je_malloc(alloc_size);
  if (!real) [[unlikely]] {
    doDecrease(monitor, size, actual);
    return ENOMEM;
  }
  auto addr =
      reinterpret_cast<uintptr_t>(real) + sizeof(AllocHeader) + sizeof(void*);
  addr = (addr + alignment - 1) & ~(alignment - 1);
  void* user = reinterpret_cast<void*>(addr);
  auto* header = headerPtr(user);
  initAlignedHeader(header, monitor, real, size);
  *memptr = user;
  return 0;
}

void* aligned_alloc(size_t alignment, size_t size) noexcept {
  void* result = nullptr;
  if (posix_memalign(&result, alignment, size) != 0) {
    return nullptr;
  }
  return result;
}

// glibc defines these and prefixed jemalloc exports none of them, so without our
// definitions the call reaches glibc's arena while the matching free() reaches
// us. Each reduces to an entry point above; only the contracts differ.

void* memalign(size_t alignment, size_t size) noexcept {
  if (!isPowerOfTwo(alignment)) [[unlikely]] {
    errno = EINVAL;
    return nullptr;
  }
  // memalign allows alignments posix_memalign rejects; over-aligning is
  // still a valid answer.
  if (alignment < sizeof(void*)) {
    alignment = sizeof(void*);
  }
  void* result = nullptr;
  if (int rc = posix_memalign(&result, alignment, size); rc != 0) [[unlikely]] {
    errno = rc;
    return nullptr;
  }
  return result;
}

void* valloc(size_t size) noexcept { return memalign(pageSize(), size); }

void* pvalloc(size_t size) noexcept {
  auto page = pageSize();
  // pvalloc rounds the request up to a whole number of pages, never zero.
  size_t rounded = 0;
  if (!checkedAdd(size, page - 1, rounded)) [[unlikely]] {
    errno = ENOMEM;
    return nullptr;
  }
  rounded &= ~(page - 1);
  if (rounded == 0) {
    rounded = page;
  }
  return memalign(page, rounded);
}

void* reallocarray(void* ptr, size_t nmemb, size_t size) noexcept {
  if (nmemb != 0 && size > SIZE_MAX / nmemb) [[unlikely]] {
    errno = ENOMEM;
    return nullptr;
  }
  return realloc(ptr, nmemb * size);
}

size_t malloc_usable_size(void* ptr) noexcept {
  if (!ptr) {
    return 0;
  }
  auto* header = headerPtr(ptr);
  return je_malloc_usable_size(getRealPtr(header)) - headerOverhead(header);
}

}  // extern "C"
