////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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

#include "Replication2/ReplicatedLog/Components/IMethodsProvider.h"

namespace arangodb {
namespace replication2 {
namespace replicated_log {

inline namespace comp {
struct IFollowerCommitManager;
struct IStorageManager;
struct ICompactionManager;
struct ISnapshotManager;
struct IMessageIdManager;

struct MethodsProviderManager : IMethodsProvider {
  MethodsProviderManager(std::shared_ptr<IFollowerCommitManager> commit,
                         std::shared_ptr<IStorageManager> storage,
                         std::shared_ptr<ICompactionManager> compaction,
                         std::shared_ptr<ISnapshotManager> snapshot,
                         std::shared_ptr<IMessageIdManager> messageIdManager);

  virtual auto getMethods()
      -> std::unique_ptr<IReplicatedLogFollowerMethods> override;

  std::shared_ptr<IFollowerCommitManager> const commit;
  std::shared_ptr<IStorageManager> const storage;
  std::shared_ptr<ICompactionManager> const compaction;
  std::shared_ptr<ISnapshotManager> const snapshot;
  std::shared_ptr<IMessageIdManager> const messageIdManager;
};

}  // namespace comp
}  // namespace replicated_log
}  // namespace replication2
}  // namespace arangodb
