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
#include "Basics/Guarded.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Futures/Promise.h"

namespace arangodb {
namespace replication2::replicated_state {
struct IStorageEngineMethods;
}
namespace replication2::replicated_log {
inline namespace comp {

struct StorageManager : IStorageManager,
                        std::enable_shared_from_this<StorageManager> {
  using IStorageEngineMethods = replicated_state::IStorageEngineMethods;

  explicit StorageManager(std::unique_ptr<IStorageEngineMethods> core);
  auto resign() -> std::unique_ptr<IStorageEngineMethods>;
  auto transaction() -> std::unique_ptr<IStorageTransaction> override;

 private:
  friend struct StorageManagerTransaction;

  struct GuardedData;

  struct StorageOperation {
    virtual ~StorageOperation() = default;
    virtual auto run(IStorageEngineMethods& methods) noexcept
        -> futures::Future<Result> = 0;
  };

  struct StorageRequest {
    std::unique_ptr<StorageOperation> operation;
    futures::Promise<Result> promise;

    explicit StorageRequest(std::unique_ptr<StorageOperation> op)
        : operation(std::move(op)) {}
  };

  struct GuardedData {
    explicit GuardedData(std::unique_ptr<IStorageEngineMethods> core);

    InMemoryLog inMemoryLog;
    std::unique_ptr<IStorageEngineMethods> core;
    std::deque<StorageRequest> queue;
    bool workerActive{false};
  };
  Guarded<GuardedData> guardedData;
  using GuardType = Guarded<GuardedData>::mutex_guard_type;

  auto scheduleOperation(GuardType&&, std::unique_ptr<StorageOperation>)
      -> futures::Future<Result>;
  template<typename F>
  auto scheduleOperationLambda(GuardType&&, F&&) -> futures::Future<Result>;
  void triggerQueueWorker(GuardType&&) noexcept;
};

}  // namespace comp
}  // namespace replication2::replicated_log
}  // namespace arangodb
