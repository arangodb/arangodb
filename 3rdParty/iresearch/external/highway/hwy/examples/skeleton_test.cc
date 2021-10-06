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

// Example of unit test for the "skeleton" module.

#include "hwy/examples/skeleton.h"  // Skeleton

#include <stdio.h>

#include "hwy/tests/test_util-inl.h"  // RunTest

namespace skeleton {

TEST(SkeletonTest, MainTest) {
  HWY_ALIGN_MAX float in1[256];
  HWY_ALIGN_MAX float in2[256];
  HWY_ALIGN_MAX float out[256];
  for (size_t i = 0; i < 256; ++i) {
    in1[i] = static_cast<float>(i);
    in2[i] = in1[i] + 300;
  }

  // Tests will run for all compiled targets to ensure all are OK.
  hwy::RunTest([&in1, &in2, &out]() {
    Skeleton(in1, in2, out);
    // Add EXPECT_... calls here.
  });
}

}  // namespace skeleton
