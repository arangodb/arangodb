// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HIGHWAY_HWY_CACHE_CONTROL_H_
#define HIGHWAY_HWY_CACHE_CONTROL_H_

#include <stddef.h>
#include <stdint.h>

#include "hwy/base.h"

#ifndef __SSE2__
#undef HWY_DISABLE_CACHE_CONTROL
#define HWY_DISABLE_CACHE_CONTROL
#endif

// intrin.h is sufficient on MSVC and already included by base.h.
#if HWY_ARCH_X86 && !defined(HWY_DISABLE_CACHE_CONTROL) && !HWY_COMPILER_MSVC
#include <emmintrin.h>  // SSE2
#endif

namespace hwy {

// Even if N*sizeof(T) is smaller, Stream may write a multiple of this size.
#define HWY_STREAM_MULTIPLE 16

// The following functions may also require an attribute.
#if HWY_ARCH_X86 && !defined(HWY_DISABLE_CACHE_CONTROL) && !HWY_COMPILER_MSVC
#define HWY_ATTR_CACHE __attribute__((target("sse2")))
#else
#define HWY_ATTR_CACHE
#endif

// Delays subsequent loads until prior loads are visible. On Intel CPUs, also
// serves as a full fence (waits for all prior instructions to complete).
// No effect on non-x86.
HWY_INLINE HWY_ATTR_CACHE void LoadFence() {
#if HWY_ARCH_X86 && !defined(HWY_DISABLE_CACHE_CONTROL)
  _mm_lfence();
#endif
}

// Ensures previous weakly-ordered stores are visible. No effect on non-x86.
HWY_INLINE HWY_ATTR_CACHE void StoreFence() {
#if HWY_ARCH_X86 && !defined(HWY_DISABLE_CACHE_CONTROL)
  _mm_sfence();
#endif
}

// Begins loading the cache line containing "p".
template <typename T>
HWY_INLINE HWY_ATTR_CACHE void Prefetch(const T* p) {
#if HWY_ARCH_X86 && !defined(HWY_DISABLE_CACHE_CONTROL)
  _mm_prefetch(reinterpret_cast<const char*>(p), _MM_HINT_T0);
#elif HWY_COMPILER_GCC || HWY_COMPILER_CLANG
  // Hint=0 (NTA) behavior differs, but skipping outer caches is probably not
  // desirable, so use the default 3 (keep in caches).
  __builtin_prefetch(p, /*write=*/0, /*hint=*/3);
#else
  (void)p;
#endif
}

// Invalidates and flushes the cache line containing "p". No effect on non-x86.
HWY_INLINE HWY_ATTR_CACHE void FlushCacheline(const void* p) {
#if HWY_ARCH_X86 && !defined(HWY_DISABLE_CACHE_CONTROL)
  _mm_clflush(p);
#else
  (void)p;
#endif
}

}  // namespace hwy

#endif  // HIGHWAY_HWY_CACHE_CONTROL_H_
