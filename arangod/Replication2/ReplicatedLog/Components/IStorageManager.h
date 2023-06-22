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

#pragma once

#include <memory>

#include "Replication2/ReplicatedLog/LogCommon.h"

namespace arangodb {
namespace futures {
template<typename T>
class Future;
}
class Result;
namespace replication2::replicated_state {
struct IStorageEngineMethods;
struct PersistedStateInfo;
}  // namespace replication2::replicated_state
namespace replication2 {
struct LogRange;
struct LogIndex;
class LogEntryView;
struct PersistedLogIterator;
template<typename T>
struct TypedLogRangeIterator;
namespace replicated_log {
struct InMemoryLog;
struct TermIndexMapping;

inline namespace comp {

struct IStorageTransaction {
  virtual ~IStorageTransaction() = default;
  [[nodiscard]] virtual auto getLogBounds() const noexcept -> LogRange = 0;
  virtual auto removeFront(LogIndex stop) noexcept
      -> futures::Future<Result> = 0;
  virtual auto removeBack(LogIndex start) noexcept
      -> futures::Future<Result> = 0;
  virtual auto appendEntries(InMemoryLog) noexcept
      -> futures::Future<Result> = 0;
};

struct IStorageManager;

struct IStateInfoTransaction {
  using InfoType = replicated_state::PersistedStateInfo;
  virtual ~IStateInfoTransaction() = default;
  virtual auto get() noexcept -> InfoType& = 0;
};

struct IStorageManager {
  virtual ~IStorageManager() = default;
  virtual auto transaction() -> std::unique_ptr<IStorageTransaction> = 0;
  [[nodiscard]] virtual auto getTermIndexMapping() const
      -> TermIndexMapping = 0;
  [[nodiscard]] virtual auto getCommittedLogIterator(
      std::optional<LogRange> range = std::nullopt) const
      -> std::unique_ptr<TypedLogRangeIterator<LogEntryView>> = 0;
  [[nodiscard]] virtual auto getCommittedMetaInfo() const
      -> replicated_state::PersistedStateInfo = 0;
  [[nodiscard]] virtual auto getPersistedLogIterator(LogIndex first) const
      -> std::unique_ptr<PersistedLogIterator> = 0;
  [[nodiscard]] virtual auto getSyncIndex() const -> LogIndex = 0;

  virtual auto beginMetaInfoTrx() -> std::unique_ptr<IStateInfoTransaction> = 0;
  virtual auto commitMetaInfoTrx(std::unique_ptr<IStateInfoTransaction>)
      -> Result = 0;
};

}  // namespace comp
}  // namespace replicated_log
}  // namespace replication2
}  // namespace arangodb
