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

#include "hwy/examples/skeleton.h"

#include <assert.h>
#include <stdio.h>

// First undef to prevent error when re-included.
#undef HWY_TARGET_INCLUDE
// For runtime dispatch, specify the name of the current file (unfortunately
// __FILE__ is not reliable) so that foreach_target.h can re-include it.
#define HWY_TARGET_INCLUDE "hwy/examples/skeleton.cc"
// Re-include this file once per enabled target to generate code for it.
#include "hwy/foreach_target.h"

#include "hwy/examples/skeleton_shared.h"
#include "hwy/highway.h"

// Optional: factor out parts of the implementation into *-inl.h
#include "hwy/examples/skeleton-inl.h"

// Optional, can instead add HWY_ATTR to all functions.
HWY_BEFORE_NAMESPACE();
namespace skeleton {
namespace HWY_NAMESPACE {

// Compiled once per target via multiple inclusion.
void Skeleton(const float* HWY_RESTRICT in1, const float* HWY_RESTRICT in2,
              float* HWY_RESTRICT out) {
  printf("Target %s: %s\n", hwy::TargetName(HWY_TARGET),
         ExampleGatherStrategy());

  ExampleMulAdd(in1, in2, out);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace skeleton
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace skeleton {

// This macro declares a static array SkeletonHighwayDispatchTable used for
// dynamic dispatch. This macro should be placed in the same namespace that
// defines the Skeleton function above.
HWY_EXPORT(Skeleton);

// This function is optional and only needed in the case of exposing it in the
// header file. Otherwise using HWY_DYNAMIC_DISPATCH(Skeleton) multiple times in
// this module is equivalent to inlining this optional function..
void Skeleton(const float* HWY_RESTRICT in1, const float* HWY_RESTRICT in2,
              float* HWY_RESTRICT out) {
  return HWY_DYNAMIC_DISPATCH(Skeleton)(in1, in2, out);
}

// Optional: anything to compile only once, e.g. non-SIMD implementations of
// public functions provided by this module, can go inside #if HWY_ONCE
// (after end_target-inl.h).

}  // namespace skeleton
#endif  // HWY_ONCE
