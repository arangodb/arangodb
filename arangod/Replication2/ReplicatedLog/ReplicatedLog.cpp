////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "Logger/LogContextKeys.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogFollower.h"
#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/ReplicatedLog/PersistedLog.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"
#include "Metrics/Counter.h"
#include "Basics/VelocyPackHelper.h"

#include <Basics/Exceptions.h>
#include <Basics/voc-errors.h>

#include <optional>
#include <utility>

namespace arangodb::replication2::replicated_log {
struct AbstractFollower;

using namespace arangodb;
using namespace arangodb::replication2;

replicated_log::ReplicatedLog::ReplicatedLog(
    std::unique_ptr<LogCore> core,
    std::shared_ptr<ReplicatedLogMetrics> metrics,
    std::shared_ptr<ReplicatedLogGlobalSettings const> options,
    std::shared_ptr<IParticipantsFactory> participantsFactory,
    LoggerContext const& logContext, agency::ServerInstanceReference myself)
    : _id(core->logId()),
      _logContext(logContext.with<logContextKeyLogId>(core->logId())),
      _metrics(std::move(metrics)),
      _options(std::move(options)),
      _participantsFactory(std::move(participantsFactory)),
      _myself(std::move(myself)),
      _guarded(std::move(core)) {}

replicated_log::ReplicatedLog::~ReplicatedLog() = default;

auto replicated_log::ReplicatedLog::connect(
    std::unique_ptr<IReplicatedStateHandle> stateHandle)
    -> ReplicatedLogConnection {
  LOG_CTX("8f193", DEBUG, _logContext)
      << "calling connect on replicated log with "
      << typeid(stateHandle.get()).name();
  auto guard = _guarded.getLockedGuard();
  ADB_PROD_ASSERT(guard->stateHandle == nullptr);
  guard->stateHandle = std::move(stateHandle);
  tryBuildParticipant(guard.get());
  return ReplicatedLogConnection(this);
}

void replicated_log::ReplicatedLog::disconnect(ReplicatedLogConnection conn) {
  LOG_CTX("66ada", DEBUG, _logContext) << "disconnecting replicated log";
  ADB_PROD_ASSERT(conn._log.get() == this);
  if (auto guard = _guarded.getLockedGuard(); not guard->resigned) {
    resetParticipant(guard.get());
    guard->stateHandle = nullptr;
  }
  conn._log.reset();
}

void replicated_log::ReplicatedLog::updateConfig(
    agency::LogPlanTermSpecification term, agency::ParticipantsConfig config) {
  auto guard = _guarded.getLockedGuard();
  LOG_CTX("acb02", DEBUG, _logContext)
      << "reconfiguring replicated log " << _id << " with term = " << term
      << " and config = " << config;
  ADB_PROD_ASSERT(not guard->latest || term.term > guard->latest->term.term ||
                  term == guard->latest->term)
      << "term should always increase old = "
      << (guard->latest
              ? arangodb::basics::velocypackhelper::toJson(guard->latest->term)
              : "(none)")
      << " new = " << term;
  ADB_PROD_ASSERT(not guard->latest ||
                  config.generation > guard->latest->config.generation ||
                  config == guard->latest->config)
      << "config generation should always increase. old = "
      << (guard->latest ? arangodb::basics::velocypackhelper::toJson(
                              guard->latest->config)
                        : "(none)")
      << " new = " << config;

  if (guard->latest->term.term < term.term) {
    resetParticipant(guard.get());
  }

  guard->latest.emplace(std::move(term), std::move(config));
  tryBuildParticipant(guard.get());
}

void replicated_log::ReplicatedLog::tryBuildParticipant(GuardedData& data) {
  if (not data.latest or not data.stateHandle) {
    LOG_CTX("79005", DEBUG, _logContext)
        << "replicated log not ready, config missing";
    return;  // config or state not yet available
  }

  auto configShared =
      std::make_shared<agency::ParticipantsConfig>(data.latest->config);

  ParticipantContext context = {.loggerContext = _logContext,
                                .stateHandle = data.stateHandle,
                                .metrics = _metrics,
                                .options = _options};

  if (not data.participant) {
    // rebuild participant
    ADB_PROD_ASSERT(data.core != nullptr);
    auto const& term = data.latest->term;
    if (term.leader == _myself) {
      LeaderTermInfo info = {.term = term.term,
                             .myself = _myself.serverId,
                             .initialConfig = configShared};

      LOG_CTX("79015", DEBUG, _logContext)
          << "replicated log configured as leader in term " << term.term;
      data.participant = _participantsFactory->constructLeader(
          std::move(data.core), info, context);
      _metrics->replicatedLogLeaderTookOverNumber->count();
    } else {
      // follower
      FollowerTermInfo info = {.term = term.term, .myself = _myself.serverId};
      if (term.leader) {
        info.leader = term.leader->serverId;
      }

      LOG_CTX("7aed7", DEBUG, _logContext)
          << "replicated log configured as follower in term " << term.term;
      data.participant = _participantsFactory->constructFollower(
          std::move(data.core), info, context);
      _metrics->replicatedLogStartedFollowingNumber->operator++();
    }
  } else if (auto leader =
                 std::dynamic_pointer_cast<ILogLeader>(data.participant);
             leader) {
    LOG_CTX("2c74c", DEBUG, _logContext)
        << "replicated log participants reconfigured with generation "
        << configShared->generation;
    leader->updateParticipantsConfig(configShared);
  }

  ADB_PROD_ASSERT(data.participant != nullptr && data.core == nullptr);
}

void ReplicatedLog::resetParticipant(GuardedData& data) {
  ADB_PROD_ASSERT(data.participant != nullptr || data.core != nullptr);
  if (data.participant) {
    LOG_CTX("9a54b", DEBUG, _logContext)
        << "reset participant of replicated log";
    auto [core, action] = std::move(*data.participant).resign();
    data.core = std::move(core);
    data.participant.reset();
  }
}

auto ReplicatedLog::getParticipant() const -> std::shared_ptr<ILogParticipant> {
  auto guard = _guarded.getLockedGuard();
  if (guard->participant == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_UNCONFIGURED);
  }
  return guard->participant;
}

auto ReplicatedLog::resign() && -> std::unique_ptr<LogCore> {
  auto guard = _guarded.getLockedGuard();
  LOG_CTX("79025", DEBUG, _logContext) << "replicated log resigned";
  ADB_PROD_ASSERT(not guard->resigned);
  resetParticipant(guard.get());
  guard->resigned = true;
  ADB_PROD_ASSERT(guard->core != nullptr);
  return std::move(guard->core);
}

auto ReplicatedLog::getId() const noexcept -> LogId { return _id; }
auto ReplicatedLog::getQuickStatus() const -> QuickLogStatus {
  if (auto guard = _guarded.getLockedGuard(); guard->participant) {
    return guard->participant->getQuickStatus();
  }
  return {};
}

void ReplicatedLogConnection::disconnect() {
  if (_log) {
    _log->disconnect(std::move(*this));
  }
}

ReplicatedLogConnection::~ReplicatedLogConnection() { disconnect(); }
ReplicatedLogConnection::ReplicatedLogConnection(ReplicatedLog* log)
    : _log(log) {}

auto DefaultParticipantsFactory::constructFollower(
    std::unique_ptr<LogCore> logCore, FollowerTermInfo info,
    ParticipantContext context) -> std::shared_ptr<ILogFollower> {
  return LogFollower::construct(
      context.loggerContext, std::move(context.metrics),
      std::move(context.options), std::move(info.myself), std::move(logCore),
      info.term, std::move(info.leader), std::move(context.stateHandle));
}

auto DefaultParticipantsFactory::constructLeader(
    std::unique_ptr<LogCore> logCore, LeaderTermInfo info,
    ParticipantContext context) -> std::shared_ptr<ILogLeader> {
  return LogLeader::construct(std::move(logCore), std::move(info.initialConfig),
                              std::move(info.myself), info.term,
                              context.loggerContext, std::move(context.metrics),
                              std::move(context.options),
                              std::move(context.stateHandle), followerFactory);
}

DefaultParticipantsFactory::DefaultParticipantsFactory(
    std::shared_ptr<IAbstractFollowerFactory> followerFactory)
    : followerFactory(std::move(followerFactory)) {}
}  // namespace arangodb::replication2::replicated_log
