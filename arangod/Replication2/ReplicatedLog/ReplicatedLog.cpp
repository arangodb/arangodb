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
#include "Replication2/ReplicatedLog/LogContextKeys.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogFollower.h"
#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/ReplicatedLog/PersistedLog.h"
#include "RestServer/Metrics.h"

#include <Basics/Exceptions.h>
#include <Basics/voc-errors.h>

#include <Basics/StringUtils.h>
#include <optional>
#include <type_traits>

namespace arangodb::replication2::replicated_log {
struct AbstractFollower;
}

using namespace arangodb;
using namespace arangodb::replication2;

replicated_log::ReplicatedLog::ReplicatedLog(std::unique_ptr<LogCore> core,
                                             std::shared_ptr<ReplicatedLogMetrics> const& metrics,
                                             LogContext const& logContext)
    : _logContext(logContext.with<logContextKeyLogId>(core->logId())),
      _participant(
          std::make_shared<replicated_log::LogUnconfiguredParticipant>(std::move(core), metrics)),
      _metrics(metrics) {}

auto replicated_log::ReplicatedLog::becomeLeader(
    LogLeader::TermData const& termData,
    std::vector<std::shared_ptr<AbstractFollower>> const& follower)
    -> std::shared_ptr<LogLeader> {
  // TODO add check that term is different from current term
  auto leader = std::shared_ptr<LogLeader>{};
  auto deferred = std::invoke([&] {
    std::unique_lock guard(_mutex);
    if (auto term = _participant->getTerm(); term && *term > termData.term) {
      LOG_CTX("b8bf7", INFO, _logContext)
          << "tried to become leader with term " << termData.term
          << ", but current term is " << *term;
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
    }

    auto [logCore, deferred] = std::move(*_participant).resign();
    LOG_CTX("23d7b", DEBUG, _logContext)
        << "becoming leader in term " << termData.term;
    leader = LogLeader::construct(termData, std::move(logCore), follower,
                                  _logContext, _metrics);
    _participant = std::static_pointer_cast<LogParticipantI>(leader);
    _metrics->replicatedLogLeaderTookOverNumber->count();
    return std::move(deferred);
  });

  return leader;
}

auto replicated_log::ReplicatedLog::becomeFollower(ParticipantId id, LogTerm term,
                                                   ParticipantId leaderId)
    -> std::shared_ptr<LogFollower> {
  auto [follower, deferred] = std::invoke([&] {
    std::unique_lock guard(_mutex);
    if (auto currentTerm = _participant->getTerm(); currentTerm && *currentTerm > term) {
      LOG_CTX("c97e9", INFO, _logContext)
          << "tried to become follower with term " << term
          << ", but current term is " << *currentTerm;
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
    }
    auto [logCore, deferred] = std::move(*_participant).resign();
    LOG_CTX("1ed24", DEBUG, _logContext)
        << "becoming follower in term " << term << " with leader " << leaderId;
    auto log = InMemoryLog{_logContext, *logCore};
    auto follower = std::make_shared<LogFollower>(_logContext, _metrics,
                                                  std::move(id), std::move(logCore),
                                                  term, std::move(leaderId), log);
    _participant = std::static_pointer_cast<LogParticipantI>(follower);
    _metrics->replicatedLogStartedFollowingNumber->count();
    return std::make_tuple(follower, std::move(deferred));
  });
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
  auto [core, deferred] = std::invoke([&] {
    std::unique_lock guard(_mutex);
    auto res = std::move(*_participant).resign();
    _participant = nullptr;
    return res;
  });
  return std::move(core);
}
