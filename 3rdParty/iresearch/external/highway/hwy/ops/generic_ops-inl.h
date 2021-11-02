// Copyright 2021 Google LLC
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

// Target-independent types/functions defined after target-specific ops.

// Relies on the external include guard in highway.h.
HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

// The lane type of a vector type, e.g. float for Vec<Simd<float, 4>>.
template <class V>
using LaneType = decltype(GetLane(V()));

// Vector type, e.g. Vec128<float> for Simd<float, 4>. Useful as the return type
// of functions that do not take a vector argument, or as an argument type if
// the function only has a template argument for D, or for explicit type names
// instead of auto. This may be a built-in type.
template <class D>
using Vec = decltype(Zero(D()));

// Mask type. Useful as the return type of functions that do not take a mask
// argument, or as an argument type if the function only has a template argument
// for D, or for explicit type names instead of auto.
template <class D>
using Mask = decltype(MaskFromVec(Zero(D())));

// Returns the closest value to v within [lo, hi].
template <class V>
HWY_API V Clamp(const V v, const V lo, const V hi) {
  return Min(Max(lo, v), hi);
}

// CombineShiftRightBytes (and -Lanes) are not available for the scalar target,
// and RVV has its own implementation of -Lanes.
#if HWY_TARGET != HWY_SCALAR && HWY_TARGET != HWY_RVV

template <size_t kLanes, class D, class V = VFromD<D>>
HWY_API V CombineShiftRightLanes(D d, const V hi, const V lo) {
  constexpr size_t kBytes = kLanes * sizeof(LaneType<V>);
  static_assert(kBytes < 16, "Shift count is per-block");
  return CombineShiftRightBytes<kBytes>(d, hi, lo);
}

// DEPRECATED
template <size_t kLanes, class V>
HWY_API V CombineShiftRightLanes(const V hi, const V lo) {
  return CombineShiftRightLanes<kLanes>(DFromV<V>(), hi, lo);
}

#endif

// Returns lanes with the most significant bit set and all other bits zero.
template <class D>
HWY_API Vec<D> SignBit(D d) {
  using Unsigned = MakeUnsigned<TFromD<D>>;
  const Unsigned bit = Unsigned(1) << (sizeof(Unsigned) * 8 - 1);
  return BitCast(d, Set(Rebind<Unsigned, D>(), bit));
}

// Returns quiet NaN.
template <class D>
HWY_API Vec<D> NaN(D d) {
  const RebindToSigned<D> di;
  // LimitsMax sets all exponent and mantissa bits to 1. The exponent plus
  // mantissa MSB (to indicate quiet) would be sufficient.
  return BitCast(d, Set(di, LimitsMax<TFromD<decltype(di)>>()));
}

// ------------------------------ AESRound

// Cannot implement on scalar: need at least 16 bytes for TableLookupBytes.
#if HWY_TARGET != HWY_SCALAR

// Define for white-box testing, even if native instructions are available.
namespace detail {

// Constant-time: computes inverse in GF(2^4) based on "Accelerating AES with
// Vector Permute Instructions" and the accompanying assembly language
// implementation: https://crypto.stanford.edu/vpaes/vpaes.tgz. See also Botan:
// https://botan.randombit.net/doxygen/aes__vperm_8cpp_source.html .
//
// A brute-force 256 byte table lookup can also be made constant-time, and
// possibly competitive on NEON, but this is more performance-portable
// especially for x86 and large vectors.
template <class V>  // u8
HWY_INLINE V SubBytes(V state) {
  const DFromV<V> du;
  const auto mask = Set(du, 0xF);

  // Change polynomial basis to GF(2^4)
  {
    alignas(16) static constexpr uint8_t basisL[16] = {
        0x00, 0x70, 0x2A, 0x5A, 0x98, 0xE8, 0xB2, 0xC2,
        0x08, 0x78, 0x22, 0x52, 0x90, 0xE0, 0xBA, 0xCA};
    alignas(16) static constexpr uint8_t basisU[16] = {
        0x00, 0x4D, 0x7C, 0x31, 0x7D, 0x30, 0x01, 0x4C,
        0x81, 0xCC, 0xFD, 0xB0, 0xFC, 0xB1, 0x80, 0xCD};
    const auto sL = And(state, mask);
    const auto sU = ShiftRight<4>(state);  // byte shift => upper bits are zero
    const auto gf4L = TableLookupBytes(LoadDup128(du, basisL), sL);
    const auto gf4U = TableLookupBytes(LoadDup128(du, basisU), sU);
    state = Xor(gf4L, gf4U);
  }

  // Inversion in GF(2^4). Elements 0 represent "infinity" (division by 0) and
  // cause TableLookupBytesOr0 to return 0.
  alignas(16) static constexpr uint8_t kZetaInv[16] = {
      0x80, 7, 11, 15, 6, 10, 4, 1, 9, 8, 5, 2, 12, 14, 13, 3};
  alignas(16) static constexpr uint8_t kInv[16] = {
      0x80, 1, 8, 13, 15, 6, 5, 14, 2, 12, 11, 10, 9, 3, 7, 4};
  const auto tbl = LoadDup128(du, kInv);
  const auto sL = And(state, mask);      // L=low nibble, U=upper
  const auto sU = ShiftRight<4>(state);  // byte shift => upper bits are zero
  const auto sX = Xor(sU, sL);
  const auto invL = TableLookupBytes(LoadDup128(du, kZetaInv), sL);
  const auto invU = TableLookupBytes(tbl, sU);
  const auto invX = TableLookupBytes(tbl, sX);
  const auto outL = Xor(sX, TableLookupBytesOr0(tbl, Xor(invL, invU)));
  const auto outU = Xor(sU, TableLookupBytesOr0(tbl, Xor(invL, invX)));

  // Linear skew (cannot bake 0x63 bias into the table because out* indices
  // may have the infinity flag set).
  alignas(16) static constexpr uint8_t kAffineL[16] = {
      0x00, 0xC7, 0xBD, 0x6F, 0x17, 0x6D, 0xD2, 0xD0,
      0x78, 0xA8, 0x02, 0xC5, 0x7A, 0xBF, 0xAA, 0x15};
  alignas(16) static constexpr uint8_t kAffineU[16] = {
      0x00, 0x6A, 0xBB, 0x5F, 0xA5, 0x74, 0xE4, 0xCF,
      0xFA, 0x35, 0x2B, 0x41, 0xD1, 0x90, 0x1E, 0x8E};
  const auto affL = TableLookupBytesOr0(LoadDup128(du, kAffineL), outL);
  const auto affU = TableLookupBytesOr0(LoadDup128(du, kAffineU), outU);
  return Xor(Xor(affL, affU), Set(du, 0x63));
}

}  // namespace detail

#endif  // HWY_TARGET != HWY_SCALAR

// "Include guard": skip if native AES instructions are available.
#if (defined(HWY_NATIVE_AES) == defined(HWY_TARGET_TOGGLE))
#ifdef HWY_NATIVE_AES
#undef HWY_NATIVE_AES
#else
#define HWY_NATIVE_AES
#endif

// (Must come after HWY_TARGET_TOGGLE, else we don't reset it for scalar)
#if HWY_TARGET != HWY_SCALAR

namespace detail {

template <class V>  // u8
HWY_API V ShiftRows(const V state) {
  const DFromV<V> du;
  alignas(16) static constexpr uint8_t kShiftRow[16] = {
      0,  5,  10, 15,  // transposed: state is column major
      4,  9,  14, 3,   //
      8,  13, 2,  7,   //
      12, 1,  6,  11};
  const auto shift_row = LoadDup128(du, kShiftRow);
  return TableLookupBytes(state, shift_row);
}

template <class V>  // u8
HWY_API V MixColumns(const V state) {
  const DFromV<V> du;
  // For each column, the rows are the sum of GF(2^8) matrix multiplication by:
  // 2 3 1 1  // Let s := state*1, d := state*2, t := state*3.
  // 1 2 3 1  // d are on diagonal, no permutation needed.
  // 1 1 2 3  // t1230 indicates column indices of threes for the 4 rows.
  // 3 1 1 2  // We also need to compute s2301 and s3012 (=1230 o 2301).
  alignas(16) static constexpr uint8_t k2301[16] = {
      2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13};
  alignas(16) static constexpr uint8_t k1230[16] = {
      1, 2, 3, 0, 5, 6, 7, 4, 9, 10, 11, 8, 13, 14, 15, 12};
  const RebindToSigned<decltype(du)> di;  // can only do signed comparisons
  const auto msb = Lt(BitCast(di, state), Zero(di));
  const auto overflow = BitCast(du, IfThenElseZero(msb, Set(di, 0x1B)));
  const auto d = Xor(Add(state, state), overflow);  // = state*2 in GF(2^8).
  const auto s2301 = TableLookupBytes(state, LoadDup128(du, k2301));
  const auto d_s2301 = Xor(d, s2301);
  const auto t_s2301 = Xor(state, d_s2301);  // t(s*3) = XOR-sum {s, d(s*2)}
  const auto t1230_s3012 = TableLookupBytes(t_s2301, LoadDup128(du, k1230));
  return Xor(d_s2301, t1230_s3012);  // XOR-sum of 4 terms
}

}  // namespace detail

template <class V>  // u8
HWY_API V AESRound(V state, const V round_key) {
  // Intel docs swap the first two steps, but it does not matter because
  // ShiftRows is a permutation and SubBytes is independent of lane index.
  state = detail::SubBytes(state);
  state = detail::ShiftRows(state);
  state = detail::MixColumns(state);
  state = Xor(state, round_key);  // AddRoundKey
  return state;
}

// Constant-time implementation inspired by
// https://www.bearssl.org/constanttime.html, but about half the cost because we
// use 64x64 multiplies and 128-bit XORs.
template <class V>
HWY_API V CLMulLower(V a, V b) {
  const DFromV<V> d;
  static_assert(IsSame<TFromD<decltype(d)>, uint64_t>(), "V must be u64");
  const auto k1 = Set(d, 0x1111111111111111ULL);
  const auto k2 = Set(d, 0x2222222222222222ULL);
  const auto k4 = Set(d, 0x4444444444444444ULL);
  const auto k8 = Set(d, 0x8888888888888888ULL);
  const auto a0 = And(a, k1);
  const auto a1 = And(a, k2);
  const auto a2 = And(a, k4);
  const auto a3 = And(a, k8);
  const auto b0 = And(b, k1);
  const auto b1 = And(b, k2);
  const auto b2 = And(b, k4);
  const auto b3 = And(b, k8);

  auto m0 = Xor(MulEven(a0, b0), MulEven(a1, b3));
  auto m1 = Xor(MulEven(a0, b1), MulEven(a1, b0));
  auto m2 = Xor(MulEven(a0, b2), MulEven(a1, b1));
  auto m3 = Xor(MulEven(a0, b3), MulEven(a1, b2));
  m0 = Xor(m0, Xor(MulEven(a2, b2), MulEven(a3, b1)));
  m1 = Xor(m1, Xor(MulEven(a2, b3), MulEven(a3, b2)));
  m2 = Xor(m2, Xor(MulEven(a2, b0), MulEven(a3, b3)));
  m3 = Xor(m3, Xor(MulEven(a2, b1), MulEven(a3, b0)));
  return Or(Or(And(m0, k1), And(m1, k2)), Or(And(m2, k4), And(m3, k8)));
}

template <class V>
HWY_API V CLMulUpper(V a, V b) {
  const DFromV<V> d;
  static_assert(IsSame<TFromD<decltype(d)>, uint64_t>(), "V must be u64");
  const auto k1 = Set(d, 0x1111111111111111ULL);
  const auto k2 = Set(d, 0x2222222222222222ULL);
  const auto k4 = Set(d, 0x4444444444444444ULL);
  const auto k8 = Set(d, 0x8888888888888888ULL);
  const auto a0 = And(a, k1);
  const auto a1 = And(a, k2);
  const auto a2 = And(a, k4);
  const auto a3 = And(a, k8);
  const auto b0 = And(b, k1);
  const auto b1 = And(b, k2);
  const auto b2 = And(b, k4);
  const auto b3 = And(b, k8);

  auto m0 = Xor(MulOdd(a0, b0), MulOdd(a1, b3));
  auto m1 = Xor(MulOdd(a0, b1), MulOdd(a1, b0));
  auto m2 = Xor(MulOdd(a0, b2), MulOdd(a1, b1));
  auto m3 = Xor(MulOdd(a0, b3), MulOdd(a1, b2));
  m0 = Xor(m0, Xor(MulOdd(a2, b2), MulOdd(a3, b1)));
  m1 = Xor(m1, Xor(MulOdd(a2, b3), MulOdd(a3, b2)));
  m2 = Xor(m2, Xor(MulOdd(a2, b0), MulOdd(a3, b3)));
  m3 = Xor(m3, Xor(MulOdd(a2, b1), MulOdd(a3, b0)));
  return Or(Or(And(m0, k1), And(m1, k2)), Or(And(m2, k4), And(m3, k8)));
}

#endif  // HWY_NATIVE_AES
#endif  // HWY_TARGET != HWY_SCALAR

// "Include guard": skip if native POPCNT-related instructions are available.
#if (defined(HWY_NATIVE_POPCNT) == defined(HWY_TARGET_TOGGLE))
#ifdef HWY_NATIVE_POPCNT
#undef HWY_NATIVE_POPCNT
#else
#define HWY_NATIVE_POPCNT
#endif

template <typename V, HWY_IF_LANES_ARE(uint8_t, V)>
HWY_API V PopulationCount(V v) {
  constexpr DFromV<V> d;
  HWY_ALIGN constexpr uint8_t kLookup[16] = {
      0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
  };
  auto lo = And(v, Set(d, 0xF));
  auto hi = ShiftRight<4>(v);
  auto lookup = LoadDup128(Simd<uint8_t, HWY_MAX(16, MaxLanes(d))>(), kLookup);
  return Add(TableLookupBytes(lookup, hi), TableLookupBytes(lookup, lo));
}

template <typename V, HWY_IF_LANES_ARE(uint16_t, V)>
HWY_API V PopulationCount(V v) {
  const DFromV<V> d;
  Repartition<uint8_t, decltype(d)> d8;
  auto vals = BitCast(d, PopulationCount(BitCast(d8, v)));
  return Add(ShiftRight<8>(vals), And(vals, Set(d, 0xFF)));
}

template <typename V, HWY_IF_LANES_ARE(uint32_t, V)>
HWY_API V PopulationCount(V v) {
  const DFromV<V> d;
  Repartition<uint16_t, decltype(d)> d16;
  auto vals = BitCast(d, PopulationCount(BitCast(d16, v)));
  return Add(ShiftRight<16>(vals), And(vals, Set(d, 0xFF)));
}

#if HWY_CAP_INTEGER64
template <typename V, HWY_IF_LANES_ARE(uint64_t, V)>
HWY_API V PopulationCount(V v) {
  const DFromV<V> d;
  Repartition<uint32_t, decltype(d)> d32;
  auto vals = BitCast(d, PopulationCount(BitCast(d32, v)));
  return Add(ShiftRight<32>(vals), And(vals, Set(d, 0xFF)));
}
#endif

#endif  // HWY_NATIVE_POPCNT

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
