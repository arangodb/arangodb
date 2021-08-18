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

// Demo of functions that might be called from multiple SIMD modules (either
// other -inl.h files, or a .cc file between begin/end_target-inl). This is
// optional - all SIMD code can reside in .cc files. However, this allows
// splitting code into different files while still inlining instead of requiring
// calling through function pointers.

// Include guard (still compiled once per target)
#if defined(HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_
#undef HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_
#else
#define HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_
#endif

// It is fine to #include normal or *-inl headers.
#include <stddef.h>

#include "hwy/examples/skeleton_shared.h"
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace skeleton {
namespace HWY_NAMESPACE {

using hwy::HWY_NAMESPACE::MulAdd;

// Computes out[i] = in1[i] * kMultiplier + in2[i] for i < 256.
HWY_MAYBE_UNUSED void ExampleMulAdd(const float* HWY_RESTRICT in1,
                                    const float* HWY_RESTRICT in2,
                                    float* HWY_RESTRICT out) {
  // Descriptor(s) for all vector types used in this function.
  HWY_FULL(float) df;

  const auto mul = Set(df, kMultiplier);
  for (size_t i = 0; i < 256; i += Lanes(df)) {
    const auto result = MulAdd(mul, Load(df, in1 + i), Load(df, in2 + i));
    Store(result, df, out + i);
  }
}

// (This doesn't generate SIMD instructions, so is not required here)
HWY_MAYBE_UNUSED const char* ExampleGatherStrategy() {
  // Highway functions generate per-target implementations from the same source
  // code via HWY_CAPPED(type, HWY_MIN(any_LANES_constants, ..)). If needed,
  // entirely different codepaths can also be selected like so:
#if HWY_GATHER_LANES > 1
  return "Has gather";
#else
  return "Gather is limited to one lane";
#endif
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace skeleton
HWY_AFTER_NAMESPACE();

#endif  // include guard
