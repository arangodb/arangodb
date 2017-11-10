// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits>

#include "include/v8config.h"

#include "src/base/bits.h"
#include "src/utils.h"
#include "src/wasm/wasm-external-refs.h"

namespace v8 {
namespace internal {
namespace wasm {

void f32_trunc_wrapper(float* param) { *param = truncf(*param); }

void f32_floor_wrapper(float* param) { *param = floorf(*param); }

void f32_ceil_wrapper(float* param) { *param = ceilf(*param); }

void f32_nearest_int_wrapper(float* param) { *param = nearbyintf(*param); }

void f64_trunc_wrapper(double* param) {
  WriteDoubleValue(param, trunc(ReadDoubleValue(param)));
}

void f64_floor_wrapper(double* param) {
  WriteDoubleValue(param, floor(ReadDoubleValue(param)));
}

void f64_ceil_wrapper(double* param) {
  WriteDoubleValue(param, ceil(ReadDoubleValue(param)));
}

void f64_nearest_int_wrapper(double* param) {
  WriteDoubleValue(param, nearbyint(ReadDoubleValue(param)));
}

void int64_to_float32_wrapper(int64_t* input, float* output) {
  *output = static_cast<float>(*input);
}

void uint64_to_float32_wrapper(uint64_t* input, float* output) {
#if V8_CC_MSVC
  // With MSVC we use static_cast<float>(uint32_t) instead of
  // static_cast<float>(uint64_t) to achieve round-to-nearest-ties-even
  // semantics. The idea is to calculate
  // static_cast<float>(high_word) * 2^32 + static_cast<float>(low_word). To
  // achieve proper rounding in all cases we have to adjust the high_word
  // with a "rounding bit" sometimes. The rounding bit is stored in the LSB of
  // the high_word if the low_word may affect the rounding of the high_word.
  uint32_t low_word = static_cast<uint32_t>(*input & 0xffffffff);
  uint32_t high_word = static_cast<uint32_t>(*input >> 32);

  float shift = static_cast<float>(1ull << 32);
  // If the MSB of the high_word is set, then we make space for a rounding bit.
  if (high_word < 0x80000000) {
    high_word <<= 1;
    shift = static_cast<float>(1ull << 31);
  }

  if ((high_word & 0xfe000000) && low_word) {
    // Set the rounding bit.
    high_word |= 1;
  }

  float result = static_cast<float>(high_word);
  result *= shift;
  result += static_cast<float>(low_word);
  *output = result;

#else
  *output = static_cast<float>(*input);
#endif
}

void int64_to_float64_wrapper(int64_t* input, double* output) {
  *output = static_cast<double>(*input);
}

void uint64_to_float64_wrapper(uint64_t* input, double* output) {
#if V8_CC_MSVC
  // With MSVC we use static_cast<double>(uint32_t) instead of
  // static_cast<double>(uint64_t) to achieve round-to-nearest-ties-even
  // semantics. The idea is to calculate
  // static_cast<double>(high_word) * 2^32 + static_cast<double>(low_word).
  uint32_t low_word = static_cast<uint32_t>(*input & 0xffffffff);
  uint32_t high_word = static_cast<uint32_t>(*input >> 32);

  double shift = static_cast<double>(1ull << 32);

  double result = static_cast<double>(high_word);
  result *= shift;
  result += static_cast<double>(low_word);
  *output = result;

#else
  *output = static_cast<double>(*input);
#endif
}

int32_t float32_to_int64_wrapper(float* input, int64_t* output) {
  // We use "<" here to check the upper bound because of rounding problems: With
  // "<=" some inputs would be considered within int64 range which are actually
  // not within int64 range.
  if (*input >= static_cast<float>(std::numeric_limits<int64_t>::min()) &&
      *input < static_cast<float>(std::numeric_limits<int64_t>::max())) {
    *output = static_cast<int64_t>(*input);
    return 1;
  }
  return 0;
}

int32_t float32_to_uint64_wrapper(float* input, uint64_t* output) {
  // We use "<" here to check the upper bound because of rounding problems: With
  // "<=" some inputs would be considered within uint64 range which are actually
  // not within uint64 range.
  if (*input > -1.0 &&
      *input < static_cast<float>(std::numeric_limits<uint64_t>::max())) {
    *output = static_cast<uint64_t>(*input);
    return 1;
  }
  return 0;
}

int32_t float64_to_int64_wrapper(double* input, int64_t* output) {
  // We use "<" here to check the upper bound because of rounding problems: With
  // "<=" some inputs would be considered within int64 range which are actually
  // not within int64 range.
  if (*input >= static_cast<double>(std::numeric_limits<int64_t>::min()) &&
      *input < static_cast<double>(std::numeric_limits<int64_t>::max())) {
    *output = static_cast<int64_t>(*input);
    return 1;
  }
  return 0;
}

int32_t float64_to_uint64_wrapper(double* input, uint64_t* output) {
  // We use "<" here to check the upper bound because of rounding problems: With
  // "<=" some inputs would be considered within uint64 range which are actually
  // not within uint64 range.
  if (*input > -1.0 &&
      *input < static_cast<double>(std::numeric_limits<uint64_t>::max())) {
    *output = static_cast<uint64_t>(*input);
    return 1;
  }
  return 0;
}

int32_t int64_div_wrapper(int64_t* dst, int64_t* src) {
  if (*src == 0) {
    return 0;
  }
  if (*src == -1 && *dst == std::numeric_limits<int64_t>::min()) {
    return -1;
  }
  *dst /= *src;
  return 1;
}

int32_t int64_mod_wrapper(int64_t* dst, int64_t* src) {
  if (*src == 0) {
    return 0;
  }
  *dst %= *src;
  return 1;
}

int32_t uint64_div_wrapper(uint64_t* dst, uint64_t* src) {
  if (*src == 0) {
    return 0;
  }
  *dst /= *src;
  return 1;
}

int32_t uint64_mod_wrapper(uint64_t* dst, uint64_t* src) {
  if (*src == 0) {
    return 0;
  }
  *dst %= *src;
  return 1;
}

uint32_t word32_ctz_wrapper(uint32_t* input) {
  return static_cast<uint32_t>(base::bits::CountTrailingZeros32(*input));
}

uint32_t word64_ctz_wrapper(uint64_t* input) {
  return static_cast<uint32_t>(base::bits::CountTrailingZeros64(*input));
}

uint32_t word32_popcnt_wrapper(uint32_t* input) {
  return static_cast<uint32_t>(base::bits::CountPopulation(*input));
}

uint32_t word64_popcnt_wrapper(uint64_t* input) {
  return static_cast<uint32_t>(base::bits::CountPopulation(*input));
}

void float64_pow_wrapper(double* param0, double* param1) {
  double x = ReadDoubleValue(param0);
  double y = ReadDoubleValue(param1);
  WriteDoubleValue(param0, Pow(x, y));
}

static WasmTrapCallbackForTesting wasm_trap_callback_for_testing = nullptr;

void set_trap_callback_for_testing(WasmTrapCallbackForTesting callback) {
  wasm_trap_callback_for_testing = callback;
}

void call_trap_callback_for_testing() {
  if (wasm_trap_callback_for_testing) {
    wasm_trap_callback_for_testing();
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
