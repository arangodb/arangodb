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

#pragma once

#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "Replication2/ReplicatedLog/WaitForBag.h"

#include <Basics/Guarded.h>

namespace arangodb::replication2::replicated_log {

/**
 * @brief Unconfigured log participant, i.e. currently neither a leader nor
 * follower. Holds a LogCore, does nothing else.
 */
struct LogUnconfiguredParticipant final
    : std::enable_shared_from_this<LogUnconfiguredParticipant>,
      arangodb::replication2::replicated_log::ILogParticipant {
  ~LogUnconfiguredParticipant() override;
  explicit LogUnconfiguredParticipant(
      std::unique_ptr<arangodb::replication2::replicated_log::LogCore> logCore,
      std::shared_ptr<
          arangodb::replication2::replicated_log::ReplicatedLogMetrics>
          logMetrics);

  [[nodiscard]] auto getStatus() const
      -> arangodb::replication2::replicated_log::LogStatus override;
  [[nodiscard]] auto getQuickStatus() const
      -> arangodb::replication2::replicated_log::QuickLogStatus override;
  [[nodiscard]] auto resign() && -> std::tuple<
      std::unique_ptr<arangodb::replication2::replicated_log::LogCore>,
      arangodb::DeferredAction> override;
  [[nodiscard]] auto waitFor(arangodb::replication2::LogIndex)
      -> WaitForFuture override;
  [[nodiscard]] auto release(arangodb::replication2::LogIndex doneWithIdx)
      -> arangodb::Result override;
  [[nodiscard]] auto waitForIterator(arangodb::replication2::LogIndex index)
      -> WaitForIteratorFuture override;
  [[nodiscard]] auto waitForResign() -> futures::Future<futures::Unit> override;
  [[nodiscard]] auto getCommitIndex() const noexcept
      -> arangodb::replication2::LogIndex override;

  [[nodiscard]] auto copyInMemoryLog() const -> InMemoryLog override;

 private:
  std::shared_ptr<
      arangodb::replication2::replicated_log::ReplicatedLogMetrics> const
      _logMetrics;

  struct GuardedData {
    explicit GuardedData(
        std::unique_ptr<arangodb::replication2::replicated_log::LogCore>
            logCore);

    [[nodiscard]] auto resign() && -> std::tuple<
        std::unique_ptr<arangodb::replication2::replicated_log::LogCore>,
        arangodb::DeferredAction>;

    [[nodiscard]] auto didResign() const noexcept -> bool;

    [[nodiscard]] auto waitForResign()
        -> std::pair<futures::Future<futures::Unit>, DeferredAction>;

    std::unique_ptr<arangodb::replication2::replicated_log::LogCore> _logCore;
    WaitForBag _waitForResignQueue;
  };

  arangodb::Guarded<GuardedData> _guardedData;
};

}  // namespace arangodb::replication2::replicated_log
