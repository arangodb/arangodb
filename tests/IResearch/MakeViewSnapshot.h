////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "IResearch/ViewSnapshot.h"

namespace arangodb::iresearch {

/// @enum snapshot getting mode
enum class ViewSnapshotMode {
  /// @brief lookup existing snapshot from a transaction
  Find,

  /// @brief lookup existing snapshot from a transaction
  /// or create if it doesn't exist
  FindOrCreate,

  /// @brief retrieve the latest view snapshot and cache it in a transaction
  SyncAndReplace,
};

inline ViewSnapshot* makeViewSnapshot(transaction::Methods& trx,
                                      ViewSnapshotMode mode,
                                      ViewSnapshot::Links&& links,
                                      void const* key, std::string_view name) {
  if (auto* snapshot = getViewSnapshot(trx, key); snapshot != nullptr) {
    if (mode == ViewSnapshotMode::SyncAndReplace) {
      syncViewSnapshot(*snapshot, name);
    }
    return snapshot;
  }
  if (mode == ViewSnapshotMode::Find) {
    return nullptr;
  }
  return makeViewSnapshot(trx, key, mode == ViewSnapshotMode::SyncAndReplace,
                          name, std::move(links));
}

}  // namespace arangodb::iresearch
