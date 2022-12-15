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
#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/ReplicatedLog/PersistedLog.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"
#include "Replication2/ReplicatedLog/Components/LogFollower2.h"
#include "Metrics/Counter.h"
#include "Basics/VelocyPackHelper.h"

#include <Basics/Exceptions.h>
#include <Basics/voc-errors.h>

#include <optional>
#include <utility>
#include "Metrics/Gauge.h"
#include "Logger/LogContextKeys.h"

namespace arangodb::replication2::replicated_log {
struct AbstractFollower;

using namespace arangodb;
using namespace arangodb::replication2;

replicated_log::ReplicatedLog::ReplicatedLog(
    std::unique_ptr<replicated_state::IStorageEngineMethods> storage,
    std::shared_ptr<ReplicatedLogMetrics> metrics,
    std::shared_ptr<ReplicatedLogGlobalSettings const> options,
    std::shared_ptr<IParticipantsFactory> participantsFactory,
    LoggerContext const& logContext, agency::ServerInstanceReference myself)
    : _logContext(logContext),  // TODO is it possible to add myself to the
                                // logger context? even if it is changed later?
      _metrics(std::move(metrics)),
      _options(std::move(options)),
      _participantsFactory(std::move(participantsFactory)),
      _guarded(std::move(storage), std::move(myself)) {
  _metrics->replicatedLogNumber->operator++();
}

replicated_log::ReplicatedLog::~ReplicatedLog() {
  ADB_PROD_ASSERT(_guarded.getLockedGuard()->stateHandle == nullptr)
      << "replicated log is destroyed before it was disconnected";
  _metrics->replicatedLogNumber->operator--();
}

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

auto replicated_log::ReplicatedLog::disconnect(ReplicatedLogConnection conn)
    -> std::unique_ptr<IReplicatedStateHandle> {
  LOG_CTX("66ada", DEBUG, _logContext) << "disconnecting replicated log";
  ADB_PROD_ASSERT(conn._log.get() == this);
  auto guard = _guarded.getLockedGuard();
  conn._log.reset();
  if (not guard->resigned) {
    resetParticipant(guard.get());
  }
  return std::move(guard->stateHandle);
}

auto replicated_log::ReplicatedLog::updateConfig(
    agency::LogPlanTermSpecification term, agency::ParticipantsConfig config)
    -> futures::Future<futures::Unit> {
  auto guard = _guarded.getLockedGuard();

  if (guard->latest) {
    TRI_ASSERT(not(guard->latest->term.term < term.term and
                   guard->latest->config.generation > config.generation) and
               not(guard->latest->term.term > term.term and
                   guard->latest->config.generation < config.generation))
        << "While we may see outdated updates here, it must not happen that we "
           "see a new term with an old generation, or the other way round. "
           "current configuration: "
        << guard->latest->config << ", new configuration: " << config
        << ", current term: " << guard->latest->term << ", new term: " << term;
  }

  bool const termChanged =
      not guard->latest or guard->latest->term.term < term.term;
  bool const generationChanged =
      not guard->latest or guard->latest->config.generation < config.generation;

  if (termChanged) {
    resetParticipant(guard.get());
  }

  if (termChanged or generationChanged) {
    guard->latest.emplace(std::move(term), std::move(config));
    return tryBuildParticipant(guard.get());
  } else {
    // nothing changed, don't do anything
    return {futures::Unit{}};
  }
}

auto replicated_log::ReplicatedLog::updateMyInstanceReference(
    agency::ServerInstanceReference newReference)
    -> futures::Future<futures::Unit> {
  auto guard = _guarded.getLockedGuard();
  if (guard->resigned) {
    LOG_CTX("c1580", TRACE, _logContext)
        << "ignoring update of reboot id. log already resigned";
    return {futures::Unit{}};
  }
  ADB_PROD_ASSERT(newReference.serverId == guard->_myself.serverId);
  bool rebootIdChanged = guard->_myself.rebootId != newReference.rebootId;
  if (rebootIdChanged) {
    LOG_CTX("fa471", INFO, _logContext)
        << "detected a change in reboot id, restarting participant";
    guard->_myself = newReference;
    resetParticipant(guard.get());
    return tryBuildParticipant(guard.get());
  } else {
    return {futures::Unit{}};
  }
}

auto replicated_log::ReplicatedLog::tryBuildParticipant(GuardedData& data)
    -> futures::Future<futures::Unit> {
  if (not data.latest or not(data.stateHandle || data.participant)) {
    LOG_CTX("79005", DEBUG, _logContext)
        << "replicated log not ready, config missing";
    return {futures::Unit{}};  // config or state not yet available
  }

  auto configShared =
      std::make_shared<agency::ParticipantsConfig>(data.latest->config);

  ParticipantContext context = {.loggerContext = _logContext,
                                .stateHandle = std::move(data.stateHandle),
                                .metrics = _metrics,
                                .options = _options};

  if (not data.participant) {
    // rebuild participant
    ADB_PROD_ASSERT(data.methods != nullptr);
    auto const& term = data.latest->term;
    if (term.leader == data._myself) {
      LeaderTermInfo info = {.term = term.term,
                             .myself = data._myself.serverId,
                             .initialConfig = configShared};

      LOG_CTX("79015", DEBUG, _logContext)
          << "replicated log configured as leader in term " << term.term;
      data.participant = _participantsFactory->constructLeader(
          std::move(data.methods), info, std::move(context));
      _metrics->replicatedLogLeaderTookOverNumber->count();
    } else {
      // follower
      FollowerTermInfo info = {
          .term = term.term, .myself = data._myself.serverId, .leader = {}};
      if (term.leader) {
        info.leader = term.leader->serverId;
      }

      LOG_CTX("7aed7", DEBUG, _logContext)
          << "replicated log configured as follower in term " << term.term;
      data.participant = _participantsFactory->constructFollower(
          std::move(data.methods), info, std::move(context));
      _metrics->replicatedLogStartedFollowingNumber->operator++();
    }
  } else if (auto leader =
                 std::dynamic_pointer_cast<ILogLeader>(data.participant);
             leader) {
    LOG_CTX("2c74c", DEBUG, _logContext)
        << "replicated log participants reconfigured with generation "
        << configShared->generation;
    TRI_ASSERT(leader->getQuickStatus().activeParticipantsConfig->generation <
               configShared->generation);
    auto idx = leader->updateParticipantsConfig(configShared);
    return leader->waitFor(idx).thenValue([](auto&&) {});
  }

  ADB_PROD_ASSERT(data.participant != nullptr && data.methods == nullptr);
  return {futures::Unit{}};
}

void ReplicatedLog::resetParticipant(GuardedData& data) {
  ADB_PROD_ASSERT(data.participant != nullptr || data.methods != nullptr);
  if (data.participant) {
    ADB_PROD_ASSERT(data.core == nullptr);
    LOG_CTX("9a54b", DEBUG, _logContext)
        << "reset participant of replicated log";
    DeferredAction action;
    std::tie(data.methods, data.stateHandle, action) =
        std::move(*data.participant).resign2();
#ifndef ARANGODB_USE_GOOGLE_TESTS
    ADB_PROD_ASSERT(data.methods != nullptr);
    ADB_PROD_ASSERT(data.stateHandle != nullptr);
#endif
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

auto ReplicatedLog::resign() && -> std::unique_ptr<
    replicated_state::IStorageEngineMethods> {
  auto guard = _guarded.getLockedGuard();
  LOG_CTX("79025", DEBUG, _logContext) << "replicated log resigned";
  ADB_PROD_ASSERT(not guard->resigned);
  resetParticipant(guard.get());
  guard->resigned = true;
  ADB_PROD_ASSERT(guard->methods != nullptr);
  return std::move(guard->methods);
}

auto ReplicatedLog::getQuickStatus() const -> QuickLogStatus {
  if (auto guard = _guarded.getLockedGuard(); guard->participant) {
    return guard->participant->getQuickStatus();
  }
  return {};
}
auto ReplicatedLog::getStatus() const -> LogStatus {
  if (auto guard = _guarded.getLockedGuard(); guard->participant) {
    return guard->participant->getStatus();
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
    std::unique_ptr<replicated_state::IStorageEngineMethods> methods,
    FollowerTermInfo info, ParticipantContext context)
    -> std::shared_ptr<ILogFollower> {
  std::shared_ptr<ILeaderCommunicator> leaderComm;
  if (info.leader) {
    leaderComm = followerFactory->constructLeaderCommunicator(*info.leader);
  }

  // TODO remove wrapper
  auto info2 = std::make_shared<FollowerTermInformation>(
      FollowerTermInformation{{info.term}, info.leader});

  return std::make_shared<refactor::LogFollowerImpl>(
      info.myself, std::move(methods), std::move(context.stateHandle), info2,
      std::move(context.options), std::move(context.metrics), leaderComm);
}

auto DefaultParticipantsFactory::constructLeader(
    std::unique_ptr<replicated_state::IStorageEngineMethods> methods,
    LeaderTermInfo info, ParticipantContext context)
    -> std::shared_ptr<ILogLeader> {
  return LogLeader::construct(
      std::move(methods), std::move(info.initialConfig), std::move(info.myself),
      info.term, context.loggerContext, std::move(context.metrics),
      std::move(context.options), std::move(context.stateHandle),
      followerFactory, scheduler);
}

DefaultParticipantsFactory::DefaultParticipantsFactory(
    std::shared_ptr<IAbstractFollowerFactory> followerFactory,
    std::shared_ptr<IScheduler> scheduler)
    : followerFactory(std::move(followerFactory)),
      scheduler(std::move(scheduler)) {}
}  // namespace arangodb::replication2::replicated_log
