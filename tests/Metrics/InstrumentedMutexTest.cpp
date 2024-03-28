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
////////////////////////////////////////////////////////////////////////////////

#include <Metrics/InstrumentedMutex.h>
#include <Metrics/Gauge.h>

#include <mutex>
#include <shared_mutex>
#include <gtest/gtest.h>

using namespace arangodb;

namespace {

struct InstrumentedMutexTest : ::testing::Test {
  metrics::Gauge<uint64_t> pendingExclusive{0, "pendingExclusive", "", ""};
  metrics::Gauge<uint64_t> pendingShared{0, "pendingShared", "", ""};
  metrics::Gauge<uint64_t> lockExclusive{0, "lockExclusive", "", ""};
  metrics::Gauge<uint64_t> lockShared{0, "lockShared", "", ""};

  InstrumentedMutexMetrics const metrics = {
      .pendingExclusive = &pendingExclusive,
      .pendingShared = &pendingShared,
      .lockExclusive = &lockExclusive,
      .lockShared = &lockShared,
  };
};

}  // namespace

TEST_F(InstrumentedMutexTest, mutex_test) {
  InstrumentedMutex<std::mutex> m{metrics};

  ASSERT_EQ(lockExclusive.load(), 0);
  ASSERT_EQ(pendingExclusive.load(), 0);

  auto guard = m.lock_exclusive();
  ASSERT_TRUE(guard.owns_lock());
  ASSERT_TRUE(guard);

  ASSERT_EQ(lockExclusive.load(), 1);
  ASSERT_EQ(pendingExclusive.load(), 0);

  auto guard2 = m.try_lock_exclusive();
  ASSERT_FALSE(guard2.owns_lock());
  ASSERT_FALSE(guard2);

  guard.unlock();
  ASSERT_FALSE(guard.owns_lock());
  ASSERT_EQ(lockExclusive.load(), 0);
  ASSERT_EQ(pendingExclusive.load(), 0);

  auto guard3 = m.try_lock_exclusive();
  ASSERT_TRUE(guard3.owns_lock());

  ASSERT_EQ(lockExclusive.load(), 1);
  ASSERT_EQ(pendingExclusive.load(), 0);
}

TEST_F(InstrumentedMutexTest, shared_mutex_test) {
  InstrumentedMutex<std::shared_mutex> m{metrics};

  ASSERT_EQ(lockExclusive.load(), 0);
  ASSERT_EQ(pendingExclusive.load(), 0);

  auto guard = m.lock_exclusive();
  ASSERT_TRUE(guard.owns_lock());
  ASSERT_TRUE(guard);

  ASSERT_EQ(lockExclusive.load(), 1);
  ASSERT_EQ(pendingExclusive.load(), 0);

  auto guard2 = m.try_lock_exclusive();
  ASSERT_FALSE(guard2.owns_lock());
  ASSERT_FALSE(guard2);

  auto guard2_1 = m.try_lock_shared();
  ASSERT_FALSE(guard2.owns_lock());
  ASSERT_FALSE(guard2);

  guard.unlock();
  ASSERT_FALSE(guard.owns_lock());
  ASSERT_EQ(lockExclusive.load(), 0);
  ASSERT_EQ(pendingExclusive.load(), 0);

  auto guard3 = m.try_lock_shared();
  ASSERT_TRUE(guard3.owns_lock());

  ASSERT_EQ(lockShared.load(), 1);
  ASSERT_EQ(lockExclusive.load(), 0);
  ASSERT_EQ(pendingExclusive.load(), 0);

  auto guard4 = m.lock_shared();
  ASSERT_TRUE(guard4.owns_lock());

  ASSERT_EQ(lockShared.load(), 2);
  ASSERT_EQ(lockExclusive.load(), 0);
  ASSERT_EQ(pendingExclusive.load(), 0);
}
