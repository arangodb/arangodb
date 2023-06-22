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
#include "Replication2/ReplicatedLog/Components/ISnapshotManager.h"
#include "Basics/Guarded.h"
#include "Replication2/LoggerContext.h"

namespace arangodb {
class Result;
}
namespace arangodb::replication2::replicated_log {
struct FollowerTermInformation;
struct ILeaderCommunicator;
struct MessageId;
inline namespace comp {
struct IStorageManager;
struct IStateHandleManager;

struct SnapshotManager : ISnapshotManager {
  explicit SnapshotManager(
      IStorageManager& storage, IStateHandleManager& stateHandle,
      std::shared_ptr<FollowerTermInformation const> termInfo,
      std::shared_ptr<ILeaderCommunicator> leaderComm,
      LoggerContext const& loggerContext);

  [[nodiscard]] auto invalidateSnapshotState() -> Result override;
  [[nodiscard]] auto checkSnapshotState() const noexcept
      -> SnapshotState override;

  // should be called once after construction
  void acquireSnapshotIfNecessary();

  [[nodiscard]] auto setSnapshotStateAvailable(MessageId msgId,
                                               std::uint64_t version)
      -> Result override;

 private:
  struct GuardedData {
    explicit GuardedData(IStorageManager& storage,
                         IStateHandleManager& stateHandle);
    IStorageManager& storage;
    IStateHandleManager& stateHandle;
    SnapshotState state;
    // this version is volatile and resets after reboot
    std::uint64_t lastSnapshotVersion{0};

    auto updatePersistedSnapshotState(SnapshotState newState) -> Result;
  };
  std::shared_ptr<ILeaderCommunicator> const leaderComm;
  std::shared_ptr<FollowerTermInformation const> const termInfo;
  LoggerContext const loggerContext;
  Guarded<GuardedData> guardedData;
};
}  // namespace comp
}  // namespace arangodb::replication2::replicated_log
