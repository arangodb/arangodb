////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "infra/ResizeAccounting.h"

using namespace arangodb::infra;

// --- across monitors: full new reserved, full old released -----------------

TEST(ResizeAccountingTest, cross_monitor_reserves_full_new_releases_full_old) {
  auto d = computeResizeDelta(/*sameMonitor*/ false, /*oldReq*/ 100,
                              /*oldAct*/ 128, /*newReq*/ 200, /*newAct*/ 256);
  EXPECT_EQ(200u, d.reserveRequested);
  EXPECT_EQ(256u, d.reserveActual);
  EXPECT_EQ(100u, d.releaseRequested);
  EXPECT_EQ(128u, d.releaseActual);
}

TEST(ResizeAccountingTest, cross_monitor_shrink_still_reserves_full_new) {
  // A shrink across monitors still reserves the full new size on the new
  // monitor -- it did not previously account for this allocation.
  auto d = computeResizeDelta(false, 200, 256, 100, 128);
  EXPECT_EQ(100u, d.reserveRequested);
  EXPECT_EQ(128u, d.reserveActual);
  EXPECT_EQ(200u, d.releaseRequested);
  EXPECT_EQ(256u, d.releaseActual);
}

// --- same monitor: netted per axis ------------------------------------------

TEST(ResizeAccountingTest, same_monitor_grow_reserves_growth_only) {
  auto d = computeResizeDelta(true, 100, 128, 200, 256);
  EXPECT_EQ(100u, d.reserveRequested);
  EXPECT_EQ(128u, d.reserveActual);
  EXPECT_EQ(0u, d.releaseRequested);
  EXPECT_EQ(0u, d.releaseActual);
}

TEST(ResizeAccountingTest, same_monitor_shrink_releases_shrink_only) {
  auto d = computeResizeDelta(true, 200, 256, 100, 128);
  EXPECT_EQ(0u, d.reserveRequested);
  EXPECT_EQ(0u, d.reserveActual);
  EXPECT_EQ(100u, d.releaseRequested);
  EXPECT_EQ(128u, d.releaseActual);
}

TEST(ResizeAccountingTest, same_monitor_same_size_is_noop) {
  auto d = computeResizeDelta(true, 100, 128, 100, 128);
  EXPECT_EQ(0u, d.reserveRequested);
  EXPECT_EQ(0u, d.reserveActual);
  EXPECT_EQ(0u, d.releaseRequested);
  EXPECT_EQ(0u, d.releaseActual);
}

TEST(ResizeAccountingTest,
     same_monitor_grow_within_size_class_reserves_only_requested) {
  // Growing but staying in the same jemalloc size class: requested grows,
  // actual is unchanged. Only the requested axis is reserved.
  auto d = computeResizeDelta(true, 40, 48, 44, 48);
  EXPECT_EQ(4u, d.reserveRequested);
  EXPECT_EQ(0u, d.reserveActual);
  EXPECT_EQ(0u, d.releaseRequested);
  EXPECT_EQ(0u, d.releaseActual);
}

TEST(ResizeAccountingTest, same_monitor_axes_move_in_opposite_directions) {
  // The aligned-block case: an over-allocated aligned block (small requested,
  // large actual) replaced by a tight normal block grows `requested` while
  // shrinking `actual`. Each axis nets independently.
  auto d = computeResizeDelta(true, /*oldReq*/ 100, /*oldAct*/ 4224,
                              /*newReq*/ 200, /*newAct*/ 224);
  EXPECT_EQ(100u, d.reserveRequested);  // requested grew
  EXPECT_EQ(0u, d.reserveActual);
  EXPECT_EQ(0u, d.releaseRequested);
  EXPECT_EQ(4000u, d.releaseActual);  // actual shrank
}

// The policy is a pure compile-time function; lock a representative case at
// compile time so a regression cannot even build.
static_assert(computeResizeDelta(true, 100, 128, 200, 256).reserveActual ==
              128u);
static_assert(computeResizeDelta(true, 100, 128, 200, 256).releaseActual == 0u);
static_assert(computeResizeDelta(false, 100, 128, 200, 256).reserveActual ==
              256u);
