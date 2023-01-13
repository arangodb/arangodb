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
#include "Replication2/ReplicatedLog/Components/IStorageManager.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Basics/Guarded.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/TermIndexMapping.h"
#include "Futures/Promise.h"
#include <deque>

namespace arangodb::replication2::replicated_log {
inline namespace comp {

struct StorageManagerTransaction;
struct StateInfoTransaction;
struct StorageManager : IStorageManager,
                        std::enable_shared_from_this<StorageManager> {
  using IStorageEngineMethods = replicated_state::IStorageEngineMethods;

  explicit StorageManager(std::unique_ptr<IStorageEngineMethods> core,
                          LoggerContext const& loggerContext);
  auto resign() -> std::unique_ptr<IStorageEngineMethods>;
  auto transaction() -> std::unique_ptr<IStorageTransaction> override;
  auto getCommittedLog() const -> InMemoryLog override;
  auto getTermIndexMapping() const -> TermIndexMapping override;
  auto beginMetaInfoTrx() -> std::unique_ptr<IStateInfoTransaction> override;
  auto commitMetaInfoTrx(std::unique_ptr<IStateInfoTransaction> ptr)
      -> Result override;
  auto getCommittedMetaInfo() const
      -> replicated_state::PersistedStateInfo override;

 private:
  friend struct StorageManagerTransaction;
  friend struct StateInfoTransaction;

  struct GuardedData;

  struct StorageOperation;

  struct StorageRequest {
    std::unique_ptr<StorageOperation> operation;
    InMemoryLog logResult;
    TermIndexMapping mappingResult;
    futures::Promise<Result> promise;

    ~StorageRequest();
    StorageRequest(StorageRequest&&) noexcept = default;
    explicit StorageRequest(std::unique_ptr<StorageOperation> op,
                            InMemoryLog logResult,
                            TermIndexMapping mappingResult);
  };

  struct GuardedData {
    explicit GuardedData(std::unique_ptr<IStorageEngineMethods> methods);

    InMemoryLog onDiskLog, spearheadLog;
    TermIndexMapping onDiskMapping, spearheadMapping;
    std::unique_ptr<IStorageEngineMethods> methods;
    std::deque<StorageRequest> queue;
    replicated_state::PersistedStateInfo info;
    bool workerActive{false};
  };
  Guarded<GuardedData> guardedData;
  using GuardType = Guarded<GuardedData>::mutex_guard_type;
  LoggerContext const loggerContext;

  auto scheduleOperation(GuardType&&, InMemoryLog result,
                         TermIndexMapping mapResult,
                         std::unique_ptr<StorageOperation>)
      -> futures::Future<Result>;
  template<typename F>
  auto scheduleOperationLambda(GuardType&&, InMemoryLog result,
                               TermIndexMapping mapResult, F&&)
      -> futures::Future<Result>;
  void triggerQueueWorker(GuardType) noexcept;
};

}  // namespace comp
}  // namespace arangodb::replication2::replicated_log
