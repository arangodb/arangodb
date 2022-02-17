////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "LogUnconfiguredParticipant.h"

#include <Basics/Exceptions.h>

#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetricsDeclarations.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

auto LogUnconfiguredParticipant::getStatus() const -> LogStatus {
  return LogStatus{UnconfiguredStatus{}};
}

auto LogUnconfiguredParticipant::getQuickStatus() const -> QuickLogStatus {
  return QuickLogStatus{.role = ParticipantRole::kUnconfigured};
}

LogUnconfiguredParticipant::LogUnconfiguredParticipant(
    std::unique_ptr<LogCore> logCore,
    std::shared_ptr<ReplicatedLogMetrics> logMetrics)
    : _logMetrics(std::move(logMetrics)), _guardedData(std::move(logCore)) {
  _logMetrics->replicatedLogInactiveNumber->fetch_add(1);
}

auto LogUnconfiguredParticipant::resign() && -> std::tuple<
    std::unique_ptr<LogCore>, DeferredAction> {
  return _guardedData.doUnderLock(
      [](auto& data) { return std::move(data).resign(); });
}

auto LogUnconfiguredParticipant::waitFor(LogIndex)
    -> ILogParticipant::WaitForFuture {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

LogUnconfiguredParticipant::~LogUnconfiguredParticipant() {
  _logMetrics->replicatedLogInactiveNumber->fetch_sub(1);
}

auto LogUnconfiguredParticipant::waitForIterator(LogIndex index)
    -> ILogParticipant::WaitForIteratorFuture {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto LogUnconfiguredParticipant::release(LogIndex doneWithIdx) -> Result {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto LogUnconfiguredParticipant::getCommitIndex() const noexcept -> LogIndex {
  return LogIndex{0};  // index 0 is always committed.
}

LogUnconfiguredParticipant::GuardedData::GuardedData(
    std::unique_ptr<arangodb::replication2::replicated_log::LogCore> logCore)
    : _logCore(std::move(logCore)) {}

auto LogUnconfiguredParticipant::waitForResign()
    -> futures::Future<futures::Unit> {
  return _guardedData.doUnderLock(
      [](auto& self) { return self.waitForResign(); });
}

auto LogUnconfiguredParticipant::GuardedData::resign() && -> std::tuple<
    std::unique_ptr<arangodb::replication2::replicated_log::LogCore>,
    arangodb::DeferredAction> {
  auto queue = std::move(_waitForResignQueue);
  auto defer = DeferredAction{
      [queue = std::move(queue)]() mutable noexcept { queue.resolveAll(); }};
  return std::make_tuple(std::move(_logCore), std::move(defer));
}

auto LogUnconfiguredParticipant::GuardedData::waitForResign()
    -> futures::Future<futures::Unit> {
  return _waitForResignQueue.addWaitFor();
}
