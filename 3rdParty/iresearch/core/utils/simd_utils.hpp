////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_SIMD_UTILS_H
#define IRESEARCH_SIMD_UTILS_H

#include <algorithm>
#include <hwy/highway.h>

#include "shared.hpp"
#include "utils/bit_packing.hpp"

namespace iresearch {
namespace simd {

using namespace hwy::HWY_NAMESPACE;

template<bool Aligned>
struct simd_helper {
  template<typename Simd, typename T, typename Ptr>
  static void store(T value, const Simd simd_tag, Ptr p) {
    if constexpr (Aligned) {
      Store(value, simd_tag, p);
    } else {
      StoreU(value, simd_tag, p);
    }
  }

  template<typename Simd, typename Ptr>
  static decltype(Load(Simd{}, Ptr{})) load(const Simd simd_tag, Ptr p) {
    if constexpr (Aligned) {
      return Load(simd_tag, p);
    } else {
      return LoadU(simd_tag, p);
    }
  }
};

template<bool Aligned, typename T>
std::pair<T, T> maxmin(const T* begin, size_t size) noexcept {
  constexpr HWY_FULL(T) simd_tag;
  using simd_helper = simd_helper<Aligned>;
  constexpr size_t Step = MaxLanes(simd_tag);
  constexpr size_t Unroll = 2*Step;

  const auto end = begin + size;
  auto minacc = Set(simd_tag, std::numeric_limits<T>::max());
  auto maxacc = Set(simd_tag, std::numeric_limits<T>::min());

  for (size_t step = size / (Unroll*Step); step; --step) {
    for (size_t j = 0; j < Unroll; ++j) {
      const auto v = simd_helper::load(simd_tag, begin + j*Step);
      minacc = Min(minacc, v);
      maxacc = Max(maxacc, v);
    }
    begin += Unroll*Step;
  }

  std::pair<T, T> minmax = {
    GetLane(MinOfLanes(minacc)),
    GetLane(MaxOfLanes(maxacc))
  };

  for (; begin != end; ++begin) {
    minmax.first = std::min(*begin, minmax.first);
    minmax.second = std::max(*begin, minmax.second);
  }

  return minmax;
}

template<size_t Length, bool Aligned, typename T>
std::pair<T, T> maxmin(const T* begin) noexcept {
  return maxmin<Aligned>(begin, Length);
}

template<bool Aligned, typename T>
uint32_t maxbits(const T* begin, size_t size) noexcept {
  using simd_helper = simd_helper<Aligned>;
  constexpr HWY_FULL(T) simd_tag;
  constexpr size_t Step = MaxLanes(simd_tag);
  constexpr size_t Unroll = 2*Step;

  const auto end = begin + size;

  auto oracc = Zero(simd_tag);
  for (size_t steps = size / (Unroll*Step); steps; --steps) {
    for (size_t j = 0; j < Unroll; ++j) {
      oracc = Or(oracc, simd_helper::load(simd_tag, begin + j*Step));
    }
    begin += Unroll*Step;
  }

  // FIXME use OrOfLanes instead
  auto max = GetLane(MaxOfLanes(oracc));

  for (; begin != end; ++begin) {
    max |= *begin;
  }

  return math::math_traits<T>::bits_required(max);
}

template<size_t Length, bool Aligned, typename T>
FORCE_INLINE uint32_t maxbits(const T* begin) noexcept {
  return maxbits<Aligned>(begin, Length);
}

template<bool Aligned, typename T>
bool all_equal(const T* begin, size_t size) noexcept {
  using simd_helper = simd_helper<Aligned>;
  constexpr HWY_FULL(T) simd_tag;
  constexpr size_t Step = MaxLanes(simd_tag);
  constexpr size_t Unroll = Step;

  const auto end = begin + size;

  const auto value = *begin;
  const auto vvalue = Set(simd_tag, value);

  for (size_t steps = size / (Unroll*Step); steps; --steps) {
    for (size_t j = 0; j < Unroll; ++j) {
      if (!AllTrue(vvalue == simd_helper::load(simd_tag, begin + j*Step))) {
        return false;
      }
    }
    begin += Unroll*Step;
  }

  for (; begin != end; ++begin) {
    if (value != *begin) {
      return false;
    }
  }

  return true;
}

FORCE_INLINE Vec<HWY_FULL(uint32_t)> zig_zag_encode(
    Vec<HWY_FULL(int32_t)> v) noexcept {
  constexpr HWY_FULL(uint32_t) simd_tag;
  const auto uv = BitCast(simd_tag, v);
  return ((uv >> Set(simd_tag, 31)) ^ (uv << Set(simd_tag, 1)));
}

FORCE_INLINE Vec<HWY_FULL(int32_t)> zig_zag_decode(
    Vec<HWY_FULL(uint32_t)> uv) noexcept {
  constexpr HWY_FULL(int32_t) simd_tag;
  const auto v = BitCast(simd_tag, uv);
  return ((v >> Set(simd_tag, 1)) ^ (Zero(simd_tag)-(v & Set(simd_tag, 1))));
}

FORCE_INLINE Vec<HWY_FULL(uint64_t)> zig_zag_encode(
    Vec<HWY_FULL(int64_t)> v) noexcept {
  constexpr HWY_FULL(uint64_t) simd_tag;
  const auto uv = BitCast(simd_tag, v);
  return ((uv >> Set(simd_tag, 63)) ^ (uv << Set(simd_tag, 1)));
}

FORCE_INLINE Vec<HWY_FULL(int64_t)> zig_zag_decode(
    Vec<HWY_FULL(uint64_t)> uv) noexcept {
  constexpr HWY_FULL(int64_t) simd_tag;
  const auto v = BitCast(simd_tag, uv);
  return ((v >> Set(simd_tag, 1)) ^ (Zero(simd_tag)-(v & Set(simd_tag, 1))));
}

// FIXME do we need this?
template<size_t Length, bool Aligned, typename T>
void subtract(T* begin, T value) noexcept {
  using simd_helper = simd_helper<Aligned>;
  constexpr HWY_FULL(T) simd_tag;
  constexpr size_t Step = MaxLanes(simd_tag);
  constexpr size_t Unroll = 2*Step;
  static_assert(0 == (Length % (Unroll*Step)));

  size_t i = 0;

  const auto rhs = Set(simd_tag, value);
  for (; i < Length; i += Unroll*Step) {
    auto* p = begin + i;
    for (size_t j = 0; j < Unroll; ++j) {
      const auto lhs = simd_helper::load(simd_tag, p + j*Step);
      simd_helper::store(lhs - rhs, simd_tag, p + j*Step);
    }
  }
}

template<size_t Length, bool Aligned, typename T, int O = HWY_CAP_GE256>
void delta_encode(T* begin, T init) noexcept {
  static_assert(Length);
  using simd_helper = simd_helper<Aligned>;
  assert(std::is_sorted(begin, begin + Length));

  if constexpr (O == 1) { // 256-bit and greater
    constexpr HWY_FULL(T) simd_tag;
    constexpr size_t Step = MaxLanes(simd_tag);
    static_assert(0 == (Length % Step));

    size_t i = 0;
    for (; i < Step - size_t(Length > Step); ++i) {
      auto prev = begin[i];
      begin[i] -= init;
      init = prev;
    }

    if constexpr (Length > Step) {
      auto prev = LoadU(simd_tag, begin + i);
      begin[Step - 1] -= init;

      for (i = Step; i < Length - Step; i += Step) {
        const auto vec = simd_helper::load(simd_tag, begin + i);
        const auto delta = vec - prev;
        prev = LoadU(simd_tag, begin + i + Step - 1);
        simd_helper::store(delta, simd_tag, begin + i);
      }

      const auto vec = simd_helper::load(simd_tag, begin + i);
      simd_helper::store(vec - prev, simd_tag, begin + i);
    }
  } else if constexpr (O == 0) { // 128-bit
    // FIXME this is true only for 32-bit values
    constexpr HWY_CAPPED(T, 4) simd_tag;
    const size_t Step = Lanes(simd_tag);
    assert(0 == (Length % Step));

    auto prev = Set(simd_tag, init);

    for (size_t i = 0; i < Length; i += Step) {
      const auto vec = simd_helper::load(simd_tag, begin + i);
      const auto delta = vec - CombineShiftRightLanes<3>(vec, prev);
      simd_helper::store(delta, simd_tag, begin + i);
      prev = vec;
    }
  } else {
    static_assert(O < 2, "unkown optimization mode");
  }
}

// Encodes block denoted by [begin;end) using average encoding algorithm
// Returns block std::pair{ base, average }
template<
  size_t Length,
  bool Aligned,
  typename T,
  typename = std::enable_if_t<std::is_integral_v<T>>
> std::pair<T, T> avg_encode(T* begin) noexcept {
  using simd_helper = simd_helper<Aligned>;
  using signed_type = hwy::MakeSigned<T>;
  using unsigned_type = hwy::MakeUnsigned<T>;
  using float_type = hwy::MakeFloat<signed_type>;
  constexpr HWY_FULL(signed_type) simd_tag;
  constexpr HWY_FULL(unsigned_type) simd_unsigned_tag;
  constexpr size_t Step = MaxLanes(simd_tag);
  constexpr size_t Unroll = 2*Step;
  static_assert(Length);
  static_assert(0 == (Length % (Unroll*Step)));
  assert(begin[Length-1] >= begin[0]);

  const unsigned_type base = *begin;

  const signed_type avg = static_cast<signed_type>(
    static_cast<float_type>(begin[Length-1] - begin[0]) / std::max(size_t(1), Length - 1));

  auto vbase = Iota(simd_tag, 0) * Set(simd_tag, avg) + Set(simd_tag, base);
  const auto vavg = Set(simd_tag, avg) * Set(simd_tag, Step);

  for (size_t i = 0; i < Length; i += Unroll*Step) {
    auto* p = begin + i;
    for (size_t j = 0; j < Unroll; j++) {
      const auto v = simd_helper::load(simd_tag, reinterpret_cast<signed_type*>(p + j*Step)) - vbase;
      simd_helper::store(zig_zag_encode(v), simd_unsigned_tag, p + j*Step);
      vbase += vavg;
    }
  }

  // FIXME
  //  subtract min???

  return std::make_pair(base, avg);
}

template<
  size_t Length,
  bool Aligned,
  typename T,
  typename = std::enable_if_t<std::is_integral_v<T>>
> void avg_decode(const T* begin, T* out, T base, T avg) noexcept {
  using simd_helper = simd_helper<Aligned>;
  using signed_type = hwy::MakeSigned<T>;
  using unsigned_type = hwy::MakeUnsigned<T>;
  constexpr HWY_FULL(signed_type) simd_tag;
  constexpr HWY_FULL(unsigned_type) simd_unsigned_tag;
  constexpr size_t Step = MaxLanes(simd_tag);
  constexpr size_t Unroll = 2*Step;
  static_assert(Length);
  static_assert(0 == (Length % (Unroll*Step)));
  assert(begin[Length-1] >= begin[0]);

  auto vbase = Iota(simd_tag, 0) * Set(simd_tag, avg) + Set(simd_tag, base);
  const auto vavg = Set(simd_tag, avg) * Set(simd_tag, Step);

  for (size_t i = 0; i < Length; i += Unroll*Step) {
    auto* pin = begin + i;
    auto* pout = out + i;
    for (size_t j = 0; j < Unroll; j++) {
      const auto v = simd_helper::load(simd_unsigned_tag, pin + j*Step);
      simd_helper::store(zig_zag_decode(v) + vbase, simd_tag, reinterpret_cast<signed_type*>(pout + j*Step));
      vbase += vavg;
    }
  }
}

}
}

#endif // IRESEARCH_SIMD_UTILS_H
