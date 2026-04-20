////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <velocypack/Builder.h>
#include <velocypack/Buffer.h>

#include "Agency/AgencyCommon.h"
#include "Agency/State.h"
#include "Metrics/MetricsFeature.h"
#include "Mocks/Death_Test.h"
#include "Mocks/Servers.h"

namespace arangodb::consensus {

class StateTest : public ::testing::Test {
 protected:
  arangodb::tests::mocks::MockMetricsServer _server{true};

  State createState() {
    return State(
        _server.server().getFeature<arangodb::metrics::MetricsFeature>());
  }

  static void populateLog(State& state, index_t from, index_t to,
                          term_t term = 1) {
    velocypack::Builder builder;
    builder.openObject();
    builder.close();
    auto buf = builder.steal();
    for (index_t i = from; i <= to; ++i) {
      state.logEmplaceBackNoLock(log_t(i, term, buffer_t(buf), "", 0));
    }
  }

  static void setCur(State& state, size_t cur) { state._cur = cur; }
};

// Regression guard for the ADB_PROD_ASSERT inside State::get. Reproduces the
// inconsistent state observed in the production crash (_cur=7023 while _log
// covered 6008-6601), where determineLogBounds underflows start -= _cur and
// the assert fires. This validates the defensive check, not the fix: the
// leader-side fix (atomic snapshot+log read in getSnapshotAndEntries)
// prevents a follower from ever reaching this state.
TEST_F(StateTest, get_crashes_when_cur_beyond_log) {
  auto state = createState();
  populateLog(state, 6008, 6601);
  setCur(state, 7023);

  EXPECT_DEATH_CORE_FREE(state.get(0, 6513), "");
}

// Verify get() returns correct entries when _cur is consistent with the log.
TEST_F(StateTest, get_returns_correct_range) {
  auto state = createState();
  populateLog(state, 100, 199);
  setCur(state, 100);

  auto entries = state.get(100, 150);
  EXPECT_EQ(entries.size(), 51u);
  EXPECT_EQ(entries.front().index, 100u);
  EXPECT_EQ(entries.back().index, 150u);
}

}  // namespace arangodb::consensus
