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
#include "Replication2/ReplicatedLog/ILogInterfaces.h"

namespace arangodb {
struct DeferredAction;
namespace replication2 {
struct LogIndex;
namespace replicated_log {

inline namespace comp {

struct IFollowerCommitManager {
  virtual ~IFollowerCommitManager() = default;
  [[nodiscard]] virtual auto updateCommitIndex(LogIndex commitIndex,
                                               bool snapshotAvailable) noexcept
      -> std::pair<std::optional<LogIndex>, DeferredAction> = 0;
  [[nodiscard]] virtual auto getCommitIndex() const noexcept -> LogIndex = 0;
  [[nodiscard]] virtual auto waitFor(LogIndex index) noexcept
      -> ILogParticipant::WaitForFuture = 0;
  [[nodiscard]] virtual auto waitForIterator(LogIndex index) noexcept
      -> ILogParticipant::WaitForIteratorFuture = 0;
};
}  // namespace comp
}  // namespace replicated_log
}  // namespace replication2
}  // namespace arangodb
