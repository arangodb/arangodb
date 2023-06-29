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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#include "AppendEntriesManager.h"
#include "Replication2/ReplicatedLog/NetworkMessages.h"
#include "Futures/Future.h"
#include "Replication2/coro-helper.h"
#include "Replication2/ReplicatedLog/Components/IStorageManager.h"
#include "Replication2/ReplicatedLog/Components/ISnapshotManager.h"
#include "Replication2/ReplicatedLog/Components/ICompactionManager.h"
#include "Replication2/ReplicatedLog/Components/IFollowerCommitManager.h"
#include "Replication2/ReplicatedLog/Components/IStateHandleManager.h"
#include "Replication2/ReplicatedLog/Components/MessageIdManager.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/DeferredExecution.h"
#include "Replication2/ReplicatedLog/Algorithms.h"
#include "Logger/LogContextKeys.h"
#include "Replication2/MetricsHelper.h"
#include "Replication2/ReplicatedLog/TermIndexMapping.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Metrics/Counter.h"

using namespace arangodb::replication2::replicated_log::comp;

auto AppendEntriesManager::appendEntries(AppendEntriesRequest request)
    -> futures::Future<AppendEntriesResult> {
  MeasureTimeGuard timer(*metrics->replicatedLogFollowerAppendEntriesRtUs);
  // TODO more metrics?

  LoggerContext lctx =
      loggerContext.with<logContextKeyMessageId>(request.messageId)
          .with<logContextKeyPrevLogIdx>(request.prevLogEntry);
  LOG_CTX("7f407", TRACE, lctx) << "receiving append entries";

  auto self = shared_from_this();  // required for coroutine to keep this alive
  Guarded<GuardedData>::mutex_guard_type guard = guarded.getLockedGuard();
  if (guard->resigned) {
    throw ParticipantResignedException(
        TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE);
  }
  auto requestGuard = guard->requestInFlight.acquire();
  if (not requestGuard) {
    LOG_CTX("58043", INFO, lctx)
        << "rejecting append entries - request in flight";
    co_return AppendEntriesResult::withRejection(
        termInfo->term, request.messageId,
        AppendEntriesErrorReason{
            AppendEntriesErrorReason::ErrorType::kPrevAppendEntriesInFlight},
        guard->snapshot.checkSnapshotState() == SnapshotState::AVAILABLE);
  }

  if (auto error = guard->preflightChecks(request, *termInfo, lctx); error) {
    error->messageId = request.messageId;
    error->logTerm = termInfo->term;
    error->snapshotAvailable =
        guard->snapshot.checkSnapshotState() == SnapshotState::AVAILABLE;
    co_return {std::move(*error)};
  }

  {
    // Invalidate snapshot status if necessary.
    if (request.prevLogEntry == TermIndexPair{} &&
        request.entries.front().entry().logIndex() > LogIndex{1}) {
      LOG_CTX("76553", INFO, lctx) << "log truncated - invalidating snapshot";
      // triggers new snapshot transfer
      if (auto result = guard->snapshot.invalidateSnapshotState();
          result.fail()) {
        LOG_CTX("c0981", ERR, lctx) << "failed to persist: " << result;
        co_return AppendEntriesResult::withPersistenceError(
            termInfo->term, request.messageId, result,
            guard->snapshot.checkSnapshotState() == SnapshotState::AVAILABLE);
      }
    }
  }

  {
    auto store = guard->storage.transaction();
    auto bounds = store->getLogBounds();
    if (bounds.to.saturatedDecrement() != request.prevLogEntry.index) {
      auto startRemoveIndex = request.prevLogEntry.index + 1;
      auto removeRange = intersect(
          LogRange{startRemoveIndex, LogIndex{static_cast<uint64_t>(-1)}},
          bounds);
      LOG_CTX("9272b", DEBUG, lctx)
          << "log does not append cleanly, removing starting at "
          << startRemoveIndex;
      metrics->replicatedLogFollowerEntryDropCount->count(removeRange.count());
      auto f = store->removeBack(startRemoveIndex);
      guard.unlock();
      auto result = co_await asResult(std::move(f));
      guard = self->guarded.getLockedGuard();
      if (guard->resigned) {
        throw ParticipantResignedException(
            TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE);
      }
      if (result.fail()) {
        LOG_CTX("0982a", ERR, lctx) << "failed to persist: " << result;
        co_return AppendEntriesResult::withPersistenceError(
            termInfo->term, request.messageId, result,
            guard->snapshot.checkSnapshotState() == SnapshotState::AVAILABLE);
      }
      store = guard->storage.transaction();
    }

    if (not request.entries.empty()) {
      LOG_CTX("fe3e1", TRACE, lctx)
          << "inserting new log entries count = " << request.entries.size()
          << ", range = [" << request.entries.front().entry().logIndex() << ", "
          << request.entries.back().entry().logIndex() + 1 << ")";
      auto f = store->appendEntries(InMemoryLog{request.entries},
                                    {.waitForSync = request.waitForSync});
      guard.unlock();
      auto result = co_await asResult(std::move(f));
      guard = self->guarded.getLockedGuard();
      if (guard->resigned) {
        throw ParticipantResignedException(
            TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE);
      }
      if (result.fail()) {
        LOG_CTX("7cb3d", ERR, lctx)
            << "failed to persist new entries: " << result;
        co_return AppendEntriesResult::withPersistenceError(
            termInfo->term, request.messageId, result,
            guard->snapshot.checkSnapshotState() == SnapshotState::AVAILABLE);
      }
    }
  }

  guard->compaction.updateLowestIndexToKeep(request.lowestIndexToKeep);
  auto hasSnapshot =
      guard->snapshot.checkSnapshotState() == SnapshotState::AVAILABLE;
  auto action =
      guard->stateHandle.updateCommitIndex(request.leaderCommit, hasSnapshot);
  guard.unlock();
  action.fire();
  requestGuard.reset();
  LOG_CTX("f5ecd", TRACE, lctx) << "append entries successful";
  co_return AppendEntriesResult::withOk(termInfo->term, request.messageId,
                                        hasSnapshot);
}

AppendEntriesManager::AppendEntriesManager(
    std::shared_ptr<FollowerTermInformation const> termInfo,
    IStorageManager& storage, ISnapshotManager& snapshot,
    ICompactionManager& compaction, IStateHandleManager& stateHandle,
    IMessageIdManager& messageIdManager,
    std::shared_ptr<ReplicatedLogMetrics> metrics,
    LoggerContext const& loggerContext)
    : loggerContext(loggerContext.with<logContextKeyLogComponent>(
          "append-entries-manager")),
      termInfo(std::move(termInfo)),
      metrics(std::move(metrics)),
      guarded(storage, snapshot, compaction, stateHandle, messageIdManager) {}

auto AppendEntriesManager::resign() && noexcept -> void {
  auto guard = guarded.getLockedGuard();
  return std::move(guard.get()).resign();
}

AppendEntriesManager::GuardedData::GuardedData(
    IStorageManager& storage, ISnapshotManager& snapshot,
    ICompactionManager& compaction, IStateHandleManager& stateHandle,
    IMessageIdManager& messageIdManager)
    : storage(storage),
      snapshot(snapshot),
      compaction(compaction),
      stateHandle(stateHandle),
      messageIdManager(messageIdManager) {}

auto AppendEntriesManager::GuardedData::preflightChecks(
    AppendEntriesRequest const& request,
    FollowerTermInformation const& termInfo, LoggerContext const& lctx)
    -> std::optional<AppendEntriesResult> {
  // First check for term, then check for message id. The message id is reset
  // on a term change. If an old leader still sends a message to a new follower,
  // the next accepted message id will be a very high value.
  if (request.leaderTerm != termInfo.term) {
    LOG_CTX("8ef92", DEBUG, lctx)
        << "rejecting append entries - wrong term - expected " << termInfo.term
        << " found " << request.leaderTerm;
    return AppendEntriesResult::withRejection(
        termInfo.term, request.messageId,
        {AppendEntriesErrorReason::ErrorType::kWrongTerm}, false);
  }

  if (not messageIdManager.acceptReceivedMessageId(request.messageId)) {
    LOG_CTX("bef55", INFO, lctx)
        << "rejecting append entries - dropping outdated message "
        << request.messageId << " expected > "
        << messageIdManager.getLastReceivedMessageId();
    return AppendEntriesResult::withRejection(
        termInfo.term, request.messageId,
        {AppendEntriesErrorReason::ErrorType::kMessageOutdated}, false);
  }

  if (request.leaderId != termInfo.leader) {
    LOG_CTX("d04a9", DEBUG, lctx)
        << "rejecting append entries - wrong leader - expected "
        << termInfo.leader.value_or("<none>") << " found " << request.leaderId;
    return AppendEntriesResult::withRejection(
        termInfo.term, request.messageId,
        {AppendEntriesErrorReason::ErrorType::kInvalidLeaderId}, false);
  }

  // It is always allowed to replace the log entirely
  if (request.prevLogEntry.index > LogIndex{0}) {
    auto termIndexMap = storage.getTermIndexMapping();
    if (auto conflict =
            algorithms::detectConflict(termIndexMap, request.prevLogEntry);
        conflict.has_value()) {
      auto [reason, next] = *conflict;
      LOG_CTX("568c7", TRACE, lctx)
          << "rejecting append entries - log conflict - reason "
          << to_string(reason) << " next " << next;
      return AppendEntriesResult::withConflict(termInfo.term, request.messageId,
                                               next, false);
    }
  }

  return std::nullopt;
}

auto AppendEntriesManager::GuardedData::resign() && noexcept -> void {
  resigned = true;
}
