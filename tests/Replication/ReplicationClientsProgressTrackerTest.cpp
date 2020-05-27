////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include <thread>

#include "gtest/gtest.h"

#include "Logger/LogMacros.h"
#include "Replication/ReplicationClients.h"
#include "Replication/SyncerId.h"
#include "VocBase/Identifiers/ServerId.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

namespace arangodb {
namespace tests {
namespace replication {

class ReplicationClientsProgressTrackerTest_SingleClient
    : public ::testing::TestWithParam<std::pair<SyncerId, ServerId>> {
 protected:
  ReplicationClientsProgressTracker testee{};
  SyncerId syncerId{};
  ServerId clientId{};

  virtual void SetUp() {
    auto const& parm = GetParam();
    syncerId = parm.first;
    clientId = parm.second;
  }
};

double constexpr infty = std::numeric_limits<double>::infinity();

static double now() {
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

static void sleepUntilAfter(double timestamp) {
  using Duration = std::chrono::duration<double>;
  using TimePoint = std::chrono::time_point<std::chrono::steady_clock, Duration>;
  Duration doubleDur = Duration(timestamp);
  TimePoint timePoint(doubleDur);
  std::this_thread::sleep_until(timePoint);
  while (timestamp >= now()) {
    // wait until we've definitely passed the ttl in terms of timestamp precision
  }
  EXPECT_LT(timestamp, now());
};

enum class RetryRv { RETRY, DONE };

// Allows to retry undeterministic code.
static void retryUpTo(int const maxTries, std::function<RetryRv(void)> const& callback) {
  int failures;
  bool done = false;
  for (failures = 0; !done && failures < maxTries; failures++) {
    switch (callback()) {
      case RetryRv::DONE:
        done = true;
        break;
      case RetryRv::RETRY:
        done = false;
        break;
    }
  }
  // If failures is maxTries it means we did not execute the test completely.
  ASSERT_LT(failures, maxTries);
}

TEST_P(ReplicationClientsProgressTrackerTest_SingleClient, test_empty) {
  ASSERT_EQ(UINT64_MAX, testee.lowestServedValue());
}

TEST_P(ReplicationClientsProgressTrackerTest_SingleClient, test_track_untrack) {
  double const ttl = 7200;

  ASSERT_EQ(UINT64_MAX, testee.lowestServedValue());

  testee.track(syncerId, clientId, "", 1, ttl);
  ASSERT_EQ(1, testee.lowestServedValue());
  testee.untrack(syncerId, clientId, "");

  ASSERT_EQ(UINT64_MAX, testee.lowestServedValue());
}

TEST_P(ReplicationClientsProgressTrackerTest_SingleClient, test_track_tick) {
  double const ttl = 7200;

  ASSERT_EQ(UINT64_MAX, testee.lowestServedValue());

  // set last tick
  testee.track(syncerId, clientId, "", 1, ttl);
  ASSERT_EQ(1, testee.lowestServedValue());

  // increase last tick
  testee.track(syncerId, clientId, "", 2, ttl);
  ASSERT_EQ(2, testee.lowestServedValue());

  // decrease last tick
  testee.track(syncerId, clientId, "", 1, ttl);
  ASSERT_EQ(1, testee.lowestServedValue());

  // zero should let the tick unchanged
  testee.track(syncerId, clientId, "", 0, ttl);
  ASSERT_EQ(1, testee.lowestServedValue());
}

TEST_P(ReplicationClientsProgressTrackerTest_SingleClient, test_garbage_collect) {
  double constexpr ttl = 1.0;
  ASSERT_EQ(UINT64_MAX, testee.lowestServedValue());

  // Allow 3 retries of theoretical timing problems
  retryUpTo(3, [&]() {
    double const beforeTrack = now();
    testee.track(syncerId, clientId, "", 1, ttl);
    double const afterTrack = now();
    if (afterTrack - beforeTrack >= ttl) {
      // retry, took too long for the test to work
      return RetryRv::RETRY;
    }
    EXPECT_EQ(1, testee.lowestServedValue());
    // Collect with a timestamp that is definitely, but barely, before the ttl
    // expires.
    testee.garbageCollect(std::nextafter(beforeTrack + ttl, -infty));
    EXPECT_EQ(1, testee.lowestServedValue());
    // Collect with a timestamp that is definitely, but barely, after the ttl
    // expired.
    testee.garbageCollect(std::nextafter(afterTrack + ttl, infty));
    EXPECT_EQ(UINT64_MAX, testee.lowestServedValue());

    return RetryRv::DONE;
  });
}

TEST_P(ReplicationClientsProgressTrackerTest_SingleClient, test_extend_ttl) {
  double constexpr ttl = 1.0;

  ASSERT_EQ(UINT64_MAX, testee.lowestServedValue());

  // Allow 3 retries of theoretical timing problems
  retryUpTo(3, [&]() {
    // track client
    double const beforeTrack = now();
    EXPECT_LT(0.0, beforeTrack);
    testee.track(syncerId, clientId, "", 1, ttl);
    double const afterTrack = now();
    EXPECT_LE(beforeTrack, afterTrack);
    if (afterTrack - beforeTrack >= ttl) {
      // retry, took too long for the test to work
      return RetryRv::RETRY;
    }
    EXPECT_EQ(1, testee.lowestServedValue());
    sleepUntilAfter(afterTrack + ttl);
    // TTL is expired now. But we didn't call garbageCollect yet, so should be
    // able to extend the time:
    EXPECT_EQ(1, testee.lowestServedValue());
    double const beforeExtend = now();
    testee.extend(syncerId, clientId, "", ttl);
    double const afterExtend = now();
    EXPECT_LE(beforeExtend, afterExtend);
    if (afterExtend - beforeExtend >= ttl) {
      // retry, took too long for the test to work
      return RetryRv::RETRY;
    }
    EXPECT_EQ(1, testee.lowestServedValue());
    // Collect with a timestamp that is definitely, but barely, before the ttl
    // expires.
    testee.garbageCollect(std::nextafter(beforeExtend + ttl, -infty));
    EXPECT_EQ(1, testee.lowestServedValue());
    // Collect with a timestamp that is definitely, but barely, after the ttl
    // expired.
    testee.garbageCollect(std::nextafter(afterExtend + ttl, infty));
    EXPECT_EQ(UINT64_MAX, testee.lowestServedValue());

    // test executed successfully, stop retrying
    return RetryRv::DONE;
  });
}

TEST_P(ReplicationClientsProgressTrackerTest_SingleClient, test_track_ttl) {
  double constexpr ttl = 1.0;

  ASSERT_EQ(UINT64_MAX, testee.lowestServedValue());

  // Allow 3 retries of theoretical timing problems
  retryUpTo(3, [&]() {
    // track client
    double const beforeTrack = now();
    EXPECT_LT(0.0, beforeTrack);
    testee.track(syncerId, clientId, "", 1, ttl);
    double const afterTrack = now();
    EXPECT_LE(beforeTrack, afterTrack);
    if (afterTrack - beforeTrack >= ttl) {
      // retry, took too long for the test to work
      return RetryRv::RETRY;
    }
    EXPECT_EQ(1, testee.lowestServedValue());
    sleepUntilAfter(afterTrack + ttl);
    // TTL is expired now. But we didn't call garbageCollect yet, so should be
    // able to extend the time by calling track() again:
    EXPECT_EQ(1, testee.lowestServedValue());
    double const beforeReTrack = now();
    testee.track(syncerId, clientId, "", 1, ttl);
    double const afterReTrack = now();
    EXPECT_LE(beforeReTrack, afterReTrack);
    if (afterReTrack - beforeReTrack >= ttl) {
      // retry, took too long for the test to work
      return RetryRv::RETRY;
    }
    EXPECT_EQ(1, testee.lowestServedValue());
    // Collect with a timestamp that is definitely, but barely, before the ttl
    // expires.
    testee.garbageCollect(std::nextafter(beforeReTrack + ttl, -infty));
    EXPECT_EQ(1, testee.lowestServedValue());
    // Collect with a timestamp that is definitely, but barely, after the ttl
    // expired.
    testee.garbageCollect(std::nextafter(afterReTrack + ttl, infty));
    EXPECT_EQ(UINT64_MAX, testee.lowestServedValue());

    // test executed successfully, stop retrying
    return RetryRv::DONE;
  });
}

INSTANTIATE_TEST_CASE_P(ReplicationClientsProgressTrackerTest_SingleClient,
                        ReplicationClientsProgressTrackerTest_SingleClient,
                        testing::Values(std::make_pair(SyncerId{0}, 23),
                                        std::make_pair(SyncerId{42}, 0),
                                        std::make_pair(SyncerId{42}, 23)));

class ReplicationClientsProgressTrackerTest_MultiClient : public ::testing::Test {
 protected:
  ReplicationClientsProgressTracker testee{};

  double const ttl = 7200;

  struct Client {
    SyncerId const syncerId;
    ServerId const clientId;
    bool operator==(Client const& other) const noexcept {
      return syncerId == other.syncerId && clientId == other.clientId;
    }
  };
  Client const clientA{SyncerId{42}, ServerId{0}};
  Client const clientB{SyncerId{0}, ServerId{23}};
  // should not clash with clientB, as the syncerId should have preference!
  Client const clientC{SyncerId{69}, ServerId{23}};
  // all clientD*s should behave the same, as clientId should be ignored iff syncerId != 0.
  Client const clientD1{SyncerId{23}, ServerId{0}};
  Client const clientD2{SyncerId{23}, ServerId{27}};
  Client const clientD3{SyncerId{23}, ServerId{3}};

  uint64_t tickOfA{UINT64_MAX}, tickOfB{UINT64_MAX}, tickOfC{UINT64_MAX},
      tickOfD{UINT64_MAX};
};

TEST_F(ReplicationClientsProgressTrackerTest_MultiClient, intermittent_tracks_with_mixed_id_types) {
  ASSERT_EQ(UINT64_MAX, testee.lowestServedValue());
  // Track first client, A
  // State {A: 100}
  testee.track(clientA.syncerId, clientA.clientId, "", tickOfA = 100, ttl);
  ASSERT_EQ(tickOfA, testee.lowestServedValue());
  // Add B with a lower tick
  // State {A: 100, B: 99}
  testee.track(clientB.syncerId, clientB.clientId, "", tickOfB = 99, ttl);
  ASSERT_EQ(tickOfB, testee.lowestServedValue());
  // Add C with a lower tick
  // State {A: 100, B: 99, C: 98}
  testee.track(clientC.syncerId, clientC.clientId, "", tickOfC = 98, ttl);
  ASSERT_EQ(tickOfC, testee.lowestServedValue());
  // Reset B, make sure the lowest tick given by C doesn't change
  // State {A: 100, B: 99, C: 98}
  testee.track(clientB.syncerId, clientB.clientId, "", tickOfB = 99, ttl);
  ASSERT_EQ(tickOfC, testee.lowestServedValue());

  // a and b should always refer to the same client.
  for (auto const& a : {clientD1, clientD2, clientD3}) {
    for (auto const& b : {clientD1, clientD2, clientD3}) {
      // Track D with a low tick
      // State {A: 100, B: 99, C: 98, D: 90}
      testee.track(a.syncerId, a.clientId, "", tickOfD = 90, ttl);
      ASSERT_EQ(tickOfD, testee.lowestServedValue());
      // Track D with a higher tick
      // State {A: 100, B: 99, C: 98, D: 95}
      testee.track(b.syncerId, b.clientId, "", tickOfD = 95, ttl);
      ASSERT_EQ(tickOfD, testee.lowestServedValue());
    }
  }
}

TEST_F(ReplicationClientsProgressTrackerTest_MultiClient,
       intermittent_untracks_with_mixed_id_types) {
  ASSERT_EQ(UINT64_MAX, testee.lowestServedValue());
  // Init,
  // State {A: 100, B: 110, C: 120}
  testee.track(clientA.syncerId, clientA.clientId, "", tickOfA = 100, ttl);
  testee.track(clientB.syncerId, clientB.clientId, "", tickOfB = 110, ttl);
  testee.track(clientC.syncerId, clientC.clientId, "", tickOfC = 120, ttl);
  ASSERT_EQ(tickOfA, testee.lowestServedValue());
  // Untracking untracked clients should do nothing
  testee.untrack(clientD1.syncerId, clientD1.clientId, "");
  ASSERT_EQ(tickOfA, testee.lowestServedValue());
  testee.untrack(clientD2.syncerId, clientD2.clientId, "");
  ASSERT_EQ(tickOfA, testee.lowestServedValue());
  testee.untrack(clientD3.syncerId, clientD3.clientId, "");
  ASSERT_EQ(tickOfA, testee.lowestServedValue());
  // Untrack B, should not change the lowest tick
  // State {A: 100, C: 120}
  testee.untrack(clientB.syncerId, clientB.clientId, "");
  ASSERT_EQ(tickOfA, testee.lowestServedValue());
  // Untrack A
  // State {C: 120}
  testee.untrack(clientA.syncerId, clientA.clientId, "");
  ASSERT_EQ(tickOfC, testee.lowestServedValue());

  // a and b should always refer to the same client.
  for (auto const& a : {clientD1, clientD2, clientD3}) {
    for (auto const& b : {clientD1, clientD2, clientD3}) {
      // Track D
      // State {C: 120, D: 90}
      testee.track(a.syncerId, a.clientId, "", tickOfD = 90, ttl);
      ASSERT_EQ(tickOfD, testee.lowestServedValue());
      // Untrack D
      // State {C: 120}
      testee.untrack(b.syncerId, b.clientId, "");
      ASSERT_EQ(tickOfC, testee.lowestServedValue());
    }
  }
  // State {}
  testee.untrack(clientC.syncerId, clientC.clientId, "");
  ASSERT_EQ(UINT64_MAX, testee.lowestServedValue());
}

TEST_F(ReplicationClientsProgressTrackerTest_MultiClient, test_ignored_clients) {
  Client ignoredClient{SyncerId{0}, ServerId{0}};

  ASSERT_EQ(UINT64_MAX, testee.lowestServedValue());

  double const start = now();

  // tracking, extending, or untracking ignored clients should do nothing:
  // State {} for all following statements:
  testee.track(ignoredClient.syncerId, ignoredClient.clientId, "", 1, ttl);
  ASSERT_EQ(UINT64_MAX, testee.lowestServedValue());
  testee.extend(ignoredClient.syncerId, ignoredClient.clientId, "", ttl);
  ASSERT_EQ(UINT64_MAX, testee.lowestServedValue());
  testee.untrack(ignoredClient.syncerId, ignoredClient.clientId, "");
  ASSERT_EQ(UINT64_MAX, testee.lowestServedValue());

  // State {A: 100}
  testee.track(clientA.syncerId, clientA.clientId, "", tickOfA = 100, ttl);
  ASSERT_EQ(tickOfA, testee.lowestServedValue());
  // State {A: 100, D: 101}
  testee.track(clientD3.syncerId, clientD3.clientId, "", tickOfD = 101, ttl);
  ASSERT_EQ(tickOfA, testee.lowestServedValue());

  // Again, tracking ignored clients should do nothing:
  // State {A: 100, D: 101}
  testee.track(ignoredClient.syncerId, ignoredClient.clientId, "", 1, ttl);
  ASSERT_EQ(tickOfA, testee.lowestServedValue());

  // Untracking ignored clients should do nothing:
  // State {A: 100, D: 101}
  testee.untrack(ignoredClient.syncerId, ignoredClient.clientId, "");
  ASSERT_EQ(tickOfA, testee.lowestServedValue());

  // Extending ignored clients should do nothing:
  // State {A: 100, D: 101}
  testee.extend(ignoredClient.syncerId, ignoredClient.clientId, "", 0.1);
  ASSERT_EQ(tickOfA, testee.lowestServedValue());
  double const afterExtend = now();
  double const collectAt = std::nextafter(afterExtend, infty);

  // Make sure the original ttl is not expired
  ASSERT_GT(start + ttl, collectAt);

  // The timestamp `collectAt` is late enough that the TTLs just used for extend
  // are definitely expired.
  // But as they should not have been used, this should do nothing.
  // State {A: 100, D: 101}
  testee.garbageCollect(collectAt);
  ASSERT_EQ(tickOfA, testee.lowestServedValue());

  // Now untrack A, to make sure D is still there and wasn't removed in between:
  // State {D: 101}
  testee.untrack(clientA.syncerId, clientA.clientId, "");
  ASSERT_EQ(tickOfD, testee.lowestServedValue());
}

// TODO A test calling track, extend and untrack with multiple threads do detect
//  possible races would also be useful.

}  // namespace replication
}  // namespace tests
}  // namespace arangodb
