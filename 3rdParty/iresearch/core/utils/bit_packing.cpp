////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "bit_packing.hpp"

#include <cassert>
#include <cstring>

namespace {

#if defined(_MSC_VER)
  __pragma(warning(push))
  __pragma(warning(disable:4127)) // constexp conditionals are intended to be optimized out
  __pragma(warning(disable:4293)) // all negative shifts are masked by constexpr conditionals
  __pragma(warning(disable:4724)) // all X % zero are masked by constexpr conditionals (must disable outside of fn body)
#elif defined (__GNUC__)
  #pragma GCC diagnostic push
  #if (__GNUC__ >= 5)
    #pragma GCC diagnostic ignored "-Wshift-count-negative" // all negative shifts are masked by constexpr conditionals
  #endif
#endif


template<int N>
void fastpack(const uint32_t* RESTRICT in, uint32_t* RESTRICT out) noexcept {
  // 32 == sizeof(uint32_t) * 8
  static_assert(N > 0 && N <= 32, "N <= 0 || N > 32");
  // ensure all computations are constexr, i.e. no conditional jumps, no loops, no variable increment/decrement
        *out |= ((*in) % (1U << N)) << (N *  0) % 32; if constexpr (((N *  1) % 32) < ((N *  0) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N *  1) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N *  1) % 32; if constexpr (((N *  2) % 32) < ((N *  1) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N *  2) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N *  2) % 32; if constexpr (((N *  3) % 32) < ((N *  2) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N *  3) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N *  3) % 32; if constexpr (((N *  4) % 32) < ((N *  3) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N *  4) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N *  4) % 32; if constexpr (((N *  5) % 32) < ((N *  4) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N *  5) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N *  5) % 32; if constexpr (((N *  6) % 32) < ((N *  5) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N *  6) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N *  6) % 32; if constexpr (((N *  7) % 32) < ((N *  6) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N *  7) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N *  7) % 32; if constexpr (((N *  8) % 32) < ((N *  7) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N *  8) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N *  8) % 32; if constexpr (((N *  9) % 32) < ((N *  8) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N *  9) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N *  9) % 32; if constexpr (((N * 10) % 32) < ((N *  9) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 10) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 10) % 32; if constexpr (((N * 11) % 32) < ((N * 10) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 11) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 11) % 32; if constexpr (((N * 12) % 32) < ((N * 11) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 12) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 12) % 32; if constexpr (((N * 13) % 32) < ((N * 12) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 13) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 13) % 32; if constexpr (((N * 14) % 32) < ((N * 13) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 14) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 14) % 32; if constexpr (((N * 15) % 32) < ((N * 14) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 15) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 15) % 32; if constexpr (((N * 16) % 32) < ((N * 15) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 16) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 16) % 32; if constexpr (((N * 17) % 32) < ((N * 16) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 17) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 17) % 32; if constexpr (((N * 18) % 32) < ((N * 17) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 18) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 18) % 32; if constexpr (((N * 19) % 32) < ((N * 18) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 19) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 19) % 32; if constexpr (((N * 20) % 32) < ((N * 19) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 20) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 20) % 32; if constexpr (((N * 21) % 32) < ((N * 20) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 21) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 21) % 32; if constexpr (((N * 22) % 32) < ((N * 21) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 22) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 22) % 32; if constexpr (((N * 23) % 32) < ((N * 22) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 23) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 23) % 32; if constexpr (((N * 24) % 32) < ((N * 23) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 24) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 24) % 32; if constexpr (((N * 25) % 32) < ((N * 24) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 25) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 25) % 32; if constexpr (((N * 26) % 32) < ((N * 25) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 26) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 26) % 32; if constexpr (((N * 27) % 32) < ((N * 26) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 27) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 27) % 32; if constexpr (((N * 28) % 32) < ((N * 27) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 28) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 28) % 32; if constexpr (((N * 29) % 32) < ((N * 28) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 29) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 29) % 32; if constexpr (((N * 30) % 32) < ((N * 29) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 30) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 30) % 32; if constexpr (((N * 31) % 32) < ((N * 30) % 32)) { ++out; *out |= ((*in) % (1U << N)) >> (N - ((N * 31) % 32)); }
  ++in; *out |= ((*in) % (1U << N)) << (N * 31) % 32;
}

template<>
void fastpack<32>(const uint32_t* RESTRICT in, uint32_t* RESTRICT out) noexcept {
  std::memcpy(out, in, sizeof(uint32_t)*irs::packed::BLOCK_SIZE_32);
}

template<int N>
void fastpack(const uint64_t* RESTRICT in, uint64_t* RESTRICT out) noexcept {
  // 64 == sizeof(uint64_t) * 8
  static_assert(N > 0 && N <= 64, "N <= 0 || N > 64");
  // ensure all computations are constexr, i.e. no conditional jumps, no loops, no variable increment/decrement
        *out |= ((*in) % (1ULL << N)) << (N *  0) % 64; if constexpr (((N *  1) % 64) < ((N *  0) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N *  1) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N *  1) % 64; if constexpr (((N *  2) % 64) < ((N *  1) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N *  2) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N *  2) % 64; if constexpr (((N *  3) % 64) < ((N *  2) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N *  3) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N *  3) % 64; if constexpr (((N *  4) % 64) < ((N *  3) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N *  4) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N *  4) % 64; if constexpr (((N *  5) % 64) < ((N *  4) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N *  5) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N *  5) % 64; if constexpr (((N *  6) % 64) < ((N *  5) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N *  6) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N *  6) % 64; if constexpr (((N *  7) % 64) < ((N *  6) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N *  7) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N *  7) % 64; if constexpr (((N *  8) % 64) < ((N *  7) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N *  8) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N *  8) % 64; if constexpr (((N *  9) % 64) < ((N *  8) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N *  9) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N *  9) % 64; if constexpr (((N * 10) % 64) < ((N *  9) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 10) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 10) % 64; if constexpr (((N * 11) % 64) < ((N * 10) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 11) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 11) % 64; if constexpr (((N * 12) % 64) < ((N * 11) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 12) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 12) % 64; if constexpr (((N * 13) % 64) < ((N * 12) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 13) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 13) % 64; if constexpr (((N * 14) % 64) < ((N * 13) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 14) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 14) % 64; if constexpr (((N * 15) % 64) < ((N * 14) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 15) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 15) % 64; if constexpr (((N * 16) % 64) < ((N * 15) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 16) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 16) % 64; if constexpr (((N * 17) % 64) < ((N * 16) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 17) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 17) % 64; if constexpr (((N * 18) % 64) < ((N * 17) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 18) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 18) % 64; if constexpr (((N * 19) % 64) < ((N * 18) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 19) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 19) % 64; if constexpr (((N * 20) % 64) < ((N * 19) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 20) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 20) % 64; if constexpr (((N * 21) % 64) < ((N * 20) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 21) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 21) % 64; if constexpr (((N * 22) % 64) < ((N * 21) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 22) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 22) % 64; if constexpr (((N * 23) % 64) < ((N * 22) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 23) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 23) % 64; if constexpr (((N * 24) % 64) < ((N * 23) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 24) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 24) % 64; if constexpr (((N * 25) % 64) < ((N * 24) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 25) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 25) % 64; if constexpr (((N * 26) % 64) < ((N * 25) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 26) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 26) % 64; if constexpr (((N * 27) % 64) < ((N * 26) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 27) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 27) % 64; if constexpr (((N * 28) % 64) < ((N * 27) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 28) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 28) % 64; if constexpr (((N * 29) % 64) < ((N * 28) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 29) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 29) % 64; if constexpr (((N * 30) % 64) < ((N * 29) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 30) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 30) % 64; if constexpr (((N * 31) % 64) < ((N * 30) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 31) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 31) % 64; if constexpr (((N * 32) % 64) < ((N * 31) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 32) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 32) % 64; if constexpr (((N * 33) % 64) < ((N * 32) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 33) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 33) % 64; if constexpr (((N * 34) % 64) < ((N * 33) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 34) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 34) % 64; if constexpr (((N * 35) % 64) < ((N * 34) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 35) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 35) % 64; if constexpr (((N * 36) % 64) < ((N * 35) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 36) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 36) % 64; if constexpr (((N * 37) % 64) < ((N * 36) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 37) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 37) % 64; if constexpr (((N * 38) % 64) < ((N * 37) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 38) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 38) % 64; if constexpr (((N * 39) % 64) < ((N * 38) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 39) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 39) % 64; if constexpr (((N * 40) % 64) < ((N * 39) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 40) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 40) % 64; if constexpr (((N * 41) % 64) < ((N * 40) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 41) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 41) % 64; if constexpr (((N * 42) % 64) < ((N * 41) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 42) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 42) % 64; if constexpr (((N * 43) % 64) < ((N * 42) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 43) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 43) % 64; if constexpr (((N * 44) % 64) < ((N * 43) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 44) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 44) % 64; if constexpr (((N * 45) % 64) < ((N * 44) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 45) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 45) % 64; if constexpr (((N * 46) % 64) < ((N * 45) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 46) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 46) % 64; if constexpr (((N * 47) % 64) < ((N * 46) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 47) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 47) % 64; if constexpr (((N * 48) % 64) < ((N * 47) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 48) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 48) % 64; if constexpr (((N * 49) % 64) < ((N * 48) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 49) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 49) % 64; if constexpr (((N * 50) % 64) < ((N * 49) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 50) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 50) % 64; if constexpr (((N * 51) % 64) < ((N * 50) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 51) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 51) % 64; if constexpr (((N * 52) % 64) < ((N * 51) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 52) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 52) % 64; if constexpr (((N * 53) % 64) < ((N * 52) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 53) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 53) % 64; if constexpr (((N * 54) % 64) < ((N * 53) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 54) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 54) % 64; if constexpr (((N * 55) % 64) < ((N * 54) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 55) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 55) % 64; if constexpr (((N * 56) % 64) < ((N * 55) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 56) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 56) % 64; if constexpr (((N * 57) % 64) < ((N * 56) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 57) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 57) % 64; if constexpr (((N * 58) % 64) < ((N * 57) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 58) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 58) % 64; if constexpr (((N * 59) % 64) < ((N * 58) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 59) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 59) % 64; if constexpr (((N * 60) % 64) < ((N * 59) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 60) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 60) % 64; if constexpr (((N * 61) % 64) < ((N * 60) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 61) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 61) % 64; if constexpr (((N * 62) % 64) < ((N * 61) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 62) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 62) % 64; if constexpr (((N * 63) % 64) < ((N * 62) % 64)) { ++out; *out |= ((*in) % (1ULL << N)) >> (N - ((N * 63) % 64)); }
  ++in; *out |= ((*in) % (1ULL << N)) << (N * 63) % 64;
}

template<>
FORCE_INLINE void fastpack<64>(const uint64_t* RESTRICT in, uint64_t* RESTRICT out) noexcept {
  std::memcpy(out, in, sizeof(uint64_t)*irs::packed::BLOCK_SIZE_64);
}

template<int N>
void fastunpack(const uint32_t* RESTRICT in, uint32_t* RESTRICT out) noexcept {
  // 32 == sizeof(uint32_t) * 8
  static_assert(N > 0 && N <= 32, "N <= 0 || N > 32");
         *out = ((*in) >> (N *  0) % 32) % (1U << N); if constexpr (((N *  1) % 32) < ((N *  0) % 32)) { ++in; *out |= ((*in) % (1U << (N *  1) % 32)) << (N - ((N *  1) % 32)); }
  out++; *out = ((*in) >> (N *  1) % 32) % (1U << N); if constexpr (((N *  2) % 32) < ((N *  1) % 32)) { ++in; *out |= ((*in) % (1U << (N *  2) % 32)) << (N - ((N *  2) % 32)); }
  out++; *out = ((*in) >> (N *  2) % 32) % (1U << N); if constexpr (((N *  3) % 32) < ((N *  2) % 32)) { ++in; *out |= ((*in) % (1U << (N *  3) % 32)) << (N - ((N *  3) % 32)); }
  out++; *out = ((*in) >> (N *  3) % 32) % (1U << N); if constexpr (((N *  4) % 32) < ((N *  3) % 32)) { ++in; *out |= ((*in) % (1U << (N *  4) % 32)) << (N - ((N *  4) % 32)); }
  out++; *out = ((*in) >> (N *  4) % 32) % (1U << N); if constexpr (((N *  5) % 32) < ((N *  4) % 32)) { ++in; *out |= ((*in) % (1U << (N *  5) % 32)) << (N - ((N *  5) % 32)); }
  out++; *out = ((*in) >> (N *  5) % 32) % (1U << N); if constexpr (((N *  6) % 32) < ((N *  5) % 32)) { ++in; *out |= ((*in) % (1U << (N *  6) % 32)) << (N - ((N *  6) % 32)); }
  out++; *out = ((*in) >> (N *  6) % 32) % (1U << N); if constexpr (((N *  7) % 32) < ((N *  6) % 32)) { ++in; *out |= ((*in) % (1U << (N *  7) % 32)) << (N - ((N *  7) % 32)); }
  out++; *out = ((*in) >> (N *  7) % 32) % (1U << N); if constexpr (((N *  8) % 32) < ((N *  7) % 32)) { ++in; *out |= ((*in) % (1U << (N *  8) % 32)) << (N - ((N *  8) % 32)); }
  out++; *out = ((*in) >> (N *  8) % 32) % (1U << N); if constexpr (((N *  9) % 32) < ((N *  8) % 32)) { ++in; *out |= ((*in) % (1U << (N *  9) % 32)) << (N - ((N *  9) % 32)); }
  out++; *out = ((*in) >> (N *  9) % 32) % (1U << N); if constexpr (((N * 10) % 32) < ((N *  9) % 32)) { ++in; *out |= ((*in) % (1U << (N * 10) % 32)) << (N - ((N * 10) % 32)); }
  out++; *out = ((*in) >> (N * 10) % 32) % (1U << N); if constexpr (((N * 11) % 32) < ((N * 10) % 32)) { ++in; *out |= ((*in) % (1U << (N * 11) % 32)) << (N - ((N * 11) % 32)); }
  out++; *out = ((*in) >> (N * 11) % 32) % (1U << N); if constexpr (((N * 12) % 32) < ((N * 11) % 32)) { ++in; *out |= ((*in) % (1U << (N * 12) % 32)) << (N - ((N * 12) % 32)); }
  out++; *out = ((*in) >> (N * 12) % 32) % (1U << N); if constexpr (((N * 13) % 32) < ((N * 12) % 32)) { ++in; *out |= ((*in) % (1U << (N * 13) % 32)) << (N - ((N * 13) % 32)); }
  out++; *out = ((*in) >> (N * 13) % 32) % (1U << N); if constexpr (((N * 14) % 32) < ((N * 13) % 32)) { ++in; *out |= ((*in) % (1U << (N * 14) % 32)) << (N - ((N * 14) % 32)); }
  out++; *out = ((*in) >> (N * 14) % 32) % (1U << N); if constexpr (((N * 15) % 32) < ((N * 14) % 32)) { ++in; *out |= ((*in) % (1U << (N * 15) % 32)) << (N - ((N * 15) % 32)); }
  out++; *out = ((*in) >> (N * 15) % 32) % (1U << N); if constexpr (((N * 16) % 32) < ((N * 15) % 32)) { ++in; *out |= ((*in) % (1U << (N * 16) % 32)) << (N - ((N * 16) % 32)); }
  out++; *out = ((*in) >> (N * 16) % 32) % (1U << N); if constexpr (((N * 17) % 32) < ((N * 16) % 32)) { ++in; *out |= ((*in) % (1U << (N * 17) % 32)) << (N - ((N * 17) % 32)); }
  out++; *out = ((*in) >> (N * 17) % 32) % (1U << N); if constexpr (((N * 18) % 32) < ((N * 17) % 32)) { ++in; *out |= ((*in) % (1U << (N * 18) % 32)) << (N - ((N * 18) % 32)); }
  out++; *out = ((*in) >> (N * 18) % 32) % (1U << N); if constexpr (((N * 19) % 32) < ((N * 18) % 32)) { ++in; *out |= ((*in) % (1U << (N * 19) % 32)) << (N - ((N * 19) % 32)); }
  out++; *out = ((*in) >> (N * 19) % 32) % (1U << N); if constexpr (((N * 20) % 32) < ((N * 19) % 32)) { ++in; *out |= ((*in) % (1U << (N * 20) % 32)) << (N - ((N * 20) % 32)); }
  out++; *out = ((*in) >> (N * 20) % 32) % (1U << N); if constexpr (((N * 21) % 32) < ((N * 20) % 32)) { ++in; *out |= ((*in) % (1U << (N * 21) % 32)) << (N - ((N * 21) % 32)); }
  out++; *out = ((*in) >> (N * 21) % 32) % (1U << N); if constexpr (((N * 22) % 32) < ((N * 21) % 32)) { ++in; *out |= ((*in) % (1U << (N * 22) % 32)) << (N - ((N * 22) % 32)); }
  out++; *out = ((*in) >> (N * 22) % 32) % (1U << N); if constexpr (((N * 23) % 32) < ((N * 22) % 32)) { ++in; *out |= ((*in) % (1U << (N * 23) % 32)) << (N - ((N * 23) % 32)); }
  out++; *out = ((*in) >> (N * 23) % 32) % (1U << N); if constexpr (((N * 24) % 32) < ((N * 23) % 32)) { ++in; *out |= ((*in) % (1U << (N * 24) % 32)) << (N - ((N * 24) % 32)); }
  out++; *out = ((*in) >> (N * 24) % 32) % (1U << N); if constexpr (((N * 25) % 32) < ((N * 24) % 32)) { ++in; *out |= ((*in) % (1U << (N * 25) % 32)) << (N - ((N * 25) % 32)); }
  out++; *out = ((*in) >> (N * 25) % 32) % (1U << N); if constexpr (((N * 26) % 32) < ((N * 25) % 32)) { ++in; *out |= ((*in) % (1U << (N * 26) % 32)) << (N - ((N * 26) % 32)); }
  out++; *out = ((*in) >> (N * 26) % 32) % (1U << N); if constexpr (((N * 27) % 32) < ((N * 26) % 32)) { ++in; *out |= ((*in) % (1U << (N * 27) % 32)) << (N - ((N * 27) % 32)); }
  out++; *out = ((*in) >> (N * 27) % 32) % (1U << N); if constexpr (((N * 28) % 32) < ((N * 27) % 32)) { ++in; *out |= ((*in) % (1U << (N * 28) % 32)) << (N - ((N * 28) % 32)); }
  out++; *out = ((*in) >> (N * 28) % 32) % (1U << N); if constexpr (((N * 29) % 32) < ((N * 28) % 32)) { ++in; *out |= ((*in) % (1U << (N * 29) % 32)) << (N - ((N * 29) % 32)); }
  out++; *out = ((*in) >> (N * 29) % 32) % (1U << N); if constexpr (((N * 30) % 32) < ((N * 29) % 32)) { ++in; *out |= ((*in) % (1U << (N * 30) % 32)) << (N - ((N * 30) % 32)); }
  out++; *out = ((*in) >> (N * 30) % 32) % (1U << N); if constexpr (((N * 31) % 32) < ((N * 30) % 32)) { ++in; *out |= ((*in) % (1U << (N * 31) % 32)) << (N - ((N * 31) % 32)); }
  out++; *out = ((*in) >> (N * 31) % 32) % (1U << N);
}

template<>
FORCE_INLINE void fastunpack<32>(const uint32_t* RESTRICT in, uint32_t* RESTRICT out) noexcept {
  std::memcpy(out, in, sizeof(uint32_t)*irs::packed::BLOCK_SIZE_32);
}

template<int N>
void fastunpack(const uint64_t* RESTRICT in, uint64_t* RESTRICT out) noexcept {
  // 64 == sizeof(uint32_t) * 8
  static_assert(N > 0 && N <= 64, "N <= 0 || N > 64");
         *out = ((*in) >> (N *  0) % 64) % (1ULL << N); if constexpr (((N *  1) % 64) < ((N *  0) % 64)) { ++in; *out |= ((*in) % (1ULL << (N *  1) % 64)) << (N - ((N *  1) % 64)); }
  out++; *out = ((*in) >> (N *  1) % 64) % (1ULL << N); if constexpr (((N *  2) % 64) < ((N *  1) % 64)) { ++in; *out |= ((*in) % (1ULL << (N *  2) % 64)) << (N - ((N *  2) % 64)); }
  out++; *out = ((*in) >> (N *  2) % 64) % (1ULL << N); if constexpr (((N *  3) % 64) < ((N *  2) % 64)) { ++in; *out |= ((*in) % (1ULL << (N *  3) % 64)) << (N - ((N *  3) % 64)); }
  out++; *out = ((*in) >> (N *  3) % 64) % (1ULL << N); if constexpr (((N *  4) % 64) < ((N *  3) % 64)) { ++in; *out |= ((*in) % (1ULL << (N *  4) % 64)) << (N - ((N *  4) % 64)); }
  out++; *out = ((*in) >> (N *  4) % 64) % (1ULL << N); if constexpr (((N *  5) % 64) < ((N *  4) % 64)) { ++in; *out |= ((*in) % (1ULL << (N *  5) % 64)) << (N - ((N *  5) % 64)); }
  out++; *out = ((*in) >> (N *  5) % 64) % (1ULL << N); if constexpr (((N *  6) % 64) < ((N *  5) % 64)) { ++in; *out |= ((*in) % (1ULL << (N *  6) % 64)) << (N - ((N *  6) % 64)); }
  out++; *out = ((*in) >> (N *  6) % 64) % (1ULL << N); if constexpr (((N *  7) % 64) < ((N *  6) % 64)) { ++in; *out |= ((*in) % (1ULL << (N *  7) % 64)) << (N - ((N *  7) % 64)); }
  out++; *out = ((*in) >> (N *  7) % 64) % (1ULL << N); if constexpr (((N *  8) % 64) < ((N *  7) % 64)) { ++in; *out |= ((*in) % (1ULL << (N *  8) % 64)) << (N - ((N *  8) % 64)); }
  out++; *out = ((*in) >> (N *  8) % 64) % (1ULL << N); if constexpr (((N *  9) % 64) < ((N *  8) % 64)) { ++in; *out |= ((*in) % (1ULL << (N *  9) % 64)) << (N - ((N *  9) % 64)); }
  out++; *out = ((*in) >> (N *  9) % 64) % (1ULL << N); if constexpr (((N * 10) % 64) < ((N *  9) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 10) % 64)) << (N - ((N * 10) % 64)); }
  out++; *out = ((*in) >> (N * 10) % 64) % (1ULL << N); if constexpr (((N * 11) % 64) < ((N * 10) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 11) % 64)) << (N - ((N * 11) % 64)); }
  out++; *out = ((*in) >> (N * 11) % 64) % (1ULL << N); if constexpr (((N * 12) % 64) < ((N * 11) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 12) % 64)) << (N - ((N * 12) % 64)); }
  out++; *out = ((*in) >> (N * 12) % 64) % (1ULL << N); if constexpr (((N * 13) % 64) < ((N * 12) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 13) % 64)) << (N - ((N * 13) % 64)); }
  out++; *out = ((*in) >> (N * 13) % 64) % (1ULL << N); if constexpr (((N * 14) % 64) < ((N * 13) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 14) % 64)) << (N - ((N * 14) % 64)); }
  out++; *out = ((*in) >> (N * 14) % 64) % (1ULL << N); if constexpr (((N * 15) % 64) < ((N * 14) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 15) % 64)) << (N - ((N * 15) % 64)); }
  out++; *out = ((*in) >> (N * 15) % 64) % (1ULL << N); if constexpr (((N * 16) % 64) < ((N * 15) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 16) % 64)) << (N - ((N * 16) % 64)); }
  out++; *out = ((*in) >> (N * 16) % 64) % (1ULL << N); if constexpr (((N * 17) % 64) < ((N * 16) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 17) % 64)) << (N - ((N * 17) % 64)); }
  out++; *out = ((*in) >> (N * 17) % 64) % (1ULL << N); if constexpr (((N * 18) % 64) < ((N * 17) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 18) % 64)) << (N - ((N * 18) % 64)); }
  out++; *out = ((*in) >> (N * 18) % 64) % (1ULL << N); if constexpr (((N * 19) % 64) < ((N * 18) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 19) % 64)) << (N - ((N * 19) % 64)); }
  out++; *out = ((*in) >> (N * 19) % 64) % (1ULL << N); if constexpr (((N * 20) % 64) < ((N * 19) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 20) % 64)) << (N - ((N * 20) % 64)); }
  out++; *out = ((*in) >> (N * 20) % 64) % (1ULL << N); if constexpr (((N * 21) % 64) < ((N * 20) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 21) % 64)) << (N - ((N * 21) % 64)); }
  out++; *out = ((*in) >> (N * 21) % 64) % (1ULL << N); if constexpr (((N * 22) % 64) < ((N * 21) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 22) % 64)) << (N - ((N * 22) % 64)); }
  out++; *out = ((*in) >> (N * 22) % 64) % (1ULL << N); if constexpr (((N * 23) % 64) < ((N * 22) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 23) % 64)) << (N - ((N * 23) % 64)); }
  out++; *out = ((*in) >> (N * 23) % 64) % (1ULL << N); if constexpr (((N * 24) % 64) < ((N * 23) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 24) % 64)) << (N - ((N * 24) % 64)); }
  out++; *out = ((*in) >> (N * 24) % 64) % (1ULL << N); if constexpr (((N * 25) % 64) < ((N * 24) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 25) % 64)) << (N - ((N * 25) % 64)); }
  out++; *out = ((*in) >> (N * 25) % 64) % (1ULL << N); if constexpr (((N * 26) % 64) < ((N * 25) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 26) % 64)) << (N - ((N * 26) % 64)); }
  out++; *out = ((*in) >> (N * 26) % 64) % (1ULL << N); if constexpr (((N * 27) % 64) < ((N * 26) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 27) % 64)) << (N - ((N * 27) % 64)); }
  out++; *out = ((*in) >> (N * 27) % 64) % (1ULL << N); if constexpr (((N * 28) % 64) < ((N * 27) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 28) % 64)) << (N - ((N * 28) % 64)); }
  out++; *out = ((*in) >> (N * 28) % 64) % (1ULL << N); if constexpr (((N * 29) % 64) < ((N * 28) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 29) % 64)) << (N - ((N * 29) % 64)); }
  out++; *out = ((*in) >> (N * 29) % 64) % (1ULL << N); if constexpr (((N * 30) % 64) < ((N * 29) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 30) % 64)) << (N - ((N * 30) % 64)); }
  out++; *out = ((*in) >> (N * 30) % 64) % (1ULL << N); if constexpr (((N * 31) % 64) < ((N * 30) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 31) % 64)) << (N - ((N * 31) % 64)); }
  out++; *out = ((*in) >> (N * 31) % 64) % (1ULL << N); if constexpr (((N * 32) % 64) < ((N * 31) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 32) % 64)) << (N - ((N * 32) % 64)); }
  out++; *out = ((*in) >> (N * 32) % 64) % (1ULL << N); if constexpr (((N * 33) % 64) < ((N * 32) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 33) % 64)) << (N - ((N * 33) % 64)); }
  out++; *out = ((*in) >> (N * 33) % 64) % (1ULL << N); if constexpr (((N * 34) % 64) < ((N * 33) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 34) % 64)) << (N - ((N * 34) % 64)); }
  out++; *out = ((*in) >> (N * 34) % 64) % (1ULL << N); if constexpr (((N * 35) % 64) < ((N * 34) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 35) % 64)) << (N - ((N * 35) % 64)); }
  out++; *out = ((*in) >> (N * 35) % 64) % (1ULL << N); if constexpr (((N * 36) % 64) < ((N * 35) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 36) % 64)) << (N - ((N * 36) % 64)); }
  out++; *out = ((*in) >> (N * 36) % 64) % (1ULL << N); if constexpr (((N * 37) % 64) < ((N * 36) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 37) % 64)) << (N - ((N * 37) % 64)); }
  out++; *out = ((*in) >> (N * 37) % 64) % (1ULL << N); if constexpr (((N * 38) % 64) < ((N * 37) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 38) % 64)) << (N - ((N * 38) % 64)); }
  out++; *out = ((*in) >> (N * 38) % 64) % (1ULL << N); if constexpr (((N * 39) % 64) < ((N * 38) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 39) % 64)) << (N - ((N * 39) % 64)); }
  out++; *out = ((*in) >> (N * 39) % 64) % (1ULL << N); if constexpr (((N * 40) % 64) < ((N * 39) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 40) % 64)) << (N - ((N * 40) % 64)); }
  out++; *out = ((*in) >> (N * 40) % 64) % (1ULL << N); if constexpr (((N * 41) % 64) < ((N * 40) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 41) % 64)) << (N - ((N * 41) % 64)); }
  out++; *out = ((*in) >> (N * 41) % 64) % (1ULL << N); if constexpr (((N * 42) % 64) < ((N * 41) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 42) % 64)) << (N - ((N * 42) % 64)); }
  out++; *out = ((*in) >> (N * 42) % 64) % (1ULL << N); if constexpr (((N * 43) % 64) < ((N * 42) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 43) % 64)) << (N - ((N * 43) % 64)); }
  out++; *out = ((*in) >> (N * 43) % 64) % (1ULL << N); if constexpr (((N * 44) % 64) < ((N * 43) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 44) % 64)) << (N - ((N * 44) % 64)); }
  out++; *out = ((*in) >> (N * 44) % 64) % (1ULL << N); if constexpr (((N * 45) % 64) < ((N * 44) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 45) % 64)) << (N - ((N * 45) % 64)); }
  out++; *out = ((*in) >> (N * 45) % 64) % (1ULL << N); if constexpr (((N * 46) % 64) < ((N * 45) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 46) % 64)) << (N - ((N * 46) % 64)); }
  out++; *out = ((*in) >> (N * 46) % 64) % (1ULL << N); if constexpr (((N * 47) % 64) < ((N * 46) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 47) % 64)) << (N - ((N * 47) % 64)); }
  out++; *out = ((*in) >> (N * 47) % 64) % (1ULL << N); if constexpr (((N * 48) % 64) < ((N * 47) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 48) % 64)) << (N - ((N * 48) % 64)); }
  out++; *out = ((*in) >> (N * 48) % 64) % (1ULL << N); if constexpr (((N * 49) % 64) < ((N * 48) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 49) % 64)) << (N - ((N * 49) % 64)); }
  out++; *out = ((*in) >> (N * 49) % 64) % (1ULL << N); if constexpr (((N * 50) % 64) < ((N * 49) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 50) % 64)) << (N - ((N * 50) % 64)); }
  out++; *out = ((*in) >> (N * 50) % 64) % (1ULL << N); if constexpr (((N * 51) % 64) < ((N * 50) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 51) % 64)) << (N - ((N * 51) % 64)); }
  out++; *out = ((*in) >> (N * 51) % 64) % (1ULL << N); if constexpr (((N * 52) % 64) < ((N * 51) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 52) % 64)) << (N - ((N * 52) % 64)); }
  out++; *out = ((*in) >> (N * 52) % 64) % (1ULL << N); if constexpr (((N * 53) % 64) < ((N * 52) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 53) % 64)) << (N - ((N * 53) % 64)); }
  out++; *out = ((*in) >> (N * 53) % 64) % (1ULL << N); if constexpr (((N * 54) % 64) < ((N * 53) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 54) % 64)) << (N - ((N * 54) % 64)); }
  out++; *out = ((*in) >> (N * 54) % 64) % (1ULL << N); if constexpr (((N * 55) % 64) < ((N * 54) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 55) % 64)) << (N - ((N * 55) % 64)); }
  out++; *out = ((*in) >> (N * 55) % 64) % (1ULL << N); if constexpr (((N * 56) % 64) < ((N * 55) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 56) % 64)) << (N - ((N * 56) % 64)); }
  out++; *out = ((*in) >> (N * 56) % 64) % (1ULL << N); if constexpr (((N * 57) % 64) < ((N * 56) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 57) % 64)) << (N - ((N * 57) % 64)); }
  out++; *out = ((*in) >> (N * 57) % 64) % (1ULL << N); if constexpr (((N * 58) % 64) < ((N * 57) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 58) % 64)) << (N - ((N * 58) % 64)); }
  out++; *out = ((*in) >> (N * 58) % 64) % (1ULL << N); if constexpr (((N * 59) % 64) < ((N * 58) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 59) % 64)) << (N - ((N * 59) % 64)); }
  out++; *out = ((*in) >> (N * 59) % 64) % (1ULL << N); if constexpr (((N * 60) % 64) < ((N * 59) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 60) % 64)) << (N - ((N * 60) % 64)); }
  out++; *out = ((*in) >> (N * 60) % 64) % (1ULL << N); if constexpr (((N * 61) % 64) < ((N * 60) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 61) % 64)) << (N - ((N * 61) % 64)); }
  out++; *out = ((*in) >> (N * 61) % 64) % (1ULL << N); if constexpr (((N * 62) % 64) < ((N * 61) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 62) % 64)) << (N - ((N * 62) % 64)); }
  out++; *out = ((*in) >> (N * 62) % 64) % (1ULL << N); if constexpr (((N * 63) % 64) < ((N * 62) % 64)) { ++in; *out |= ((*in) % (1ULL << (N * 63) % 64)) << (N - ((N * 63) % 64)); }
  out++; *out = ((*in) >> (N * 63) % 64) % (1ULL << N);
}

template<>
FORCE_INLINE void fastunpack<64>(const uint64_t* RESTRICT in, uint64_t* RESTRICT out) noexcept {
  std::memcpy(out, in, sizeof(uint64_t)*irs::packed::BLOCK_SIZE_64);
}

MSVC_ONLY(__pragma(warning(disable:4702))) // unreachable code

template<int N, int I>
FORCE_INLINE uint32_t fastpack_at(const uint32_t* in) noexcept {
  // 32 == sizeof(uint32_t) * 8
  static_assert(N > 0 && N < 32, "N <= 0 || N > 32");
  static_assert(I >= 0 && I < 32, "I < 0 || I >= 32");

  // ensure all computations are constexr, i.e. no conditional jumps, no loops, no variable increment/decrement

  if constexpr ((N*(I + 1) % 32) < ((N*I) % 32)) {
    return ((in[N*I / 32] >> (N*I % 32)) % (1U << N)) | ((in[1 + N*I / 32] % (1U << (N * (I + 1)) % 32)) << (N - ((N * (I + 1)) % 32)));
  }

  return ((in[N*I / 32] >> (N*I % 32)) % (1U << N));
}

template<int N, int I>
FORCE_INLINE uint64_t fastpack_at(const uint64_t* in) noexcept {
  // 64 == sizeof(uint64_t) * 8
  static_assert(N > 0 && N < 64, "N <= 0 || N > 64");
  static_assert(I >= 0 && I < 64, "I < 0 || I >= 64");

  // ensure all computations are constexr, i.e. no conditional jumps, no loops, no variable increment/decrement

  if constexpr ((N*(I + 1) % 64) < ((N*I) % 64)) {
    return ((in[N*I / 64] >> (N*I % 64)) % (1ULL << N)) | ((in[1 + N*I / 64] % (1ULL << (N * (I + 1)) % 64)) << (N - ((N * (I + 1)) % 64)));
  }

  return ((in[N*I / 64] >> (N*I % 64)) % (1ULL << N));
}

MSVC_ONLY(__pragma(warning(disable:4715))) // not all control paths return a value

template<int N>
uint32_t fastpack_at(const uint32_t* in, const size_t i) noexcept {
  assert(i < irs::packed::BLOCK_SIZE_32);

  switch (i) {
    case 0:  return fastpack_at<N, 0>(in);
    case 1:  return fastpack_at<N, 1>(in);
    case 2:  return fastpack_at<N, 2>(in);
    case 3:  return fastpack_at<N, 3>(in);
    case 4:  return fastpack_at<N, 4>(in);
    case 5:  return fastpack_at<N, 5>(in);
    case 6:  return fastpack_at<N, 6>(in);
    case 7:  return fastpack_at<N, 7>(in);
    case 8:  return fastpack_at<N, 8>(in);
    case 9:  return fastpack_at<N, 9>(in);
    case 10: return fastpack_at<N, 10>(in);
    case 11: return fastpack_at<N, 11>(in);
    case 12: return fastpack_at<N, 12>(in);
    case 13: return fastpack_at<N, 13>(in);
    case 14: return fastpack_at<N, 14>(in);
    case 15: return fastpack_at<N, 15>(in);
    case 16: return fastpack_at<N, 16>(in);
    case 17: return fastpack_at<N, 17>(in);
    case 18: return fastpack_at<N, 18>(in);
    case 19: return fastpack_at<N, 19>(in);
    case 20: return fastpack_at<N, 20>(in);
    case 21: return fastpack_at<N, 21>(in);
    case 22: return fastpack_at<N, 22>(in);
    case 23: return fastpack_at<N, 23>(in);
    case 24: return fastpack_at<N, 24>(in);
    case 25: return fastpack_at<N, 25>(in);
    case 26: return fastpack_at<N, 26>(in);
    case 27: return fastpack_at<N, 27>(in);
    case 28: return fastpack_at<N, 28>(in);
    case 29: return fastpack_at<N, 29>(in);
    case 30: return fastpack_at<N, 30>(in);
    case 31: return fastpack_at<N, 31>(in);
    default: assert(false); return 0; // this should never be hit, or algorithm error
  }
}

template<>
uint32_t fastpack_at<32>(const uint32_t* in, const size_t i) noexcept {
  // 32 == sizeof(uint32_t) * 8
  assert(i < 32);
  return in[i];
}

template<int N>
uint64_t fastpack_at(const uint64_t* in, const size_t i) noexcept {
  assert(i < irs::packed::BLOCK_SIZE_64);

  switch (i) {
    case 0:  return fastpack_at<N, 0>(in);
    case 1:  return fastpack_at<N, 1>(in);
    case 2:  return fastpack_at<N, 2>(in);
    case 3:  return fastpack_at<N, 3>(in);
    case 4:  return fastpack_at<N, 4>(in);
    case 5:  return fastpack_at<N, 5>(in);
    case 6:  return fastpack_at<N, 6>(in);
    case 7:  return fastpack_at<N, 7>(in);
    case 8:  return fastpack_at<N, 8>(in);
    case 9:  return fastpack_at<N, 9>(in);
    case 10: return fastpack_at<N, 10>(in);
    case 11: return fastpack_at<N, 11>(in);
    case 12: return fastpack_at<N, 12>(in);
    case 13: return fastpack_at<N, 13>(in);
    case 14: return fastpack_at<N, 14>(in);
    case 15: return fastpack_at<N, 15>(in);
    case 16: return fastpack_at<N, 16>(in);
    case 17: return fastpack_at<N, 17>(in);
    case 18: return fastpack_at<N, 18>(in);
    case 19: return fastpack_at<N, 19>(in);
    case 20: return fastpack_at<N, 20>(in);
    case 21: return fastpack_at<N, 21>(in);
    case 22: return fastpack_at<N, 22>(in);
    case 23: return fastpack_at<N, 23>(in);
    case 24: return fastpack_at<N, 24>(in);
    case 25: return fastpack_at<N, 25>(in);
    case 26: return fastpack_at<N, 26>(in);
    case 27: return fastpack_at<N, 27>(in);
    case 28: return fastpack_at<N, 28>(in);
    case 29: return fastpack_at<N, 29>(in);
    case 30: return fastpack_at<N, 30>(in);
    case 31: return fastpack_at<N, 31>(in);
    case 32: return fastpack_at<N, 32>(in);
    case 33: return fastpack_at<N, 33>(in);
    case 34: return fastpack_at<N, 34>(in);
    case 35: return fastpack_at<N, 35>(in);
    case 36: return fastpack_at<N, 36>(in);
    case 37: return fastpack_at<N, 37>(in);
    case 38: return fastpack_at<N, 38>(in);
    case 39: return fastpack_at<N, 39>(in);
    case 40: return fastpack_at<N, 40>(in);
    case 41: return fastpack_at<N, 41>(in);
    case 42: return fastpack_at<N, 42>(in);
    case 43: return fastpack_at<N, 43>(in);
    case 44: return fastpack_at<N, 44>(in);
    case 45: return fastpack_at<N, 45>(in);
    case 46: return fastpack_at<N, 46>(in);
    case 47: return fastpack_at<N, 47>(in);
    case 48: return fastpack_at<N, 48>(in);
    case 49: return fastpack_at<N, 49>(in);
    case 50: return fastpack_at<N, 50>(in);
    case 51: return fastpack_at<N, 51>(in);
    case 52: return fastpack_at<N, 52>(in);
    case 53: return fastpack_at<N, 53>(in);
    case 54: return fastpack_at<N, 54>(in);
    case 55: return fastpack_at<N, 55>(in);
    case 56: return fastpack_at<N, 56>(in);
    case 57: return fastpack_at<N, 57>(in);
    case 58: return fastpack_at<N, 58>(in);
    case 59: return fastpack_at<N, 59>(in);
    case 60: return fastpack_at<N, 60>(in);
    case 61: return fastpack_at<N, 61>(in);
    case 62: return fastpack_at<N, 62>(in);
    case 63: return fastpack_at<N, 63>(in);
    default: assert(false); return 0; // this should never be hit, or algorithm error
  }
}

template<>
uint64_t fastpack_at<64>(const uint64_t* in, const size_t i) noexcept {
  // 64 == sizeof(uint64_t) * 8
  assert(i < 64);
  return in[i];
}

#if defined(_MSC_VER)
  __pragma(warning(pop))
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

} // namespace {

namespace iresearch {
namespace packed {

void pack_block(const uint32_t* RESTRICT in,
                uint32_t* RESTRICT out,
                const uint32_t bit) noexcept {
  switch (bit) {
    case 1:  fastpack<1>(in, out); break;
    case 2:  fastpack<2>(in, out); break;
    case 3:  fastpack<3>(in, out); break;
    case 4:  fastpack<4>(in, out); break;
    case 5:  fastpack<5>(in, out); break;
    case 6:  fastpack<6>(in, out); break;
    case 7:  fastpack<7>(in, out); break;
    case 8:  fastpack<8>(in, out); break;
    case 9:  fastpack<9>(in, out); break;
    case 10: fastpack<10>(in, out); break;
    case 11: fastpack<11>(in, out); break;
    case 12: fastpack<12>(in, out); break;
    case 13: fastpack<13>(in, out); break;
    case 14: fastpack<14>(in, out); break;
    case 15: fastpack<15>(in, out); break;
    case 16: fastpack<16>(in, out); break;
    case 17: fastpack<17>(in, out); break;
    case 18: fastpack<18>(in, out); break;
    case 19: fastpack<19>(in, out); break;
    case 20: fastpack<20>(in, out); break;
    case 21: fastpack<21>(in, out); break;
    case 22: fastpack<22>(in, out); break;
    case 23: fastpack<23>(in, out); break;
    case 24: fastpack<24>(in, out); break;
    case 25: fastpack<25>(in, out); break;
    case 26: fastpack<26>(in, out); break;
    case 27: fastpack<27>(in, out); break;
    case 28: fastpack<28>(in, out); break;
    case 29: fastpack<29>(in, out); break;
    case 30: fastpack<30>(in, out); break;
    case 31: fastpack<31>(in, out); break;
    case 32: fastpack<32>(in, out); break;
    default: assert(false); break;
  }
}

void pack_block(const uint64_t* RESTRICT in,
                uint64_t* RESTRICT out,
                const uint32_t bit) noexcept {
  switch (bit) {
    case 1:  fastpack<1>(in, out); break;
    case 2:  fastpack<2>(in, out); break;
    case 3:  fastpack<3>(in, out); break;
    case 4:  fastpack<4>(in, out); break;
    case 5:  fastpack<5>(in, out); break;
    case 6:  fastpack<6>(in, out); break;
    case 7:  fastpack<7>(in, out); break;
    case 8:  fastpack<8>(in, out); break;
    case 9:  fastpack<9>(in, out); break;
    case 10: fastpack<10>(in, out); break;
    case 11: fastpack<11>(in, out); break;
    case 12: fastpack<12>(in, out); break;
    case 13: fastpack<13>(in, out); break;
    case 14: fastpack<14>(in, out); break;
    case 15: fastpack<15>(in, out); break;
    case 16: fastpack<16>(in, out); break;
    case 17: fastpack<17>(in, out); break;
    case 18: fastpack<18>(in, out); break;
    case 19: fastpack<19>(in, out); break;
    case 20: fastpack<20>(in, out); break;
    case 21: fastpack<21>(in, out); break;
    case 22: fastpack<22>(in, out); break;
    case 23: fastpack<23>(in, out); break;
    case 24: fastpack<24>(in, out); break;
    case 25: fastpack<25>(in, out); break;
    case 26: fastpack<26>(in, out); break;
    case 27: fastpack<27>(in, out); break;
    case 28: fastpack<28>(in, out); break;
    case 29: fastpack<29>(in, out); break;
    case 30: fastpack<30>(in, out); break;
    case 31: fastpack<31>(in, out); break;
    case 32: fastpack<32>(in, out); break;
    case 33: fastpack<33>(in, out); break;
    case 34: fastpack<34>(in, out); break;
    case 35: fastpack<35>(in, out); break;
    case 36: fastpack<36>(in, out); break;
    case 37: fastpack<37>(in, out); break;
    case 38: fastpack<38>(in, out); break;
    case 39: fastpack<39>(in, out); break;
    case 40: fastpack<40>(in, out); break;
    case 41: fastpack<41>(in, out); break;
    case 42: fastpack<42>(in, out); break;
    case 43: fastpack<43>(in, out); break;
    case 44: fastpack<44>(in, out); break;
    case 45: fastpack<45>(in, out); break;
    case 46: fastpack<46>(in, out); break;
    case 47: fastpack<47>(in, out); break;
    case 48: fastpack<48>(in, out); break;
    case 49: fastpack<49>(in, out); break;
    case 50: fastpack<50>(in, out); break;
    case 51: fastpack<51>(in, out); break;
    case 52: fastpack<52>(in, out); break;
    case 53: fastpack<53>(in, out); break;
    case 54: fastpack<54>(in, out); break;
    case 55: fastpack<55>(in, out); break;
    case 56: fastpack<56>(in, out); break;
    case 57: fastpack<57>(in, out); break;
    case 58: fastpack<58>(in, out); break;
    case 59: fastpack<59>(in, out); break;
    case 60: fastpack<60>(in, out); break;
    case 61: fastpack<61>(in, out); break;
    case 62: fastpack<62>(in, out); break;
    case 63: fastpack<63>(in, out); break;
    case 64: fastpack<64>(in, out); break;
    default: assert(false); break;
  }
}

void unpack_block(const uint32_t* RESTRICT in,
                  uint32_t* RESTRICT out,
                  const uint32_t bit) noexcept {
  switch (bit) {
    case 1:  fastunpack<1>(in, out); break;
    case 2:  fastunpack<2>(in, out); break;
    case 3:  fastunpack<3>(in, out); break;
    case 4:  fastunpack<4>(in, out); break;
    case 5:  fastunpack<5>(in, out); break;
    case 6:  fastunpack<6>(in, out); break;
    case 7:  fastunpack<7>(in, out); break;
    case 8:  fastunpack<8>(in, out); break;
    case 9:  fastunpack<9>(in, out); break;
    case 10: fastunpack<10>(in, out); break;
    case 11: fastunpack<11>(in, out); break;
    case 12: fastunpack<12>(in, out); break;
    case 13: fastunpack<13>(in, out); break;
    case 14: fastunpack<14>(in, out); break;
    case 15: fastunpack<15>(in, out); break;
    case 16: fastunpack<16>(in, out); break;
    case 17: fastunpack<17>(in, out); break;
    case 18: fastunpack<18>(in, out); break;
    case 19: fastunpack<19>(in, out); break;
    case 20: fastunpack<20>(in, out); break;
    case 21: fastunpack<21>(in, out); break;
    case 22: fastunpack<22>(in, out); break;
    case 23: fastunpack<23>(in, out); break;
    case 24: fastunpack<24>(in, out); break;
    case 25: fastunpack<25>(in, out); break;
    case 26: fastunpack<26>(in, out); break;
    case 27: fastunpack<27>(in, out); break;
    case 28: fastunpack<28>(in, out); break;
    case 29: fastunpack<29>(in, out); break;
    case 30: fastunpack<30>(in, out); break;
    case 31: fastunpack<31>(in, out); break;
    case 32: fastunpack<32>(in, out); break;
    default: assert(false); break;
  }
}

void unpack_block(const uint64_t* RESTRICT in,
                  uint64_t* RESTRICT out,
                  const uint32_t bit) noexcept {
  switch (bit) {
    case 1:   fastunpack<1>(in, out); break;
    case 2:   fastunpack<2>(in, out); break;
    case 3:   fastunpack<3>(in, out); break;
    case 4:   fastunpack<4>(in, out); break;
    case 5:   fastunpack<5>(in, out); break;
    case 6:   fastunpack<6>(in, out); break;
    case 7:   fastunpack<7>(in, out); break;
    case 8:   fastunpack<8>(in, out); break;
    case 9:   fastunpack<9>(in, out); break;
    case 10: fastunpack<10>(in, out); break;
    case 11: fastunpack<11>(in, out); break;
    case 12: fastunpack<12>(in, out); break;
    case 13: fastunpack<13>(in, out); break;
    case 14: fastunpack<14>(in, out); break;
    case 15: fastunpack<15>(in, out); break;
    case 16: fastunpack<16>(in, out); break;
    case 17: fastunpack<17>(in, out); break;
    case 18: fastunpack<18>(in, out); break;
    case 19: fastunpack<19>(in, out); break;
    case 20: fastunpack<20>(in, out); break;
    case 21: fastunpack<21>(in, out); break;
    case 22: fastunpack<22>(in, out); break;
    case 23: fastunpack<23>(in, out); break;
    case 24: fastunpack<24>(in, out); break;
    case 25: fastunpack<25>(in, out); break;
    case 26: fastunpack<26>(in, out); break;
    case 27: fastunpack<27>(in, out); break;
    case 28: fastunpack<28>(in, out); break;
    case 29: fastunpack<29>(in, out); break;
    case 30: fastunpack<30>(in, out); break;
    case 31: fastunpack<31>(in, out); break;
    case 32: fastunpack<32>(in, out); break;
    case 33: fastunpack<33>(in, out); break;
    case 34: fastunpack<34>(in, out); break;
    case 35: fastunpack<35>(in, out); break;
    case 36: fastunpack<36>(in, out); break;
    case 37: fastunpack<37>(in, out); break;
    case 38: fastunpack<38>(in, out); break;
    case 39: fastunpack<39>(in, out); break;
    case 40: fastunpack<40>(in, out); break;
    case 41: fastunpack<41>(in, out); break;
    case 42: fastunpack<42>(in, out); break;
    case 43: fastunpack<43>(in, out); break;
    case 44: fastunpack<44>(in, out); break;
    case 45: fastunpack<45>(in, out); break;
    case 46: fastunpack<46>(in, out); break;
    case 47: fastunpack<47>(in, out); break;
    case 48: fastunpack<48>(in, out); break;
    case 49: fastunpack<49>(in, out); break;
    case 50: fastunpack<50>(in, out); break;
    case 51: fastunpack<51>(in, out); break;
    case 52: fastunpack<52>(in, out); break;
    case 53: fastunpack<53>(in, out); break;
    case 54: fastunpack<54>(in, out); break;
    case 55: fastunpack<55>(in, out); break;
    case 56: fastunpack<56>(in, out); break;
    case 57: fastunpack<57>(in, out); break;
    case 58: fastunpack<58>(in, out); break;
    case 59: fastunpack<59>(in, out); break;
    case 60: fastunpack<60>(in, out); break;
    case 61: fastunpack<61>(in, out); break;
    case 62: fastunpack<62>(in, out); break;
    case 63: fastunpack<63>(in, out); break;
    case 64: fastunpack<64>(in, out); break;
    default: assert(false); break;
  }
}

uint32_t fastpack_at(
    const uint32_t* in,
    const size_t i,
    const uint32_t bits) noexcept {
  assert(i < BLOCK_SIZE_32);

  switch (bits) {
    case 1:  return ::fastpack_at<1>(in, i);
    case 2:  return ::fastpack_at<2>(in, i);
    case 3:  return ::fastpack_at<3>(in, i);
    case 4:  return ::fastpack_at<4>(in, i);
    case 5:  return ::fastpack_at<5>(in, i);
    case 6:  return ::fastpack_at<6>(in, i);
    case 7:  return ::fastpack_at<7>(in, i);
    case 8:  return ::fastpack_at<8>(in, i);
    case 9:  return ::fastpack_at<9>(in, i);
    case 10: return ::fastpack_at<10>(in, i);
    case 11: return ::fastpack_at<11>(in, i);
    case 12: return ::fastpack_at<12>(in, i);
    case 13: return ::fastpack_at<13>(in, i);
    case 14: return ::fastpack_at<14>(in, i);
    case 15: return ::fastpack_at<15>(in, i);
    case 16: return ::fastpack_at<16>(in, i);
    case 17: return ::fastpack_at<17>(in, i);
    case 18: return ::fastpack_at<18>(in, i);
    case 19: return ::fastpack_at<19>(in, i);
    case 20: return ::fastpack_at<20>(in, i);
    case 21: return ::fastpack_at<21>(in, i);
    case 22: return ::fastpack_at<22>(in, i);
    case 23: return ::fastpack_at<23>(in, i);
    case 24: return ::fastpack_at<24>(in, i);
    case 25: return ::fastpack_at<25>(in, i);
    case 26: return ::fastpack_at<26>(in, i);
    case 27: return ::fastpack_at<27>(in, i);
    case 28: return ::fastpack_at<28>(in, i);
    case 29: return ::fastpack_at<29>(in, i);
    case 30: return ::fastpack_at<30>(in, i);
    case 31: return ::fastpack_at<31>(in, i);
    case 32: return ::fastpack_at<32>(in, i);
    default: assert(false); return 0; // this should never be hit, or algorithm error
  }
}

uint64_t fastpack_at(
    const uint64_t* in,
    const size_t i,
    const uint32_t bits) noexcept {
  assert(i < BLOCK_SIZE_64);

  switch (bits) {
    case 1:  return ::fastpack_at<1>(in, i);
    case 2:  return ::fastpack_at<2>(in, i);
    case 3:  return ::fastpack_at<3>(in, i);
    case 4:  return ::fastpack_at<4>(in, i);
    case 5:  return ::fastpack_at<5>(in, i);
    case 6:  return ::fastpack_at<6>(in, i);
    case 7:  return ::fastpack_at<7>(in, i);
    case 8:  return ::fastpack_at<8>(in, i);
    case 9:  return ::fastpack_at<9>(in, i);
    case 10: return ::fastpack_at<10>(in, i);
    case 11: return ::fastpack_at<11>(in, i);
    case 12: return ::fastpack_at<12>(in, i);
    case 13: return ::fastpack_at<13>(in, i);
    case 14: return ::fastpack_at<14>(in, i);
    case 15: return ::fastpack_at<15>(in, i);
    case 16: return ::fastpack_at<16>(in, i);
    case 17: return ::fastpack_at<17>(in, i);
    case 18: return ::fastpack_at<18>(in, i);
    case 19: return ::fastpack_at<19>(in, i);
    case 20: return ::fastpack_at<20>(in, i);
    case 21: return ::fastpack_at<21>(in, i);
    case 22: return ::fastpack_at<22>(in, i);
    case 23: return ::fastpack_at<23>(in, i);
    case 24: return ::fastpack_at<24>(in, i);
    case 25: return ::fastpack_at<25>(in, i);
    case 26: return ::fastpack_at<26>(in, i);
    case 27: return ::fastpack_at<27>(in, i);
    case 28: return ::fastpack_at<28>(in, i);
    case 29: return ::fastpack_at<29>(in, i);
    case 30: return ::fastpack_at<30>(in, i);
    case 31: return ::fastpack_at<31>(in, i);
    case 32: return ::fastpack_at<32>(in, i);
    case 33: return ::fastpack_at<33>(in, i);
    case 34: return ::fastpack_at<34>(in, i);
    case 35: return ::fastpack_at<35>(in, i);
    case 36: return ::fastpack_at<36>(in, i);
    case 37: return ::fastpack_at<37>(in, i);
    case 38: return ::fastpack_at<38>(in, i);
    case 39: return ::fastpack_at<39>(in, i);
    case 40: return ::fastpack_at<40>(in, i);
    case 41: return ::fastpack_at<41>(in, i);
    case 42: return ::fastpack_at<42>(in, i);
    case 43: return ::fastpack_at<43>(in, i);
    case 44: return ::fastpack_at<44>(in, i);
    case 45: return ::fastpack_at<45>(in, i);
    case 46: return ::fastpack_at<46>(in, i);
    case 47: return ::fastpack_at<47>(in, i);
    case 48: return ::fastpack_at<48>(in, i);
    case 49: return ::fastpack_at<49>(in, i);
    case 50: return ::fastpack_at<50>(in, i);
    case 51: return ::fastpack_at<51>(in, i);
    case 52: return ::fastpack_at<52>(in, i);
    case 53: return ::fastpack_at<53>(in, i);
    case 54: return ::fastpack_at<54>(in, i);
    case 55: return ::fastpack_at<55>(in, i);
    case 56: return ::fastpack_at<56>(in, i);
    case 57: return ::fastpack_at<57>(in, i);
    case 58: return ::fastpack_at<58>(in, i);
    case 59: return ::fastpack_at<59>(in, i);
    case 60: return ::fastpack_at<60>(in, i);
    case 61: return ::fastpack_at<61>(in, i);
    case 62: return ::fastpack_at<62>(in, i);
    case 63: return ::fastpack_at<63>(in, i);
    case 64: return ::fastpack_at<64>(in, i);
    default: assert(false); return 0; // this should never be hit, or algorithm error
  }
}

} // packed
}
