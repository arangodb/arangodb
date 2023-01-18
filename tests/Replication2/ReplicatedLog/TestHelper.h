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
#pragma once

#include <deque>
#include <memory>
#include <utility>

#include "Replication2/ReplicatedLog/LogEntries.h"

namespace arangodb::replication2::test {

using namespace replicated_log;

template<typename I>
struct SimpleIterator : PersistedLogIterator {
  SimpleIterator(I begin, I end) : current(begin), end(end) {}
  ~SimpleIterator() override = default;

  auto next() -> std::optional<PersistingLogEntry> override {
    if (current == end) {
      return std::nullopt;
    }

    return *(current++);
  }

  I current, end;
};

template<typename C, typename Iter = typename C::const_iterator>
auto make_iterator(C const& c) -> std::unique_ptr<SimpleIterator<Iter>> {
  return std::make_unique<SimpleIterator<Iter>>(c.begin(), c.end());
}
/*
struct ReplicatedLogTest : ::testing::Test {
  template<typename MockLogT = MockLog>
  auto makeLogCore(LogId id) -> std::unique_ptr<LogCore> {
    auto persisted = makePersistedLog<MockLogT>(id);
    return std::make_unique<LogCore>(persisted);
  }

  template<typename MockLogT = MockLog>
  auto makeLogCore(GlobalLogIdentifier gid) -> std::unique_ptr<LogCore> {
    auto persisted = makePersistedLog<MockLogT>(std::move(gid));
    return std::make_unique<LogCore>(persisted);
  }

  template<typename MockLogT = MockLog>
  auto getPersistedLogById(LogId id) -> std::shared_ptr<MockLogT> {
    return std::dynamic_pointer_cast<MockLogT>(_persistedLogs.at(id));
  }

  template<typename MockLogT = MockLog>
  auto makePersistedLog(LogId id) -> std::shared_ptr<MockLogT> {
    auto persisted = std::make_shared<MockLogT>(id);
    _persistedLogs[id] = persisted;
    return persisted;
  }

  template<typename MockLogT = MockLog>
  auto makePersistedLog(GlobalLogIdentifier gid) -> std::shared_ptr<MockLogT> {
    auto persisted = std::make_shared<MockLogT>(gid);
    _persistedLogs[gid.id] = persisted;
    return persisted;
  }

  auto makeDelayedPersistedLog(LogId id) {
    return makePersistedLog<DelayedMockLog>(id);
  }

  template<typename MockLogT = MockLog>
  auto makeReplicatedLog(LogId id) -> std::shared_ptr<TestReplicatedLog> {
    auto core = makeLogCore<MockLogT>(id);
    return std::make_shared<TestReplicatedLog>(
        std::move(core), _logMetricsMock, _optionsMock,
        LoggerContext(Logger::REPLICATION2));
  }

  template<typename MockLogT = MockLog>
  auto makeReplicatedLog(GlobalLogIdentifier gid)
      -> std::shared_ptr<TestReplicatedLog> {
    auto core = makeLogCore<MockLogT>(std::move(gid));
    return std::make_shared<TestReplicatedLog>(
        std::move(core), _logMetricsMock, _optionsMock,
        LoggerContext(Logger::REPLICATION2));
  }

  auto makeReplicatedLogWithAsyncMockLog(LogId id)
      -> std::shared_ptr<TestReplicatedLog> {
    auto persisted = std::make_shared<AsyncMockLog>(id);
    _persistedLogs[id] = persisted;
    auto core = std::make_unique<LogCore>(persisted);
    return std::make_shared<TestReplicatedLog>(
        std::move(core), _logMetricsMock, _optionsMock,
        LoggerContext(Logger::REPLICATION2));
  }

  auto defaultLogger() { return LoggerContext(Logger::REPLICATION2); }

  auto createLeaderWithDefaultFlags(
      ParticipantId id, LogTerm term, std::unique_ptr<LogCore> logCore,
      std::vector<std::shared_ptr<AbstractFollower>> const& follower,
      std::size_t effectiveWriteConcern, bool waitForSync = false,
      std::shared_ptr<cluster::IFailureOracle> failureOracle = nullptr)
      -> std::shared_ptr<LogLeader> {
    auto config = agency::LogPlanConfig{effectiveWriteConcern, waitForSync};
    auto participants =
        std::unordered_map<ParticipantId, ParticipantFlags>{{id, {}}};
    for (auto const& participant : follower) {
      participants.emplace(participant->getParticipantId(), ParticipantFlags{});
    }
    auto participantsConfig = std::make_shared<agency::ParticipantsConfig>(
        agency::ParticipantsConfig{.generation = 1,
                                   .participants = std::move(participants),
                                   .config = config});

    if (!failureOracle) {
      failureOracle = std::make_shared<FakeFailureOracle>();
    }

    return LogLeader::construct(
        std::move(logCore), {follower}, std::move(participantsConfig), id, term,
        defaultLogger(), _logMetricsMock, _optionsMock, failureOracle);
  }

  auto stopAsyncMockLogs() -> void {
    for (auto const& it : _persistedLogs) {
      if (auto log = std::dynamic_pointer_cast<AsyncMockLog>(it.second);
          log != nullptr) {
        log->stop();
      }
    }
  }

  std::unordered_map<LogId, std::shared_ptr<MockLog>> _persistedLogs;
  std::shared_ptr<ReplicatedLogMetricsMock> _logMetricsMock =
      std::make_shared<ReplicatedLogMetricsMock>();
  std::shared_ptr<ReplicatedLogGlobalSettings> _optionsMock =
      std::make_shared<ReplicatedLogGlobalSettings>();
  std::string const dbName = "testDb";
};
*/
}  // namespace arangodb::replication2::test
