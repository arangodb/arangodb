////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include <Replication2/ReplicatedLog/ReplicatedLog.h>
#include <Replication2/Streams/LogMultiplexer.h>

#include <Replication2/Mocks/FakeReplicatedLog.h>
#include <Replication2/Mocks/PersistedLog.h>
#include <Replication2/Mocks/ReplicatedLogMetricsMock.h>

#include <Replication2/Streams/TestLogSpecification.h>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::streams;
using namespace arangodb::replication2::test;

struct LogMultiplexerTest : LogMultiplexerTestBase {};

TEST_F(LogMultiplexerTest, leader_follower_test) {
  auto ints = {12, 13, 14, 15, 16};
  auto strings = {"foo", "bar", "baz", "fuz"};

  auto leaderLog = createReplicatedLog();
  auto followerLog = createReplicatedLog();

  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto leader = leaderLog->becomeLeader(LogConfig(2, 2, 2, false), "leader",
                                        LogTerm{1}, {follower});

  auto mux = LogMultiplexer<MyTestSpecification>::construct(leader);
  auto demux = LogDemultiplexer<MyTestSpecification>::construct(follower);
  demux->listen();

  auto leaderStreamA = mux->getStreamBaseById<my_int_stream_id>();
  auto leaderStreamB = mux->getStreamBaseById<my_string_stream_id>();

  {
    auto iterA = ints.begin();
    auto iterB = strings.begin();
    while (iterA != ints.end() || iterB != strings.end()) {
      if (iterA != ints.end()) {
        leaderStreamA->insert(*iterA);
        ++iterA;
      }
      if (iterB != strings.end()) {
        leaderStreamB->insert(*iterB);
        ++iterB;
      }
    }
  }

  auto followerStreamA = demux->getStreamBaseById<my_int_stream_id>();
  auto followerStreamB = demux->getStreamBaseById<my_string_stream_id>();

  auto futureA = followerStreamA->waitFor(LogIndex{2});
  auto futureB = followerStreamB->waitFor(LogIndex{1});
  ASSERT_TRUE(futureA.isReady());
  ASSERT_TRUE(futureB.isReady());

  {
    auto iter = followerStreamA->getAllEntriesIterator();
    for (auto x : ints) {
      auto entry = iter->next();
      ASSERT_TRUE(entry.has_value()) << "expected value " << x;
      auto const& [index, value] = *entry;
      EXPECT_EQ(value, x);
    }
    EXPECT_EQ(iter->next(), std::nullopt);
  }
  {
    auto iter = followerStreamB->getAllEntriesIterator();
    for (auto x : strings) {
      auto entry = iter->next();
      ASSERT_TRUE(entry.has_value());
      auto const& [index, value] = *entry;
      EXPECT_EQ(value, x);
    }
    EXPECT_EQ(iter->next(), std::nullopt);
  }
}

TEST_F(LogMultiplexerTest, leader_wait_for) {
  auto leaderLog = createReplicatedLog();
  auto followerLog = createFakeReplicatedLog();

  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto leader =
      leaderLog->becomeLeader(LogConfig(2, 2, 2, false), "leader", LogTerm{1}, {follower});
  auto mux = LogMultiplexer<MyTestSpecification>::construct(leader);

  auto stream = mux->getStreamById<my_int_stream_id>();

  // Write an entry and wait for it
  auto idx = stream->insert(12);
  auto f = stream->waitFor(idx);
  // Future not yet resolved because follower did not answer yet
  EXPECT_FALSE(f.isReady());

  // let follower run
  EXPECT_TRUE(follower->hasPendingAppendEntries());
  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  // future should be ready
  ASSERT_TRUE(f.isReady());
}

TEST_F(LogMultiplexerTest, leader_wait_for_multiple) {
  auto leaderLog = createReplicatedLog();
  auto followerLog = createFakeReplicatedLog();

  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto leader = leaderLog->becomeLeader(LogConfig(2, 2, 2, false), "leader",
                                        LogTerm{1}, {follower});
  auto mux = LogMultiplexer<MyTestSpecification>::construct(leader);

  auto streamA = mux->getStreamById<my_int_stream_id>();
  auto streamB = mux->getStreamById<my_string_stream_id>();

  // Write an entry and wait for it
  auto idxA = streamA->insert(12);
  auto fA = streamA->waitFor(idxA);
  // Future not yet resolved because follower did not answer yet
  EXPECT_FALSE(fA.isReady());
  // Follower has pending append entries
  EXPECT_TRUE(follower->hasPendingAppendEntries());

  // Write another entry
  auto idxB = streamB->insert("hello world");
  auto fB = streamB->waitFor(idxB);
  // Both futures are not yet resolved because follower did not answer yet
  EXPECT_FALSE(fB.isReady());
  EXPECT_FALSE(fA.isReady());

  // Do a single follower run
  follower->runAsyncAppendEntries();

  // future A should be ready and follower has still pending append entries
  EXPECT_TRUE(fA.isReady());
  EXPECT_TRUE(follower->hasPendingAppendEntries());

  // Now future B should become ready.
  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }
  EXPECT_TRUE(fB.isReady());
}

TEST_F(LogMultiplexerTest, follower_wait_for) {
  auto leaderLog = createReplicatedLog(LogId{1});
  auto followerLog = createFakeReplicatedLog(LogId{2});

  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto leader = leaderLog->becomeLeader(LogConfig(2, 2, 2, false), "leader",
                                        LogTerm{1}, {follower});
  // handle first leader log entry (empty)
  leader->triggerAsyncReplication();
  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  auto mux = LogMultiplexer<MyTestSpecification>::construct(leader);
  auto demux = LogDemultiplexer<MyTestSpecification>::construct(follower);
  demux->listen();

  auto inStream = mux->getStreamById<my_int_stream_id>();
  auto outStream = demux->getStreamById<my_int_stream_id>();

  auto idx = inStream->insert(17);
  auto f = outStream->waitFor(idx);
  EXPECT_FALSE(f.isReady());
  EXPECT_TRUE(follower->hasPendingAppendEntries());

  // Handle append request, entry not yet committed on follower
  follower->runAsyncAppendEntries();
  EXPECT_FALSE(f.isReady());
  EXPECT_TRUE(follower->hasPendingAppendEntries());

  // Receive commit update
  follower->runAsyncAppendEntries();
  EXPECT_TRUE(f.isReady());
}

TEST_F(LogMultiplexerTest, leader_digest_existing_entries) {
  auto leaderLog = createReplicatedLog(LogId{1});
  auto followerLog = createFakeReplicatedLog(LogId{2});
  {
    // create a leader and follower in term 1
    auto follower =
        followerLog->becomeFollower("follower", LogTerm{1}, "leader");
    auto leader = leaderLog->becomeLeader(LogConfig(2, 2, 2, false), "leader",
                                          LogTerm{1}, {follower});
    auto mux = LogMultiplexer<MyTestSpecification>::construct(leader);
    auto stream = mux->getStreamById<my_int_stream_id>();

    // Write multiple entries
    for (int i = 0; i < 20; i++) {
      stream->insert(i);
    }

    // handle first leader log entry (empty)
    leader->triggerAsyncReplication();
    while (follower->hasPendingAppendEntries()) {
      follower->runAsyncAppendEntries();
    }
  }
  // now everything in memory state is gone
  // wake up in new term
  {
    auto follower =
        followerLog->becomeFollower("follower", LogTerm{2}, "leader");
    auto leader = leaderLog->becomeLeader(LogConfig(2, 2, 2, false), "leader",
                                          LogTerm{2}, {follower});
    // handle first leader log entry (empty)
    leader->triggerAsyncReplication();
    while (follower->hasPendingAppendEntries()) {
      follower->runAsyncAppendEntries();
    }
    TRI_ASSERT(leader->isLeadershipEstablished());
    auto mux = LogMultiplexer<MyTestSpecification>::construct(leader);
    mux->digestAvailableEntries();

    // now read the stream and check if all entries are available
    auto stream = mux->getStreamById<my_int_stream_id>();
    auto f = stream->waitForIterator(LogIndex{0});
    ASSERT_TRUE(f.isReady());
    auto iter = std::move(f).get();

    int i = 0;
    while (auto entry = iter->next()) {
      EXPECT_EQ(entry->second, i);
      i += 1;
    }
  }
}

TEST_F(LogMultiplexerTest, leader_resign_stream) {
  auto leaderLog = createReplicatedLog(LogId{1});
  auto followerLog = createFakeReplicatedLog(LogId{2});

  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto leader = leaderLog->becomeLeader(LogConfig(2, 2, 2, false), "leader",
                                        LogTerm{1}, {follower});
  auto mux = LogMultiplexer<MyTestSpecification>::construct(leader);
  auto stream = mux->getStreamById<my_int_stream_id>();
  mux->digestAvailableEntries();

  // handle first leader log entry (empty)
  leader->triggerAsyncReplication();
  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  // wait for some random log index
  auto f = leader->waitFor(LogIndex{10});
  ASSERT_FALSE(f.isReady());
  auto fs = stream->waitFor(LogIndex{10});
  ASSERT_FALSE(fs.isReady());

  // become leader in new term, this should trigger an exception
  leader = leaderLog->becomeLeader(LogConfig(2, 2, 2, false), "leader", LogTerm{2}, {follower});

  // leader should have resolved this promise
  ASSERT_TRUE(f.isReady());
  EXPECT_TRUE(f.hasException());

  // multiplexer should have resolved this promise
  ASSERT_TRUE(fs.isReady());
  EXPECT_TRUE(fs.hasException());
}

TEST_F(LogMultiplexerTest, follower_resign_stream) {
  auto followerLog = createFakeReplicatedLog(LogId{2});

  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto demux = LogDemultiplexer<MyTestSpecification>::construct(follower);
  demux->listen();
  auto stream = demux->getStreamById<my_int_stream_id>();

  // wait for some random log index
  auto f = follower->waitFor(LogIndex{10});
  ASSERT_FALSE(f.isReady());
  auto fs = stream->waitFor(LogIndex{10});
  ASSERT_FALSE(fs.isReady());

  // become leader in new term, this should trigger an exception
  follower = followerLog->becomeFollower("follower", LogTerm{2}, "leader");

  // leader should have resolved this promise
  ASSERT_TRUE(f.isReady());
  EXPECT_TRUE(f.hasException());

  // multiplexer should have resolved this promise
  ASSERT_TRUE(fs.isReady());
  EXPECT_TRUE(fs.hasException());
}
