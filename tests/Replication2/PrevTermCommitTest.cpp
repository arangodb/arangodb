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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "TestHelper.h"

#include "Replication2/ReplicatedLog/Algorithms.h"

#include <Basics/ScopeGuard.h>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

auto buildLogLocalState(std::shared_ptr<ReplicatedLog> const& log) -> agency::LogCurrentLocalState {
  auto status = log->getParticipant()->getStatus();
  return agency::LogCurrentLocalState(status.getCurrentTerm().value(),
                                      status.getLocalStatistics()->spearHead);
}

TEST_F(ReplicatedLogTest, test_override_committed_entries) {
  auto A = makeReplicatedLog(LogId{1});
  auto B = makeReplicatedLog(LogId{2});
  auto C = makeReplicatedLog(LogId{3});

  {
    // First let A become the leader
    auto Bf = B->becomeFollower("B", LogTerm{1}, "A");
    auto Cf = C->becomeFollower("C", LogTerm{1}, "A");
    auto Al = A->becomeLeader("A", LogTerm{1}, {Bf, Cf}, 2);

    {
      auto const idx = Al->insert(LogPayload::createFromString("first entry"),
                                  false, LogLeader::doNotTriggerAsyncReplication);
      auto f = Al->waitFor(idx);
      ASSERT_FALSE(f.isReady());
      Al->triggerAsyncReplication();
      // let both servers commit the first entry
      while (Cf->hasPendingAppendEntries() || Bf->hasPendingAppendEntries()) {
        Cf->runAsyncAppendEntries();
        Bf->runAsyncAppendEntries();
      }

      ASSERT_TRUE(f.isReady());
      std::move(f).thenFinal([](auto&&) {});
    }

    {
      std::ignore = Al->insert(LogPayload::createFromString("second entry A"),
                               false, LogLeader::doNotTriggerAsyncReplication);
      Al->triggerAsyncReplication();
      ASSERT_TRUE(Bf->hasPendingAppendEntries());
      ASSERT_TRUE(Cf->hasPendingAppendEntries());
    }
  }

  {
      // Check that the algorithm would either pick B or C as new leader
      // algorithms::checkReplicatedLog("<test db>", );
  }

  {
    // Now let C become the leader, assuming A has failed
    auto Af = A->becomeFollower("A", LogTerm{3}, "C");
    auto Bf = B->becomeFollower("B", LogTerm{3}, "C");
    auto Cl = C->becomeLeader("C", LogTerm{3}, {Af, Bf}, 2);

    auto const idx = Cl->insert(LogPayload::createFromString("first entry C"),
                                false, LogLeader::doNotTriggerAsyncReplication);
    // Note that the leader inserts an empty log entry in becomeLeader, which
    // happened twice already.
    ASSERT_EQ(idx, LogIndex{4});
    auto f = Cl->waitFor(idx);
    ASSERT_FALSE(f.isReady());
    Cl->triggerAsyncReplication();

    ASSERT_TRUE(Bf->hasPendingAppendEntries());
    ASSERT_TRUE(Af->hasPendingAppendEntries());
    ASSERT_FALSE(f.isReady());
    std::move(f).thenFinal([](auto&&) {});
  }

  {
    // Check that the algorithm would either pick A as new leader.
    auto const spec = agency::LogPlanSpecification{
        LogId{5},
        agency::LogPlanTermSpecification{LogTerm{3},
                                         LogConfig(2, false),
                                         std::nullopt,
                                         {{"A", {}}, {"B", {}}, {"C", {}}}},
        LogConfig(2, false)};
    auto current = agency::LogCurrent{};
    current.localState["A"] = buildLogLocalState(A);
    current.localState["B"] = buildLogLocalState(B);
    current.localState["C"] = buildLogLocalState(C);
    auto const result = algorithms::checkReplicatedLog(
        "<test db>", spec, current,
        {
            {"A", algorithms::ParticipantRecord{RebootId{1}, true}},
            {"B", algorithms::ParticipantRecord{RebootId{1}, true}},
            {"C", algorithms::ParticipantRecord{RebootId{1}, false}},
        });
    ASSERT_TRUE(std::holds_alternative<agency::LogPlanTermSpecification>(result));
    auto const new_spec = std::get<agency::LogPlanTermSpecification>(result);
    ASSERT_TRUE(new_spec.leader.has_value());
    EXPECT_EQ(new_spec.leader->serverId, ParticipantId{"A"});
  }

  {
    // Now A becomes the leader again and replicates his first entry
    // to B and considers it committed.
    auto Bf = B->becomeFollower("B", LogTerm{5}, "A");
    auto Cf = C->becomeFollower("C", LogTerm{5}, "A");
    auto Al = A->becomeLeader("A", LogTerm{5}, {Bf, Cf}, 2);

    auto f = Al->waitFor(LogIndex{1});

    // The leader is freshly elected. It thus does not know about its followers'
    // states, and therefore cannot calculate a commit index > 0.
    ASSERT_FALSE(f.isReady());

    Al->triggerAsyncReplication();

    ASSERT_TRUE(Bf->hasPendingAppendEntries());
    ASSERT_TRUE(Cf->hasPendingAppendEntries());
    while (Bf->hasPendingAppendEntries()) {
      Bf->runAsyncAppendEntries();
    }

    // The leader should now have replicated its first empty log entry
    ASSERT_TRUE(f.isReady());
    std::move(f).thenFinal([](auto&&) {});
  }

  auto firstCheckPoint = std::vector<std::pair<LogIndex, LogPayload>>();
  {
    auto fut = A->getParticipant()->waitForIterator(LogIndex{1});
    // The log index should have been committed before the checkpoint, so we
    // may assume that the future is already resolved.
    ASSERT_TRUE(fut.isReady());
    auto iter = std::move(fut).get();
    while (auto entry = iter->next()) {
      firstCheckPoint.emplace_back(entry->logIndex(), entry->logPayload());
    }
    EXPECT_EQ(firstCheckPoint.size(), 1);
  }

  {

      // Now A and B should have entry (1, 1) while
      // C has entry (3, 1).
      {auto status = A->getParticipant()->getStatus();
  auto local = status.getLocalStatistics();
  ASSERT_TRUE(local.has_value());
  // Note that the leader inserts an empty log entry in becomeLeader, which
  // happened twice already.
  EXPECT_EQ(local->spearHead.index, LogIndex{4});
  EXPECT_EQ(local->spearHead.term, LogTerm{5});
  EXPECT_EQ(local->commitIndex, LogIndex{1});
}
{
  auto status = B->getParticipant()->getStatus();
  auto local = status.getLocalStatistics();
  ASSERT_TRUE(local.has_value());
  EXPECT_EQ(local->spearHead.index, LogIndex{4});
  EXPECT_EQ(local->spearHead.term, LogTerm{5});
}
{
  auto status = C->getParticipant()->getStatus();
  auto local = status.getLocalStatistics();
  ASSERT_TRUE(local.has_value());
  EXPECT_EQ(local->spearHead.index, LogIndex{4});
  EXPECT_EQ(local->spearHead.term, LogTerm{5});
}
}

{
  // Check that leader election would elect C.
  auto spec = agency::LogPlanSpecification{
      LogId{5},
      agency::LogPlanTermSpecification{LogTerm{5},
                                       LogConfig(2, false),
                                       std::nullopt,
                                       {{"A", {}}, {"B", {}}, {"C", {}}}},
      LogConfig(2, false)};
  auto current = agency::LogCurrent{};
  current.localState["A"] = buildLogLocalState(A);
  current.localState["B"] = buildLogLocalState(B);
  current.localState["C"] = buildLogLocalState(C);
  auto result = algorithms::checkReplicatedLog(
      "<test db>", spec, current,
      {
          {"A", algorithms::ParticipantRecord{RebootId{1}, true}},
          {"B", algorithms::ParticipantRecord{RebootId{1}, true}},
          {"C", algorithms::ParticipantRecord{RebootId{1}, true}},
      });
  ASSERT_TRUE(std::holds_alternative<agency::LogPlanTermSpecification>(result));
  auto new_spec = std::get<agency::LogPlanTermSpecification>(result);
  ASSERT_TRUE(new_spec.leader.has_value());
  EXPECT_EQ(new_spec.leader->serverId, ParticipantId{"C"});
}

{
  auto Af = A->becomeFollower("A", LogTerm{7}, "C");
  auto Bf = B->becomeFollower("B", LogTerm{7}, "C");
  auto Cl = C->becomeLeader("C", LogTerm{7}, {Af, Bf}, 2);

  auto f = Cl->waitFor(LogIndex{1});
  ASSERT_FALSE(f.isReady());
  Cl->triggerAsyncReplication();

  ASSERT_TRUE(Bf->hasPendingAppendEntries());
  ASSERT_TRUE(Af->hasPendingAppendEntries());
  while (Af->hasPendingAppendEntries() || Bf->hasPendingAppendEntries()) {
    Af->runAsyncAppendEntries();
    Bf->runAsyncAppendEntries();
  }
  ASSERT_TRUE(f.isReady());
  std::move(f).thenFinal([](auto&&) {});
}
auto const secondCheckPoint = std::invoke([&] {
  auto result = std::vector<std::pair<LogIndex, LogPayload>>();
  auto fut = C->getParticipant()->waitForIterator(LogIndex{1});
  auto iter = std::move(fut).get();
  while (auto entry = iter->next()) {
    result.emplace_back(entry->logIndex(), entry->logPayload());
  }
  return result;
});
EXPECT_EQ(secondCheckPoint.size(), 2);

{
  // Assert that firstCheckPoint is a prefix of secondCheckPoint
  auto [first, second] =
      std::mismatch(firstCheckPoint.begin(), firstCheckPoint.end(),
                    secondCheckPoint.begin(), secondCheckPoint.end());
  EXPECT_TRUE(first == firstCheckPoint.end());
}
}
