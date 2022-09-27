////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include <gtest/gtest.h>

#include <fmt/core.h>

#include "Replication2/ReplicatedLog/ReplicatedLog.h"

#include "TestHelper.h"

using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

namespace {
struct Event {
  enum class Type {
    kBecomeLeader,
    kRecoverEntries,
    kBecomeFollower,
    kAcquireSnapshot,
    kCommitIndex,
    kDropEntries,
  } type;
  std::unique_ptr<LogIterator> iterator;
  arangodb::ServerID leader;
  LogIndex index;
};
}  // namespace

struct MyReplicatedStateHandle : IReplicatedStateHandle {
  void becomeLeader() override {
    events.emplace_back(Event{.type = Event::Type::kBecomeLeader});
  }
  void recoverEntries(std::unique_ptr<LogIterator> iterator) override {
    events.emplace_back(Event{.type = Event::Type::kRecoverEntries,
                              .iterator = std::move(iterator)});
  }
  void becomeFollower() override {
    events.emplace_back(Event{.type = Event::Type::kBecomeFollower});
  }
  void acquireSnapshot(arangodb::ServerID leader, LogIndex index) override {
    events.emplace_back(Event{.type = Event::Type::kAcquireSnapshot,
                              .leader = leader,
                              .index = index});
  }
  void commitIndex(LogIndex index) override {
    events.emplace_back(Event{.type = Event::Type::kCommitIndex});
  }
  void dropEntries() override {
    events.emplace_back(Event{.type = Event::Type::kDropEntries});
  }
  std::vector<Event> events;
};

auto operator<<(std::ostream& os, Event const& event) -> std::ostream& {
  auto str = std::invoke([&]() -> std::string {
    switch (event.type) {
      case Event::Type::kBecomeLeader: {
        return "becomeLeader";
      }
      case Event::Type::kRecoverEntries: {
        return "recoverEntries ...";
        // TODO print event.iterator
      }
      case Event::Type::kBecomeFollower: {
        return "becomeFollower";
      }
      case Event::Type::kAcquireSnapshot: {
        return fmt::format("acquireSnapshot leader={} index={}", event.leader,
                           event.index.value);
      }
      case Event::Type::kCommitIndex: {
        return "commitIndex";
      }
      case Event::Type::kDropEntries: {
        return "dropEntries";
      }
    }
  });
  return os << str << "\n";
}

struct ReplicatedLogConnectTest : test::ReplicatedLogTest {
  std::shared_ptr<ReplicatedLog> log = makeReplicatedLog(LogId{12});
  MessageId nextMessageId{0};
};

TEST_F(ReplicatedLogConnectTest, test_become_follower) {
  auto stateHandle = std::make_shared<MyReplicatedStateHandle>();
  auto handle = log->connect(stateHandle);

  auto follower = log->becomeFollower("follower", LogTerm{1}, "leader");

  {
    AppendEntriesRequest request;
    request.leaderId = "leader";
    request.leaderTerm = LogTerm{1};
    request.prevLogEntry = TermIndexPair{LogTerm{0}, LogIndex{0}};
    request.leaderCommit = LogIndex{3};
    request.messageId = ++nextMessageId;
    request.entries = {InMemoryLogEntry(
        PersistingLogEntry(LogTerm{1}, LogIndex{1},
                           LogPayload::createFromString("some payload")))};

    auto f = follower->appendEntries(std::move(request));
    ASSERT_TRUE(f.isReady());
    {
      auto result = f.get();
      EXPECT_EQ(result.logTerm, LogTerm{1});
      EXPECT_EQ(result.errorCode, TRI_ERROR_NO_ERROR);
      EXPECT_EQ(result.reason, AppendEntriesErrorReason{});
    }
  }

  auto& events = stateHandle->events;
  ASSERT_GE(events.size(), 2);
  EXPECT_EQ(events[0].type, Event::Type::kBecomeFollower);
  EXPECT_EQ(events[1].type, Event::Type::kCommitIndex);
  EXPECT_EQ(events[1].index, LogIndex{3});
}
