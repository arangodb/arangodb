/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include <folly/Portability.h>
#include <folly/ConstexprMath.h>

// Work around bug https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56019
#ifdef __GNUC__
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 9)
namespace std {
using ::max_align_t;
}
#endif
#endif

namespace folly {

//  has_extended_alignment
//
//  True if it may be presumed that the platform has static extended alignment;
//  false if it may not be so presumed, even when the platform might actually
//  have it. Static extended alignment refers to extended alignment of objects
//  with automatic, static, or thread storage. Whether the there is support for
//  dynamic extended alignment is a property of the allocator which is used for
//  each given dynamic allocation.
//
//  Currently, very heuristical - only non-mobile 64-bit linux gets the extended
//  alignment treatment. Theoretically, this could be tuned better.
constexpr bool has_extended_alignment =
    kIsLinux && sizeof(void*) >= sizeof(std::uint64_t);

namespace detail {

// Implemented this way because of a bug in Clang for ARMv7, which gives the
// wrong result for `alignof` a `union` with a field of each scalar type.
// Modified for RocksDB to use C++11 only
constexpr std::size_t max_align_v = constexpr_max(
    alignof(long double),
    alignof(double),
    alignof(float),
    alignof(long long int),
    alignof(long int),
    alignof(int),
    alignof(short int),
    alignof(bool),
    alignof(char),
    alignof(char16_t),
    alignof(char32_t),
    alignof(wchar_t),
    alignof(void*),
    alignof(std::max_align_t));

} // namespace detail

// max_align_v is the alignment of max_align_t.
//
// max_align_t is a type which is aligned at least as strictly as the
// most-aligned basic type (see the specification of std::max_align_t). This
// implementation exists because 32-bit iOS platforms have a broken
// std::max_align_t (see below).
//
// You should refer to this as `::folly::max_align_t` in portable code, even if
// you have `using namespace folly;` because C11 defines a global namespace
// `max_align_t` type.
//
// To be certain, we consider every non-void fundamental type specified by the
// standard. On most platforms `long double` would be enough, but iOS 32-bit
// has an 8-byte aligned `double` and `long long int` and a 4-byte aligned
// `long double`.
//
// So far we've covered locals and other non-allocated storage, but we also need
// confidence that allocated storage from `malloc`, `new`, etc will also be
// suitable for objects with this alignment requirement.
//
// Apple document that their implementation of malloc will issue 16-byte
// granularity chunks for small allocations (large allocations are page-size
// granularity and page-aligned). We think that allocated storage will be
// suitable for these objects based on the following assumptions:
//
// 1. 16-byte granularity also means 16-byte aligned.
// 2. `new` and other allocators follow the `malloc` rules.
//
// We also have some anecdotal evidence: we don't see lots of misaligned-storage
// crashes on 32-bit iOS apps that use `double`.
//
// Apple's allocation reference: http://bit.ly/malloc-small
constexpr std::size_t max_align_v = detail::max_align_v;
struct alignas(max_align_v) max_align_t {};

//  Memory locations within the same cache line are subject to destructive
//  interference, also known as false sharing, which is when concurrent
//  accesses to these different memory locations from different cores, where at
//  least one of the concurrent accesses is or involves a store operation,
//  induce contention and harm performance.
//
//  Microbenchmarks indicate that pairs of cache lines also see destructive
//  interference under heavy use of atomic operations, as observed for atomic
//  increment on Sandy Bridge.
//
//  We assume a cache line size of 64, so we use a cache line pair size of 128
//  to avoid destructive interference.
//
//  mimic: std::hardware_destructive_interference_size, C++17
constexpr std::size_t hardware_destructive_interference_size =
    kIsArchArm ? 64 : 128;
static_assert(hardware_destructive_interference_size >= max_align_v, "math?");

//  Memory locations within the same cache line are subject to constructive
//  interference, also known as true sharing, which is when accesses to some
//  memory locations induce all memory locations within the same cache line to
//  be cached, benefiting subsequent accesses to different memory locations
//  within the same cache line and heping performance.
//
//  mimic: std::hardware_constructive_interference_size, C++17
constexpr std::size_t hardware_constructive_interference_size = 64;
static_assert(hardware_constructive_interference_size >= max_align_v, "math?");

//  A value corresponding to hardware_constructive_interference_size but which
//  may be used with alignas, since hardware_constructive_interference_size may
//  be too large on some platforms to be used with alignas.
constexpr std::size_t cacheline_align_v = has_extended_alignment
    ? hardware_constructive_interference_size
    : max_align_v;
struct alignas(cacheline_align_v) cacheline_align_t {};

} // namespace folly
