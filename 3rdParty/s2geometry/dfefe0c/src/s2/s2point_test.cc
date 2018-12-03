// Copyright 2005 Google Inc. All Rights Reserved.
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

#include "s2/s2point.h"

#include <cstddef>
#include <unordered_set>

#include <gtest/gtest.h>
#include "s2/s2cell.h"
#include "s2/s2testing.h"

using std::unordered_set;

TEST(S2, S2PointHashSpreads) {
  int kTestPoints = 1 << 16;
  unordered_set<size_t> set;
  unordered_set<S2Point, S2PointHash> points;
  S2PointHash hasher;
  S2Point base = S2Point(1, 1, 1);
  for (int i = 0; i < kTestPoints; ++i) {
    // All points in a tiny cap to test avalanche property of hash
    // function (the cap would be of radius 1mm on Earth (4*10^9/2^35).
    S2Point perturbed = base + S2Testing::RandomPoint() / (1ULL << 35);
    perturbed = perturbed.Normalize();
    set.insert(hasher(perturbed));
    points.insert(perturbed);
  }
  // A real collision is extremely unlikely.
  EXPECT_EQ(0, kTestPoints - points.size());
  // Allow a few for the hash.
  EXPECT_GE(10, kTestPoints - set.size());
}
