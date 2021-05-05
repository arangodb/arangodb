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

#ifndef ARANGOD_REPLICATION2_REPLICATEDLOG_H
#define ARANGOD_REPLICATION2_REPLICATEDLOG_H

#include "Replication2/ReplicatedLog/LogFollower.h"
#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/ReplicatedLog/LogParticipantI.h"

namespace arangodb::replication2 {

namespace replicated_log {

struct alignas(16) ReplicatedLog {
  ReplicatedLog() = delete;
  ReplicatedLog(ReplicatedLog const&) = delete;
  ReplicatedLog(ReplicatedLog&&) = delete;
  auto operator=(ReplicatedLog const&) -> ReplicatedLog& = delete;
  auto operator=(ReplicatedLog&&) -> ReplicatedLog& = delete;
  explicit ReplicatedLog(std::shared_ptr<LogParticipantI> participant)
      : _participant(std::move(participant)) {}
  explicit ReplicatedLog(std::unique_ptr<LogCore> core)
      : _participant(std::make_shared<LogUnconfiguredParticipant>(std::move(core))) {}

  auto becomeLeader(ParticipantId id, LogTerm term,
                    std::vector<std::shared_ptr<AbstractFollower>> const& follower,
                    std::size_t writeConcern) -> std::shared_ptr<LogLeader>;
  auto becomeFollower(ParticipantId id, LogTerm term, ParticipantId leaderId)
      -> std::shared_ptr<LogFollower>;

  auto getParticipant() const -> std::shared_ptr<LogParticipantI>;

  auto getLeader() const -> std::shared_ptr<LogLeader>;
  auto getFollower() const -> std::shared_ptr<LogFollower>;

 private:
  mutable std::mutex _mutex;
  std::shared_ptr<LogParticipantI> _participant;
};

}  // namespace replicated_log

}  // namespace arangodb::replication2

#endif  // ARANGOD_REPLICATION2_REPLICATEDLOG_H
