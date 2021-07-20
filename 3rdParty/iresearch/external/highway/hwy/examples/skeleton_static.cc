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

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/examples/skeleton.cc"
#include "hwy/foreach_target.h"

#include <assert.h>
#include <stdio.h>

#include "hwy/highway.h"
#include "hwy/examples/skeleton_shared.h"

// Optional: include any shared *-inl.h
#include "hwy/examples/skeleton-inl.h"

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

HWY_EXPORT(Skeleton);

// Optional: anything to compile only once, e.g. non-SIMD implementations of
// public functions provided by this module, can go inside #if HWY_ONCE
// (after end_target-inl.h).

}  // namespace skeleton
#endif  // HWY_ONCE
