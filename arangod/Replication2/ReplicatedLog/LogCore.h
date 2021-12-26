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

#include <memory>
#include <mutex>

#include <Basics/Result.h>
#include <Basics/UnshackledMutex.h>
#include <Futures/Future.h>

#include "Replication2/ReplicatedLog/LogCommon.h"

namespace arangodb::replication2::replicated_log {
struct PersistedLog;
struct PersistedLogIterator;

/**
 * @brief The persistent core of a replicated log. There must only ever by one
 * instance of LogCore for a particular physical log. It is always held by the
 * single active ILogParticipant instance, which in turn lives in the
 * ReplicatedLog instance for this particular log. That is, usually by either a
 * LogLeader, or a LogFollower. If the term changes (and with that
 * leader/followers and/or configuration like writeConcern), a new participant
 * instance is created, and the core moved from the old to the new instance. If
 * the server is currently neither a leader nor follower for the log, e.g.
 * during startup, the LogCore is held by a LogUnconfiguredParticipant instance.
 */
struct alignas(64) LogCore {
  explicit LogCore(std::shared_ptr<PersistedLog> persistedLog);

  // There must only be one LogCore per physical log
  LogCore() = delete;
  LogCore(LogCore const&) = delete;
  LogCore(LogCore&&) = delete;
  auto operator=(LogCore const&) -> LogCore& = delete;
  auto operator=(LogCore&&) -> LogCore& = delete;

  auto insertAsync(std::unique_ptr<PersistedLogIterator> iter, bool waitForSync)
      -> futures::Future<Result>;
  auto insert(PersistedLogIterator& iter, bool waitForSync) -> Result;
  [[nodiscard]] auto read(LogIndex first) const -> std::unique_ptr<PersistedLogIterator>;
  auto removeBack(LogIndex first) -> Result;
  auto removeFront(LogIndex stop) -> Result;

  auto releasePersistedLog() && -> std::shared_ptr<PersistedLog>;

  auto logId() const noexcept -> LogId;

 private:
  std::shared_ptr<PersistedLog> _persistedLog;
  mutable basics::UnshackledMutex _operationMutex;
};

}  // namespace arangodb::replication2::replicated_log
