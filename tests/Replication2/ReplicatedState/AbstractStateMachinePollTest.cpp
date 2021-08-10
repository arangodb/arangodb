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

#include <Replication2/ReplicatedLog/types.h>
#include <unordered_map>
#include <utility>

#include "StateMachineTestHelper.h"

#include "Replication2/ReplicatedLog/LogFollower.h"
#include "Replication2/ReplicatedLog/LogLeader.h"

#include "Basics/voc-errors.h"
#include "Replication2/ReplicatedState/AbstractStateMachine.h"

using namespace arangodb;
using namespace arangodb::replication2;

struct MyTestStateMachine : replicated_state::AbstractStateMachine<TestLogEntry> {
  explicit MyTestStateMachine(std::shared_ptr<replicated_log::ReplicatedLog> log)
      : replicated_state::AbstractStateMachine<TestLogEntry>(std::move(log)) {}

  auto add(std::string_view value) -> futures::Future<Result> {
    auto idx = insert(TestLogEntry(std::string{value}));
    return waitFor(idx).thenValue([this, weak = weak_from_this(), value = std::string{value}](auto&& res) mutable {
      if (auto self = weak.lock()) {
        _entries.insert(std::move(value));
      }
      return Result{TRI_ERROR_NO_ERROR};
    });
  }

  auto get() -> std::unordered_set<std::string> {
    std::unique_lock guard(mutex);
    return _entries;
  }

 protected:
  auto installSnapshot(ParticipantId const& id) -> futures::Future<Result> override {
    TRI_ASSERT(false);
  }
  auto applyEntries(std::unique_ptr<LogRangeIterator> ptr)
      -> futures::Future<Result> override {
    std::unique_lock guard(mutex);
    while (auto e = ptr->next()) {
      _entries.insert(e->payload);
    }

    return futures::Future<Result>{std::in_place, TRI_ERROR_NO_ERROR};
  }


  std::mutex mutex;
  std::unordered_set<std::string> _entries;
};

struct ParticipantBase {
  explicit ParticipantBase(std::shared_ptr<replicated_log::ReplicatedLog> const& log)
      : state(std::make_shared<MyTestStateMachine>(log)) {}
  std::shared_ptr<MyTestStateMachine> state;
};

struct Follower : ParticipantBase {
  explicit Follower(std::shared_ptr<replicated_log::ReplicatedLog> const& log,
                    ParticipantId const& p, LogTerm term, ParticipantId const& leader)
      : ParticipantBase(log), log(log->becomeFollower(p, term, leader)) {}

  std::shared_ptr<replicated_log::LogFollower> log;
};

struct Leader : ParticipantBase {
  explicit Leader(std::shared_ptr<replicated_log::ReplicatedLog> const& log,
                  LogConfig config, ParticipantId id, LogTerm term,
                  std::vector<std::shared_ptr<replicated_log::AbstractFollower>> const& follower)
      : ParticipantBase(log),
        log(log->becomeLeader(config, std::move(id), term, follower)) {}

  std::shared_ptr<replicated_log::LogLeader> log;
};

TEST_F(StateMachineTest, check_apply_entries) {
  auto A = createReplicatedLog();
  auto B = createReplicatedLog();

  auto follower = std::make_shared<Follower>(B, "B", LogTerm{1}, "A");
  auto leader = std::make_shared<Leader>(
      A, LogConfig{2, false}, "A", LogTerm{1},
      std::vector<std::shared_ptr<replicated_log::AbstractFollower>>{follower->log});

  auto f1 = leader->state->add("first");
  ASSERT_TRUE(f1.isReady());
  auto f = follower->state->pollEntries();
  ASSERT_TRUE(f.isReady());

  using namespace std::string_literals;

  {
    auto set = follower->state->get();
    EXPECT_EQ(set.size(), 1);
    EXPECT_EQ(set, std::unordered_set{"first"s});
  }
  {
    auto set = leader->state->get();
    EXPECT_EQ(set.size(), 1);
    EXPECT_EQ(set, std::unordered_set{"first"s});
  }
}
