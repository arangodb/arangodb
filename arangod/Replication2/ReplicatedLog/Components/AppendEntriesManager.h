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
#include <optional>
#include "Basics/Guarded.h"

namespace arangodb {
class Result;
namespace futures {
template<typename T>
class Future;
}
namespace replication2::replicated_log {
struct AppendEntriesRequest;
struct AppendEntriesResult;
inline namespace comp {

struct IAppendEntriesManager {
  virtual ~IAppendEntriesManager() = default;
  virtual auto appendEntries(AppendEntriesRequest request)
      -> futures::Future<AppendEntriesResult> = 0;
};

struct IStorageManager;
struct ISnapshotManager;

struct AppendEntriesManager
    : IAppendEntriesManager,
      std::enable_shared_from_this<AppendEntriesManager> {
  AppendEntriesManager(IStorageManager& storage, ISnapshotManager& snapshot);

  auto appendEntries(AppendEntriesRequest request)
      -> futures::Future<AppendEntriesResult> override;

  struct GuardedData {
    GuardedData(IStorageManager& storage, ISnapshotManager& snapshot);
    auto preflightChecks() -> std::optional<AppendEntriesResult>;

    bool requestInFlight{false};

    IStorageManager& storage;
    ISnapshotManager& snapshot;
  };

  Guarded<GuardedData> guarded;
};
}  // namespace comp
}  // namespace replication2::replicated_log
}  // namespace arangodb
