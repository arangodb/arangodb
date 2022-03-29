// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>

#include "gtest/gtest.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/debugging/leak_check.h"

namespace {

TEST(LeakCheckTest, DetectLeakSanitizer) {
#ifdef ABSL_EXPECT_LEAK_SANITIZER
  EXPECT_TRUE(absl::HaveLeakSanitizer());
  EXPECT_TRUE(absl::LeakCheckerIsActive());
#else
  EXPECT_FALSE(absl::HaveLeakSanitizer());
  EXPECT_FALSE(absl::LeakCheckerIsActive());
#endif
}

TEST(LeakCheckTest, IgnoreLeakSuppressesLeakedMemoryErrors) {
  auto foo = absl::IgnoreLeak(new std::string("some ignored leaked string"));
  ABSL_RAW_LOG(INFO, "Ignoring leaked string %s", foo->c_str());
}

TEST(LeakCheckTest, LeakCheckDisablerIgnoresLeak) {
  absl::LeakCheckDisabler disabler;
  auto foo = new std::string("some string leaked while checks are disabled");
  ABSL_RAW_LOG(INFO, "Ignoring leaked string %s", foo->c_str());
}

}  // namespace
