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
#include <string>

namespace arangodb {
struct DeferredAction;
}

namespace arangodb::replication2 {
struct LogIndex;
using ParticipantId = std::string;
namespace replicated_log {
struct IReplicatedStateHandle;
struct IReplicatedLogFollowerMethods;
inline namespace comp {
struct IStateHandleManager {
  virtual ~IStateHandleManager() = default;
  virtual auto updateCommitIndex(LogIndex commitIndex,
                                 bool snapshotAvailable) noexcept
      -> DeferredAction = 0;
  virtual auto resign() noexcept -> std::unique_ptr<IReplicatedStateHandle> = 0;
  virtual void becomeFollower(
      std::unique_ptr<IReplicatedLogFollowerMethods>) = 0;
  virtual void acquireSnapshot(ParticipantId const& leader,
                               std::uint64_t version) noexcept = 0;
};

}  // namespace comp
}  // namespace replicated_log
}  // namespace arangodb::replication2
