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

// Per-target definitions shared by ops/*.h and user code.

// Separate header because foreach_target.h re-enables its include guard.
#include "hwy/ops/set_macros-inl.h"

// Relies on the external include guard in highway.h.
HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

// SIMD operations are implemented as overloaded functions selected using a
// "descriptor" D := Simd<T, N>. T is the lane type, N a number of lanes >= 1
// (always a power of two). Users generally do not choose N directly, but
// instead use HWY_FULL(T[, LMUL]) (the largest available size). N is not
// necessarily the actual number of lanes, which is returned by Lanes(D()).
//
// Only HWY_FULL(T) and N <= 16 / sizeof(T) are guaranteed to be available - the
// latter are useful if >128 bit vectors are unnecessary or undesirable.
template <typename Lane, size_t N>
struct Simd {
  constexpr Simd() = default;
  using T = Lane;
  static_assert((N & (N - 1)) == 0 && N != 0, "N must be a power of two");

  // Widening/narrowing ops change the number of lanes and/or their type.
  // To initialize such vectors, we need the corresponding descriptor types:

  // PromoteTo/DemoteTo() with another lane type, but same number of lanes.
  template <typename NewLane>
  using Rebind = Simd<NewLane, N>;

  // MulEven() with another lane type, but same total size.
  // Round up to correctly handle scalars with N=1.
  template <typename NewLane>
  using Repartition =
      Simd<NewLane, (N * sizeof(Lane) + sizeof(NewLane) - 1) / sizeof(NewLane)>;

  // LowerHalf() with the same lane type, but half the lanes.
  // Round up to correctly handle scalars with N=1.
  using Half = Simd<T, (N + 1) / 2>;

  // Combine() with the same lane type, but twice the lanes.
  using Twice = Simd<T, 2 * N>;
};

template <class D>
using TFromD = typename D::T;

// Descriptor for the same number of lanes as D, but with the LaneType T.
template <class T, class D>
using Rebind = typename D::template Rebind<T>;

template <class D>
using RebindToSigned = Rebind<MakeSigned<TFromD<D>>, D>;
template <class D>
using RebindToUnsigned = Rebind<MakeUnsigned<TFromD<D>>, D>;
template <class D>
using RebindToFloat = Rebind<MakeFloat<TFromD<D>>, D>;

// Descriptor for the same total size as D, but with the LaneType T.
template <class T, class D>
using Repartition = typename D::template Repartition<T>;

template <class D>
using RepartitionToWide = Repartition<MakeWide<TFromD<D>>, D>;
template <class D>
using RepartitionToNarrow = Repartition<MakeNarrow<TFromD<D>>, D>;

// Descriptor for the same lane type as D, but half the lanes.
template <class D>
using Half = typename D::Half;

// Descriptor for the same lane type as D, but twice the lanes.
template <class D>
using Twice = typename D::Twice;

// Same as base.h macros but with a Simd<T, N> argument instead of T.
#define HWY_IF_UNSIGNED_D(D) HWY_IF_UNSIGNED(TFromD<D>)
#define HWY_IF_SIGNED_D(D) HWY_IF_SIGNED(TFromD<D>)
#define HWY_IF_FLOAT_D(D) HWY_IF_FLOAT(TFromD<D>)
#define HWY_IF_NOT_FLOAT_D(D) HWY_IF_NOT_FLOAT(TFromD<D>)
#define HWY_IF_LANE_SIZE_D(D, bytes) HWY_IF_LANE_SIZE(TFromD<D>, bytes)
#define HWY_IF_NOT_LANE_SIZE_D(D, bytes) HWY_IF_NOT_LANE_SIZE(TFromD<D>, bytes)

// Compile-time-constant, (typically but not guaranteed) an upper bound on the
// number of lanes.
// Prefer instead using Lanes() and dynamic allocation, or Rebind, or
// `#if HWY_CAP_GE*`.
template <typename T, size_t N>
HWY_INLINE HWY_MAYBE_UNUSED constexpr size_t MaxLanes(Simd<T, N>) {
  return N;
}

// Targets with non-constexpr Lanes define this themselves.
#if HWY_TARGET != HWY_RVV

// (Potentially) non-constant actual size of the vector at runtime, subject to
// the limit imposed by the Simd. Useful for advancing loop counters.
template <typename T, size_t N>
HWY_INLINE HWY_MAYBE_UNUSED size_t Lanes(Simd<T, N>) {
  return N;
}

#endif

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
