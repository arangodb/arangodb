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

#include "infra/ResourceMonitorBase.h"

#include <cstddef>
#include <cstdint>

namespace arangodb::infra {

// The tag bit in monitor_tagged requires monitor pointers to be even.
static_assert(alignof(ResourceMonitorBase) >= 2);

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

inline auto userPtr(AllocHeader* header) -> void* { return header + 1; }

inline auto headerPtr(void* user) -> AllocHeader* {
  return static_cast<AllocHeader*>(user) - 1;
}

inline auto getMonitor(AllocHeader* header) -> ResourceMonitorBase* {
  return reinterpret_cast<ResourceMonitorBase*>(header->monitor_tagged &
                                                ~uintptr_t{1});
}

inline auto isAlignedAlloc(AllocHeader* header) -> bool {
  return (header->monitor_tagged & 1) != 0;
}

inline auto getRealPtr(AllocHeader* header) -> void* {
  if (!isAlignedAlloc(header)) [[likely]] {
    return header;
  }
  return *(reinterpret_cast<void**>(header) - 1);
}

inline auto headerOverhead(AllocHeader* header) -> size_t {
  return static_cast<size_t>(reinterpret_cast<char*>(userPtr(header)) -
                             reinterpret_cast<char*>(getRealPtr(header)));
}

inline void initHeader(AllocHeader* header, ResourceMonitorBase* monitor,
                       size_t requested) {
  header->monitor_tagged = reinterpret_cast<uintptr_t>(monitor);
  header->requested_size = requested;
}

inline void initAlignedHeader(AllocHeader* header, ResourceMonitorBase* monitor,
                              void* real_ptr, size_t requested) {
  *(reinterpret_cast<void**>(header) - 1) = real_ptr;
  header->monitor_tagged = reinterpret_cast<uintptr_t>(monitor) | uintptr_t{1};
  header->requested_size = requested;
}

}  // namespace arangodb::infra
