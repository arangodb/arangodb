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

#include "Replication2/ReplicatedLog/Common.h"
#include "Replication2/ReplicatedLog/types.h"

#include <Futures/Future.h>
#include <Futures/Promise.h>

#include <Replication2/DeferredExecution.h>
#include <Replication2/ReplicatedLogMetrics.h>
#include <map>
#include <memory>

namespace arangodb::replication2::replicated_log {

struct LogCore;

/**
* @brief Interface for a log participant: That is, usually either a leader or a
* follower (LogLeader and LogFollower). Can also be a LogUnconfiguredParticipant,
* e.g. during startup.
* The most prominent thing this interface provides is that each instance is
* responsible for a singular LogCore, which can be moved out with resign().
*/
struct LogParticipantI {
  [[nodiscard]] virtual auto getStatus() const -> LogStatus = 0;
  virtual ~LogParticipantI() = default;
  [[nodiscard]] virtual auto resign() &&
      -> std::tuple<std::unique_ptr<LogCore>, DeferredAction> = 0;

  using WaitForPromise = futures::Promise<std::shared_ptr<QuorumData>>;
  using WaitForFuture = futures::Future<std::shared_ptr<QuorumData>>;
  using WaitForQueue = std::multimap<LogIndex, WaitForPromise>;

  [[nodiscard]] virtual auto waitFor(LogIndex index) -> WaitForFuture = 0;

  using WaitForIteratorFuture = futures::Future<std::unique_ptr<LogIterator>>;
  [[nodiscard]] virtual auto waitForIterator(LogIndex index) -> WaitForIteratorFuture;
};

/**
* @brief Unconfigured log participant, i.e. currently neither a leader nor
* follower. Holds a LogCore, does nothing else.
*/
struct LogUnconfiguredParticipant
    : std::enable_shared_from_this<LogUnconfiguredParticipant>,
      LogParticipantI {
  ~LogUnconfiguredParticipant() override;
  explicit LogUnconfiguredParticipant(std::unique_ptr<LogCore> logCore,
                                      std::shared_ptr<ReplicatedLogMetrics> logMetrics);

  [[nodiscard]] auto getStatus() const -> LogStatus override;
  auto resign() &&
      -> std::tuple<std::unique_ptr<LogCore>, DeferredAction> override;
  [[nodiscard]] auto waitFor(LogIndex) -> WaitForFuture override;
  std::unique_ptr<LogCore> _logCore;
  std::shared_ptr<ReplicatedLogMetrics> const _logMetrics;
};

}  // namespace arangodb::replication2::replicated_log
