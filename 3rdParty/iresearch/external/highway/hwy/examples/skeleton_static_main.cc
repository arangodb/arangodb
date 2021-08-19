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

// Example of static dispatch.

#include <stdio.h>

#include "hwy/examples/skeleton_shared.h"  // kMultiplier

// SIMD modules can optionally reside in inl headers, or below:
#include "hwy/examples/skeleton-inl.h"  // ExampleMulAdd
#include "hwy/highway.h"
HWY_BEFORE_NAMESPACE();
namespace skeleton {
namespace HWY_NAMESPACE {

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

namespace skeleton {

void StaticMain() {
  HWY_ALIGN_MAX float in1[256];
  HWY_ALIGN_MAX float in2[256];
  HWY_ALIGN_MAX float out[256];
  for (size_t i = 0; i < 256; ++i) {
    in1[i] = static_cast<float>(i);
    in2[i] = in1[i] + 300;
  }

  HWY_STATIC_DISPATCH(Skeleton)(in1, in2, out);
  printf("Should be %.2f: %.2f\n", in1[255] * kMultiplier + in2[255], out[255]);
}

}  // namespace skeleton

int main() {
  skeleton::StaticMain();
  return 0;
}
