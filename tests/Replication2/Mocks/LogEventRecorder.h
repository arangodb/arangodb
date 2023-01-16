////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2022 ArangoDB GmbH, Cologne, Germany
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
#include "Replication2/ReplicatedLog/ReplicatedLog.h"

namespace arangodb::replication2::test {

struct LogEvent {
  enum class Type {
    kResign,
    kLeadershipEstablished,
    kBecomeFollower,
    kAcquireSnapshot,
    kCommitIndex,
    kDropEntries,
  } type;
  std::unique_ptr<LogIterator> iterator;
  arangodb::ServerID leader;
  LogIndex index;
};

struct LogEventRecorder {
  auto createHandle()
      -> std::unique_ptr<replicated_log::IReplicatedStateHandle>;

  std::vector<LogEvent> events;
  std::unique_ptr<replicated_log::IReplicatedLogMethodsBase> methods;
};

struct LogEventRecorderHandle : replicated_log::IReplicatedStateHandle {
  explicit LogEventRecorderHandle(LogEventRecorder& recorder);
  auto resignCurrentState() noexcept
      -> std::unique_ptr<replicated_log::IReplicatedLogMethodsBase> override;
  void leadershipEstablished(
      std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods> ptr)
      override;
  void becomeFollower(
      std::unique_ptr<replicated_log::IReplicatedLogFollowerMethods> ptr)
      override;

  void acquireSnapshot(arangodb::ServerID leader, LogIndex index) override;
  void updateCommitIndex(LogIndex index) override;
  void dropEntries() override;

  [[nodiscard]] auto getStatus() const
      -> std::optional<replicated_state::StateStatus> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  [[nodiscard]] auto getFollower() const -> std::shared_ptr<
      replicated_state::IReplicatedFollowerStateBase> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  [[nodiscard]] auto getLeader() const -> std::shared_ptr<
      replicated_state::IReplicatedLeaderStateBase> override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  LogEventRecorder& recorder;
};

}  // namespace arangodb::replication2::test
