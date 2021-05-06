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

#include "ReplicatedLog.h"

#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogFollower.h"
#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/ReplicatedLog/PersistedLog.h"

#include <Basics/Exceptions.h>
#include <Basics/voc-errors.h>

#include <optional>
#include <type_traits>

namespace arangodb::replication2::replicated_log {
struct AbstractFollower;
}

using namespace arangodb;
using namespace arangodb::replication2;

auto replicated_log::ReplicatedLog::becomeLeader(
    ParticipantId id, LogTerm term,
    std::vector<std::shared_ptr<AbstractFollower>> const& follower,
    std::size_t writeConcern) -> std::shared_ptr<LogLeader> {
  auto leader = std::shared_ptr<LogLeader>{};
  {
    std::unique_lock guard(_mutex);
    // TODO Resign: will resolve some promises because leader resigned
    //      those promises will call ReplicatedLog::getLeader() -> DEADLOCK
    auto logCore = std::move(*_participant).resign();
    leader = LogLeader::construct(std::move(id), std::move(logCore), term,
                                  follower, writeConcern);
    _participant = std::static_pointer_cast<LogParticipantI>(leader);
  }

  // resolve(promises);

  return leader;
}

auto replicated_log::ReplicatedLog::becomeFollower(ParticipantId id, LogTerm term,
                                                   ParticipantId leaderId)
    -> std::shared_ptr<LogFollower> {
  auto logCore = std::move(*_participant).resign();
  // TODO this is a cheap trick for now. Later we should be aware of the fact
  //      that the log might not start at 1.
  auto iter = logCore->read(LogIndex{0});
  auto log = InMemoryLog{};
  while (auto entry = iter->next()) {
    log._log = log._log.push_back(std::move(entry).value());
  }
  auto follower = std::make_shared<LogFollower>(id, std::move(logCore), term,
                                                std::move(leaderId), log);
  _participant = std::static_pointer_cast<LogParticipantI>(follower);
  return follower;
}

auto replicated_log::ReplicatedLog::getParticipant() const
    -> std::shared_ptr<LogParticipantI> {
  std::unique_lock guard(_mutex);
  if (_participant == nullptr) {
    // TODO better error message
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  return _participant;
}

auto replicated_log::ReplicatedLog::getLeader() const -> std::shared_ptr<LogLeader> {
  auto log = getParticipant();
  if (auto leader =
          std::dynamic_pointer_cast<arangodb::replication2::replicated_log::LogLeader>(log);
      leader != nullptr) {
    return leader;
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_THE_LEADER);
  }
}

auto replicated_log::ReplicatedLog::getFollower() const -> std::shared_ptr<LogFollower> {
  auto log = getParticipant();
  if (auto follower = std::dynamic_pointer_cast<replicated_log::LogFollower>(log);
      follower != nullptr) {
    return follower;
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_THE_LEADER);
  }
}

auto replicated_log::ReplicatedLog::drop() -> std::unique_ptr<LogCore> {
  std::unique_lock guard(_mutex);
  auto core = std::move(*_participant).resign();
  _participant = nullptr;
  return core;
}
