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

#include "Replication2/DeferredExecution.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "Replication2/ReplicatedLog/types.h"

#include <Futures/Future.h>
#include <Futures/Promise.h>

#include <map>
#include <memory>

namespace arangodb {
class Result;
struct LoggerContext;
}  // namespace arangodb

namespace arangodb::replication2::replicated_log {

struct LogCore;
struct LogStatus;

struct WaitForResult {
  /// @brief contains the _current_ commit index. (Not the index waited for)
  LogIndex currentCommitIndex;
  /// @brief Quorum information
  std::shared_ptr<QuorumData const> quorum;

  WaitForResult(LogIndex index, std::shared_ptr<QuorumData const> quorum);
  WaitForResult() = default;
  WaitForResult(velocypack::Slice);

  void toVelocyPack(velocypack::Builder&) const;
};

/**
 * @brief Interface for a log participant: That is, usually either a leader or a
 * follower (LogLeader and LogFollower). Can also be a
 * LogUnconfiguredParticipant, e.g. during startup. The most prominent thing
 * this interface provides is that each instance is responsible for a singular
 * LogCore, which can be moved out with resign().
 */
struct ILogParticipant {
  [[nodiscard]] virtual auto getStatus() const -> LogStatus = 0;
  virtual ~ILogParticipant() = default;
  [[nodiscard]] virtual auto
  resign() && -> std::tuple<std::unique_ptr<LogCore>, DeferredAction> = 0;

  using WaitForPromise = futures::Promise<WaitForResult>;
  using WaitForFuture = futures::Future<WaitForResult>;
  using WaitForIteratorFuture =
      futures::Future<std::unique_ptr<LogRangeIterator>>;
  using WaitForQueue = std::multimap<LogIndex, WaitForPromise>;

  [[nodiscard]] virtual auto waitFor(LogIndex index) -> WaitForFuture = 0;
  [[nodiscard]] virtual auto waitForIterator(LogIndex index)
      -> WaitForIteratorFuture;
  [[nodiscard]] virtual auto getTerm() const noexcept -> std::optional<LogTerm>;

  [[nodiscard]] virtual auto release(LogIndex doneWithIdx) -> Result = 0;
};

/**
 * @brief Unconfigured log participant, i.e. currently neither a leader nor
 * follower. Holds a LogCore, does nothing else.
 */
struct LogUnconfiguredParticipant final
    : std::enable_shared_from_this<LogUnconfiguredParticipant>,
      ILogParticipant {
  ~LogUnconfiguredParticipant() override;
  explicit LogUnconfiguredParticipant(
      std::unique_ptr<LogCore> logCore,
      std::shared_ptr<ReplicatedLogMetrics> logMetrics);

  [[nodiscard]] auto getStatus() const -> LogStatus override;
  auto
  resign() && -> std::tuple<std::unique_ptr<LogCore>, DeferredAction> override;
  [[nodiscard]] auto waitFor(LogIndex) -> WaitForFuture override;
  [[nodiscard]] auto release(LogIndex doneWithIdx) -> Result override;

 private:
  std::unique_ptr<LogCore> _logCore;
  std::shared_ptr<ReplicatedLogMetrics> const _logMetrics;
};
}  // namespace arangodb::replication2::replicated_log
