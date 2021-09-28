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

#include "ILogParticipant.h"

#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "RestServer/Metrics.h"

#include <Basics/Exceptions.h>
#include <Basics/StaticStrings.h>

using namespace arangodb;
using namespace arangodb::replication2;

auto replicated_log::LogUnconfiguredParticipant::getStatus() const -> LogStatus {
  return LogStatus{UnconfiguredStatus{}};
}

replicated_log::LogUnconfiguredParticipant::LogUnconfiguredParticipant(
    std::unique_ptr<LogCore> logCore, std::shared_ptr<ReplicatedLogMetrics> logMetrics)
    : _logCore(std::move(logCore)), _logMetrics(std::move(logMetrics)) {
  _logMetrics->replicatedLogInactiveNumber->fetch_add(1);
}

auto replicated_log::LogUnconfiguredParticipant::resign() && -> std::tuple<std::unique_ptr<LogCore>, DeferredAction> {
  auto nop = DeferredAction{};
  return std::make_tuple(std::move(_logCore), std::move(nop));
}

auto replicated_log::LogUnconfiguredParticipant::waitFor(LogIndex)
    -> replicated_log::ILogParticipant::WaitForFuture {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
replicated_log::LogUnconfiguredParticipant::~LogUnconfiguredParticipant() {
  _logMetrics->replicatedLogInactiveNumber->fetch_sub(1);
}

auto replicated_log::ILogParticipant::waitForIterator(LogIndex index)
    -> replicated_log::ILogParticipant::WaitForIteratorFuture {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto replicated_log::ILogParticipant::getTerm() const noexcept -> std::optional<LogTerm> {
  return getStatus().getCurrentTerm();
}

auto replicated_log::LogUnconfiguredParticipant::release(LogIndex doneWithIdx) -> Result {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

replicated_log::WaitForResult::WaitForResult(LogIndex index,
                                             std::shared_ptr<QuorumData const> quorum)
    : currentCommitIndex(index), quorum(std::move(quorum)) {}

void replicated_log::WaitForResult::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::CommitIndex, VPackValue(currentCommitIndex));
  builder.add(VPackValue("quorum"));
  quorum->toVelocyPack(builder);
}

replicated_log::WaitForResult::WaitForResult(velocypack::Slice s) {
  currentCommitIndex = s.get(StaticStrings::CommitIndex).extract<LogIndex>();
  quorum = std::make_shared<QuorumData>(s.get("quorum"));
}
