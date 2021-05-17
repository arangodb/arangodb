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

#pragma once

#include "Replication2/ReplicatedLog/Common.h"
#include "Replication2/ReplicatedLog/LogParticipantI.h"
#include "Replication2/ReplicatedLogMetrics.h"

#include <iosfwd>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace arangodb::replication2::replicated_log {
class LogFollower;
class LogLeader;
struct AbstractFollower;
struct LogCore;
}  // namespace arangodb::replication2::replicated_log

namespace arangodb::replication2::replicated_log {

/**
 * @brief Container for a replicated log. These are managed by the responsible
 * vocbase. Exactly one instance exists for each replicated log this server is a
 * participant of.
 *
 * It holds a single LogParticipantI; starting with a LogUnconfiguredParticipant,
 * this will usually be either a LogLeader or a LogFollower.
 *
 * The active participant is also responsible for the singular LogCore of this
 * log, providing access to the physical log. The fact that only one LogCore
 * exists, and only one participant has access to it, asserts that only the
 * active instance can write to (or read from) the physical log.
 *
 * ReplicatedLog is responsible for instantiating Participants, and moving the
 * LogCore from the previous active participant to a new one. This happens in
 * becomeLeader and becomeFollower.
 *
 * A mutex must be used to make sure that moving the LogCore from the old to
 * the new participant, and switching the participant pointer, happen
 * atomically.
 */
struct alignas(64) ReplicatedLog {
  explicit ReplicatedLog(std::unique_ptr<LogCore> core, ReplicatedLogMetrics& metrics);

  ReplicatedLog() = delete;
  ReplicatedLog(ReplicatedLog const&) = delete;
  ReplicatedLog(ReplicatedLog&&) = delete;
  auto operator=(ReplicatedLog const&) -> ReplicatedLog& = delete;
  auto operator=(ReplicatedLog&&) -> ReplicatedLog& = delete;

  auto becomeLeader(ParticipantId id, LogTerm term,
                    std::vector<std::shared_ptr<AbstractFollower>> const& follower,
                    std::size_t writeConcern) -> std::shared_ptr<LogLeader>;
  auto becomeFollower(ParticipantId id, LogTerm term, ParticipantId leaderId)
      -> std::shared_ptr<LogFollower>;

  auto getParticipant() const -> std::shared_ptr<LogParticipantI>;

  auto getLeader() const -> std::shared_ptr<LogLeader>;
  auto getFollower() const -> std::shared_ptr<LogFollower>;

  auto drop() -> std::unique_ptr<LogCore>;

  // TODO allow non-void return values
  // template <typename F, std::enable_if_t<std::is_invocable_v<F, std::shared_ptr<LogLeader>>> = 0>
  template <typename F>
  void executeIfLeader(F&& f) {
    auto leaderPtr = std::dynamic_pointer_cast<LogLeader>(getParticipant());
    if (leaderPtr != nullptr) {
      return std::invoke(f, leaderPtr);
    }
  }

  // TODO delete this again, it's a workaround
  bool isUnconfigured() const {
    auto leaderPtr =
        std::dynamic_pointer_cast<LogUnconfiguredParticipant>(getParticipant());
    return leaderPtr != nullptr;
  }

 private:
  mutable std::mutex _mutex;
  std::shared_ptr<LogParticipantI> _participant;
  ReplicatedLogMetrics& _metrics;
};

}  // namespace arangodb::replication2::replicated_log
