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

#include "hwy/targets.h"

#include "hwy/tests/test_util-inl.h"

namespace fake {

#define DECLARE_FUNCTION(TGT)                        \
  namespace N_##TGT {                                \
    uint32_t FakeFunction(int) { return HWY_##TGT; } \
  }

DECLARE_FUNCTION(AVX3_DL)
DECLARE_FUNCTION(AVX3)
DECLARE_FUNCTION(AVX2)
DECLARE_FUNCTION(SSE4)
DECLARE_FUNCTION(SSSE3)
DECLARE_FUNCTION(NEON)
DECLARE_FUNCTION(SVE)
DECLARE_FUNCTION(SVE2)
DECLARE_FUNCTION(PPC8)
DECLARE_FUNCTION(WASM)
DECLARE_FUNCTION(RVV)
DECLARE_FUNCTION(SCALAR)

HWY_EXPORT(FakeFunction);

void CheckFakeFunction() {
#define CHECK_ARRAY_ENTRY(TGT)                                              \
  if ((HWY_TARGETS & HWY_##TGT) != 0) {                                     \
    hwy::SetSupportedTargetsForTest(HWY_##TGT);                             \
    /* Calling Update() first to make &HWY_DYNAMIC_DISPATCH() return */     \
    /* the pointer to the already cached function. */                       \
    hwy::chosen_target.Update();                                            \
    EXPECT_EQ(uint32_t(HWY_##TGT), HWY_DYNAMIC_DISPATCH(FakeFunction)(42)); \
    /* Calling DeInit() will test that the initializer function */          \
    /* also calls the right function. */                                    \
    hwy::chosen_target.DeInit();                                            \
    EXPECT_EQ(uint32_t(HWY_##TGT), HWY_DYNAMIC_DISPATCH(FakeFunction)(42)); \
    /* Second call uses the cached value from the previous call. */         \
    EXPECT_EQ(uint32_t(HWY_##TGT), HWY_DYNAMIC_DISPATCH(FakeFunction)(42)); \
  }
  CHECK_ARRAY_ENTRY(AVX3_DL)
  CHECK_ARRAY_ENTRY(AVX3)
  CHECK_ARRAY_ENTRY(AVX2)
  CHECK_ARRAY_ENTRY(SSE4)
  CHECK_ARRAY_ENTRY(SSSE3)
  CHECK_ARRAY_ENTRY(NEON)
  CHECK_ARRAY_ENTRY(SVE)
  CHECK_ARRAY_ENTRY(SVE2)
  CHECK_ARRAY_ENTRY(PPC8)
  CHECK_ARRAY_ENTRY(WASM)
  CHECK_ARRAY_ENTRY(RVV)
  CHECK_ARRAY_ENTRY(SCALAR)
#undef CHECK_ARRAY_ENTRY
}

}  // namespace fake

namespace hwy {

class HwyTargetsTest : public testing::Test {
 protected:
  void TearDown() override {
    SetSupportedTargetsForTest(0);
    DisableTargets(0);  // Reset the mask.
  }
};

// Test that the order in the HWY_EXPORT static array matches the expected
// value of the target bits. This is only checked for the targets that are
// enabled in the current compilation.
TEST_F(HwyTargetsTest, ChosenTargetOrderTest) { fake::CheckFakeFunction(); }

TEST_F(HwyTargetsTest, DisabledTargetsTest) {
  DisableTargets(~0u);
  // Check that the baseline can't be disabled.
  HWY_ASSERT(HWY_ENABLED_BASELINE == SupportedTargets());

  DisableTargets(0);  // Reset the mask.
  uint32_t current_targets = SupportedTargets();
  if ((current_targets & ~uint32_t(HWY_ENABLED_BASELINE)) == 0) {
    // We can't test anything else if the only compiled target is the baseline.
    return;
  }
  // Get the lowest bit in the mask (the best target) and disable that one.
  uint32_t lowest_target = current_targets & (~current_targets + 1);
  // The lowest target shouldn't be one in the baseline.
  HWY_ASSERT((lowest_target & ~uint32_t(HWY_ENABLED_BASELINE)) != 0);
  DisableTargets(lowest_target);

  // Check that the other targets are still enabled.
  HWY_ASSERT((lowest_target ^ current_targets) == SupportedTargets());
  DisableTargets(0);  // Reset the mask.
}

}  // namespace hwy

// Ought not to be necessary, but without this, no tests run on RVV.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
