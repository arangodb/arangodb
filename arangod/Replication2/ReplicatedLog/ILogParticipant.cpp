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

#include "Replication2/LoggerContext.h"
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

auto ::arangodb::replication2::replicated_log::assertQueueNotEmptyOrTryToClear(
    TryToClearParticipant participant, LoggerContext const& loggerContext,
    std::multimap<LogIndex, futures::Promise<WaitForResult>>& queue) noexcept -> TryToClearResult {
  auto const [lcParticipant, ucParticipant, resignError] = std::invoke(
      [participant]() -> std::tuple<std::string_view, std::string_view, ErrorCode> {
        switch (participant) {
          case TryToClearParticipant::Leader:
            return {"leader", "Leader", TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED};
          case TryToClearParticipant::Follower:
            return {"follower", "Follower",
                    TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
        }
      });
  auto result = TryToClearResult::NoProgress;
  try {
    // The queue cannot be empty: resign() clears it while under the mutex,
    // and waitFor also holds the mutex, but refuses to add entries after
    // the leader resigned.
    // This means it should never happen in production code.
    // It used to happen in the C++ unit tests, but shouldn't any more since
    // ~ReplicatedLog resigns the participant (if it wasn't dropped already).
    TRI_ASSERT(queue.empty());

    if (!queue.empty()) {
      LOG_CTX("c1138", ERR, loggerContext)
          << ucParticipant << " destroyed, but queue isn't empty!";
      for (auto it = queue.begin(); it != queue.end(); ) {
        if (!it->second.isFulfilled()) {
          it->second.setException(basics::Exception(resignError, __FILE__, __LINE__));
        } else {
          LOG_CTX("a1db0", ERR, loggerContext)
              << "Fulfilled promise in replication queue!";
        }
        it = queue.erase(it);
        result = TryToClearResult::Partial;
      }
    }
    result = TryToClearResult::Cleared;
  } catch (basics::Exception const& exception) {
    LOG_CTX("c546f", ERR, loggerContext)
        << "Caught exception while destroying a log " << lcParticipant << ": "
        << exception.message();
  } catch (std::exception const& exception) {
    LOG_CTX("61f0d", ERR, loggerContext)
        << "Caught exception while destroying a log " << lcParticipant << ": "
        << exception.what();
  } catch (...) {
    LOG_CTX("338c9", ERR, loggerContext)
        << "Caught unknown exception while destroying a log " << lcParticipant << "!";
  }
  return result;
}
