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

#include "infra/ResourceMonitor.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define JEMALLOC_NO_DEMANGLE
#include <jemalloc/jemalloc.h>

namespace {

// 16-byte allocation header, placed immediately before the user pointer.
// For normal allocations (malloc/calloc/realloc and aligned with
// alignment<=16), real_ptr == header, so we don't store it. For aligned
// allocations with alignment >= 32, real_ptr is stored in the alignment gap at
// (header - 8) for free, and bit 0 of monitor_tagged is set.
struct AllocHeader {
  uintptr_t monitor_tagged;  // ResourceMonitor* | tag bit 0
  size_t requested_size;
};
static_assert(sizeof(AllocHeader) == 16);
static_assert(sizeof(AllocHeader) == alignof(std::max_align_t));

arangodb::infra::ResourceMonitor g_global_monitor;

thread_local arangodb::infra::ResourceMonitor* g_current_monitor = nullptr;

auto userPtr(AllocHeader* header) -> void* { return header + 1; }

auto headerPtr(void* user) -> AllocHeader* {
  return static_cast<AllocHeader*>(user) - 1;
}

auto effectiveMonitor() -> arangodb::infra::ResourceMonitor* {
  auto* m = g_current_monitor;
  return m ? m : &g_global_monitor;
}

auto getMonitor(AllocHeader* header) -> arangodb::infra::ResourceMonitor* {
  return reinterpret_cast<arangodb::infra::ResourceMonitor*>(
      header->monitor_tagged & ~uintptr_t{1});
}

auto isAlignedAlloc(AllocHeader* header) -> bool {
  return (header->monitor_tagged & 1) != 0;
}

auto getRealPtr(AllocHeader* header) -> void* {
  if (!isAlignedAlloc(header)) {
    return header;
  }
  return *(reinterpret_cast<void**>(header) - 1);
}

auto headerOverhead(AllocHeader* header) -> size_t {
  return static_cast<size_t>(reinterpret_cast<char*>(userPtr(header)) -
                             reinterpret_cast<char*>(getRealPtr(header)));
}

void initHeader(AllocHeader* header, size_t requested) {
  header->monitor_tagged = reinterpret_cast<uintptr_t>(effectiveMonitor());
  header->requested_size = requested;
}

void initAlignedHeader(AllocHeader* header, void* real_ptr, size_t requested) {
  *(reinterpret_cast<void**>(header) - 1) = real_ptr;
  header->monitor_tagged =
      reinterpret_cast<uintptr_t>(effectiveMonitor()) | uintptr_t{1};
  header->requested_size = requested;
}

void trackAlloc(AllocHeader* header) {
  auto requested = header->requested_size;
  auto actual = je_malloc_usable_size(getRealPtr(header));
  getMonitor(header)->increase(requested, actual);
}

void trackFree(AllocHeader* header) {
  auto requested = header->requested_size;
  auto actual = je_malloc_usable_size(getRealPtr(header));
  getMonitor(header)->decrease(requested, actual);
}

void printAllocationStats() {
  auto& m = g_global_monitor;
  std::fprintf(stderr,
               "\n[MallocOverride] live: %lu requested, %lu actual bytes\n",
               static_cast<unsigned long>(
                   m.requested_bytes.load(std::memory_order_relaxed)),
               static_cast<unsigned long>(
                   m.actual_bytes.load(std::memory_order_relaxed)));
}

// NOLINTNEXTLINE(cert-err58-cpp)
const auto kRegistered = (std::atexit(printAllocationStats), true);
}  // namespace

namespace arangodb::infra {

auto getGlobalResourceMonitor() noexcept -> ResourceMonitor& {
  return g_global_monitor;
}

}  // namespace arangodb::infra

extern "C" {

void* malloc(size_t size) noexcept {
  void* real = je_malloc(sizeof(AllocHeader) + size);
  if (!real) {
    return nullptr;
  }
  auto* header = static_cast<AllocHeader*>(real);
  initHeader(header, size);
  trackAlloc(header);
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
  if (nmemb != 0 && size > SIZE_MAX / nmemb) {
    return nullptr;
  }
  size_t total = nmemb * size;
  void* real = je_malloc(sizeof(AllocHeader) + total);
  if (!real) {
    return nullptr;
  }
  auto* header = static_cast<AllocHeader*>(real);
  initHeader(header, total);
  void* user = userPtr(header);
  std::memset(user, 0, total);
  trackAlloc(header);
  return user;
}

void* realloc(void* ptr, size_t size) noexcept {
  if (!ptr) {
    return malloc(size);
  }

  auto* old_header = headerPtr(ptr);
  trackFree(old_header);

  void* old_real = getRealPtr(old_header);
  bool was_aligned = isAlignedAlloc(old_header);

  if (was_aligned) {
    size_t old_size = old_header->requested_size;
    void* new_real = je_malloc(sizeof(AllocHeader) + size);
    if (!new_real) {
      trackAlloc(old_header);
      return nullptr;
    }
    auto* new_header = static_cast<AllocHeader*>(new_real);
    initHeader(new_header, size);
    void* new_user = userPtr(new_header);
    std::memcpy(new_user, ptr, size < old_size ? size : old_size);
    trackAlloc(new_header);
    je_free(old_real);
    return new_user;
  }

  void* new_real = je_realloc(old_real, sizeof(AllocHeader) + size);
  if (!new_real) {
    trackAlloc(old_header);
    return nullptr;
  }

  auto* new_header = static_cast<AllocHeader*>(new_real);
  new_header->requested_size = size;
  trackAlloc(new_header);
  return userPtr(new_header);
}

int posix_memalign(void** memptr, size_t alignment, size_t size) noexcept {
  if (alignment <= sizeof(AllocHeader)) {
    void* real = nullptr;
    int result =
        je_posix_memalign(&real, alignment, sizeof(AllocHeader) + size);
    if (result != 0) {
      return result;
    }
    auto* header = static_cast<AllocHeader*>(real);
    initHeader(header, size);
    trackAlloc(header);
    *memptr = userPtr(header);
    return 0;
  }

  void* real =
      je_malloc(sizeof(AllocHeader) + sizeof(void*) + size + alignment);
  if (!real) {
    return ENOMEM;
  }
  auto addr =
      reinterpret_cast<uintptr_t>(real) + sizeof(AllocHeader) + sizeof(void*);
  addr = (addr + alignment - 1) & ~(alignment - 1);
  void* user = reinterpret_cast<void*>(addr);
  auto* header = headerPtr(user);
  initAlignedHeader(header, real, size);
  trackAlloc(header);
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

size_t malloc_usable_size(void* ptr) noexcept {
  if (!ptr) {
    return 0;
  }
  auto* header = headerPtr(ptr);
  return je_malloc_usable_size(getRealPtr(header)) - headerOverhead(header);
}

}  // extern "C"
