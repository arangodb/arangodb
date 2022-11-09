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

namespace arangodb::replication2 {
namespace replicated_state {
struct SnapshotStatus;
}
namespace replicated_log {
inline namespace comp {
enum class SnapshotState { INVALID, AVAILABLE };

struct ISnapshotManager {
  virtual ~ISnapshotManager() = default;
  virtual void updateSnapshotState(SnapshotState) = 0;
  [[nodiscard]] virtual auto getSnapshotStatus() const noexcept
      -> replicated_state::SnapshotStatus = 0;
  virtual auto checkSnapshotState() noexcept -> SnapshotState = 0;
};

struct SnapshotManager : ISnapshotManager {
  void updateSnapshotState(SnapshotState state) override;
  [[nodiscard]] auto getSnapshotStatus() const noexcept
      -> replicated_state::SnapshotStatus override;
  auto checkSnapshotState() noexcept -> SnapshotState override;
};
}  // namespace comp
}  // namespace replicated_log
}  // namespace arangodb::replication2
