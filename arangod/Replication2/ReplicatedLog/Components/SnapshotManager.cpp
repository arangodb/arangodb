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

#include "SnapshotManager.h"

using namespace arangodb::replication2::replicated_log::comp;

auto SnapshotManager::getSnapshotStatus() const noexcept
    -> replicated_state::SnapshotStatus {
  return replicated_state::SnapshotStatus::kCompleted;
}

auto SnapshotManager::checkSnapshotState() noexcept -> SnapshotState {
  return SnapshotState::INVALID;
}

void SnapshotManager::updateSnapshotState(SnapshotState state) {}
