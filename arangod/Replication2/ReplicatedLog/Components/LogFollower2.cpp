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

#include "LogFollower2.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_log::refactor;

FollowerManager::FollowerManager(
    std::unique_ptr<replicated_state::IStorageEngineMethods> methods,
    std::unique_ptr<IReplicatedStateHandle> stateHandlePtr,
    std::shared_ptr<FollowerTermInformation const> termInfo,
    std::shared_ptr<ReplicatedLogGlobalSettings const> options)
    : options(options),
      storage(std::make_shared<StorageManager>(std::move(methods))),
      compaction(std::make_shared<CompactionManager>(*storage, options)),
      snapshot(std::make_shared<SnapshotManager>(*storage)),
      stateHandle(
          std::make_shared<StateHandleManager>(std::move(stateHandlePtr))),
      commit(std::make_shared<FollowerCommitManager>(*storage, *stateHandle)),
      appendEntriesManager(std::make_shared<AppendEntriesManager>(
          termInfo, *storage, *snapshot, *compaction, *commit)) {
  auto provider = std::make_unique<MethodsProvider>(*this);
  stateHandle->becomeFollower(std::move(provider));
}
