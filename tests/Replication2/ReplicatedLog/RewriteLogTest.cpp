////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogEntry.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/types.h"

#include "Replication2/Helper/ReplicatedLogTestSetup.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

// Allows matching a log entry partially in gtest EXPECT_THAT. Members set to
// std::nullopt are ignored when matching; only the set members are matched.
struct PartialLogEntry {
  std::optional<LogTerm> term{};
  std::optional<LogIndex> index{};
  // Note: Add more (optional) fields to IsMeta/IsPayload as needed;
  // then match them in MatchesMapLogEntry, and print them in PrintTo.
  struct IsMeta {};
  struct IsPayload {};
  std::variant<std::nullopt_t, IsMeta, IsPayload> payload = std::nullopt;

  friend void PrintTo(PartialLogEntry const& point, std::ostream* os) {
    *os << "(";
    if (point.term) {
      *os << *point.term;
    } else {
      *os << "?";
    }
    *os << ":";
    if (point.index) {
      *os << *point.index;
    } else {
      *os << "?";
    }
    *os << ";";
    std::visit(overload{
                   [&](std::nullopt_t) { *os << "?"; },
                   [&](IsMeta const&) { *os << "meta=?"; },
                   [&](IsPayload const&) { *os << "payload=?"; },
               },
               point.payload);
    *os << ")";
  }
};
using PartialLogEntries = std::initializer_list<PartialLogEntry>;

namespace arangodb::replication2 {
void PrintTo(LogEntry const& entry, std::ostream* os) {
  *os << "(";
  *os << entry.logIndex() << ":" << entry.logTerm() << ";";
  if (entry.hasPayload()) {
    *os << "payload=" << entry.logPayload()->slice().toJson();
  } else {
    auto builder = velocypack::Builder();
    entry.meta()->toVelocyPack(builder);
    *os << "meta=" << builder.slice().toJson();
  }
  *os << ")";
}
}  // namespace arangodb::replication2

MATCHER_P2(IsTermIndexPair, term, index, "") {
  return arg.term == term and arg.index == index;
}

// Matches a map entry pair (LogIndex, LogEntry) against a PartialLogEntry.
MATCHER(MatchesMapLogEntry,
        fmt::format("{} log entries", negation ? "doesn't match" : "matches")) {
  auto const& logIndex = std::get<0>(arg).first;
  auto const& logEntry = std::get<0>(arg).second;
  auto const& partialLogEntry = std::get<1>(arg);
  return (not partialLogEntry.term.has_value() or
          partialLogEntry.term == logEntry.logTerm()) and
         (not partialLogEntry.index.has_value() or
          (partialLogEntry.index == logIndex and
           partialLogEntry.index == logEntry.logIndex())) and
         (std::visit(overload{
                         [](std::nullopt_t) { return true; },
                         [&](PartialLogEntry::IsPayload const& payload) {
                           return logEntry.hasPayload();
                         },
                         [&](PartialLogEntry::IsMeta const& payload) {
                           return logEntry.hasMeta();
                         },
                     },
                     partialLogEntry.payload));
}

struct RewriteLogTest : ReplicatedLogTest {};

TEST_F(RewriteLogTest, rewrite_old_leader) {
  // create one log that has three entries:
  // (1:1), (2:2), (3:2)
  auto followerLogContainer =
      makeLogWithFakes({.initialLogRange = LogRange{LogIndex{1}, LogIndex{2}}});
  followerLogContainer.storageContext->emplaceLogRange(
      LogRange{LogIndex{2}, LogIndex{4}}, LogTerm{2});

  // create different log that has only one entry
  // (1:1)
  auto leaderLogContainer =
      makeLogWithFakes({.initialLogRange = LogRange{LogIndex{1}, LogIndex{2}}});

  auto config = makeConfig(leaderLogContainer, {followerLogContainer},
                           {.term = LogTerm{3}, .writeConcern = 2});

  // we start with a snapshot, no need to acquire one
  EXPECT_CALL(*followerLogContainer.stateHandleMock, acquireSnapshot).Times(0);
  auto followerMethodsFuture = followerLogContainer.waitToBecomeFollower();
  auto leaderMethodsFuture = leaderLogContainer.waitForLeadership();

  {
    auto fut = followerLogContainer.updateConfig(config);
    EXPECT_TRUE(fut.isReady());
    ASSERT_TRUE(followerMethodsFuture.isReady());
    ASSERT_TRUE(followerMethodsFuture.result().hasValue());
  }

  auto followerMethods = std::move(followerMethodsFuture).get();
  ASSERT_NE(followerMethods, nullptr);

  {
    auto fut = leaderLogContainer.updateConfig(config);
    // write concern is 2, leadership can't be established yet
    EXPECT_FALSE(fut.isReady());
    EXPECT_FALSE(leaderMethodsFuture.isReady());
  }

  auto leader = leaderLogContainer.getAsLeader();
  auto follower = followerLogContainer.getAsFollower();

  {
    auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant()).local;
    EXPECT_EQ(stats.commitIndex, LogIndex{0});
    // Note that the leader inserts an empty log entry to establish leadership
    EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm(3), LogIndex(2)));
  }
  {
    auto stats =
        std::get<FollowerStatus>(follower->getStatus().getVariant()).local;
    EXPECT_EQ(stats.commitIndex, LogIndex{0});
    EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm(2), LogIndex(3)));
  }

  EXPECT_CALL(*followerLogContainer.stateHandleMock,
              updateCommitIndex(testing::Eq(LogIndex{2})));

  // have the leader send the append entries request;
  // have the follower process the append entries request.
  // this should rewrite its log.
  IDelayedScheduler::runAll(
      leaderLogContainer.logScheduler, leaderLogContainer.storageExecutor,
      followerLogContainer.logScheduler, followerLogContainer.storageExecutor,
      followerLogContainer.delayedLogFollower->scheduler);

  EXPECT_FALSE(IDelayedScheduler::hasWork(
      leaderLogContainer.logScheduler, leaderLogContainer.storageExecutor,
      followerLogContainer.logScheduler, followerLogContainer.storageExecutor,
      followerLogContainer.delayedLogFollower->scheduler));

  {
    ASSERT_TRUE(leaderMethodsFuture.isReady());
    ASSERT_TRUE(leaderMethodsFuture.result().hasValue());
  }
  auto leaderMethods = std::move(leaderMethodsFuture).get();
  ASSERT_NE(leaderMethods, nullptr);

  // We got the leader methods, now have to give them back to the leader
  EXPECT_CALL(*leaderLogContainer.stateHandleMock, resignCurrentState)
      .WillOnce(testing::Return(std::move(leaderMethods)));
  EXPECT_CALL(*followerLogContainer.stateHandleMock, resignCurrentState);

  {
    auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant()).local;
    EXPECT_EQ(stats.commitIndex, LogIndex{2});
    EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm(3), LogIndex(2)));
  }
  {
    auto stats =
        std::get<FollowerStatus>(follower->getStatus().getVariant()).local;
    EXPECT_EQ(stats.commitIndex, LogIndex{2});
    EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm(3), LogIndex(2)));
  }

  {
    auto expectedEntries = PartialLogEntries{
        {.term = 1_T, .index = 1_Lx, .payload = PartialLogEntry::IsPayload{}},
        {.term = 3_T, .index = 2_Lx, .payload = PartialLogEntry::IsMeta{}},
    };
    // check the follower's log: this must have been rewritten
    EXPECT_THAT(followerLogContainer.storageContext->log,
                testing::Pointwise(MatchesMapLogEntry(), expectedEntries));
    // just for completeness' sake, check the leader's log as well
    EXPECT_THAT(leaderLogContainer.storageContext->log,
                testing::Pointwise(MatchesMapLogEntry(), expectedEntries));
  }
}
