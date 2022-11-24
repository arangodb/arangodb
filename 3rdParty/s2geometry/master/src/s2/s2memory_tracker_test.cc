// Copyright 2020 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)

#include "s2/s2memory_tracker.h"

#include <gtest/gtest.h>

TEST(S2MemoryTracker, PeriodicCallback) {
  S2MemoryTracker tracker;
  int callback_count = 0;
  S2MemoryTracker::Client client(&tracker);

  // Test that a callback interval of 0 bytes invokes the callback every time.
  tracker.set_periodic_callback(0, [&]() { ++callback_count; });
  client.Tally(0);
  EXPECT_EQ(callback_count, 1);
  client.Tally(-10);
  EXPECT_EQ(callback_count, 2);

  // Test that the callback interval is based on total allocated bytes rather
  // than current usage.
  tracker.set_periodic_callback(100, [&]() { ++callback_count; });
  client.Tally(99);
  EXPECT_EQ(callback_count, 2);
  client.Tally(1);
  EXPECT_EQ(callback_count, 3);
  client.Tally(-50);
  client.Tally(50);
  client.Tally(-50);
  client.Tally(49);
  EXPECT_EQ(callback_count, 3);
  client.Tally(1);
  EXPECT_EQ(callback_count, 4);
}
