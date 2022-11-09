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
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Basics/Guarded.h"

namespace arangodb {
namespace futures {
template<typename T>
class Future;
}
class Result;
namespace replication2::replicated_log {
inline namespace comp {

struct IStorageInterface {
  virtual auto getInMemoryLog() const noexcept -> InMemoryLog const& = 0;
  virtual auto removeFront(LogIndex stop) noexcept
      -> futures::Future<Result> = 0;
  virtual auto appendEntries(LogIndex appendAfter, InMemoryLogSlice)
      -> futures::Future<Result>;
};

struct StorageTransaction {
  [[nodiscard]] auto getInMemoryLog() const noexcept -> InMemoryLog const&;
  auto removeFront(LogIndex stop) noexcept -> futures::Future<Result>;
  auto removeBack(LogIndex first) noexcept -> futures::Future<Result>;
  auto appendEntries(LogIndex appendAfter, InMemoryLogSlice)
      -> futures::Future<Result>;

 private:
  IStorageInterface* interface;
};

struct IStorageManager {
  virtual ~IStorageManager() = default;
  virtual auto transaction() -> StorageTransaction = 0;
};

struct StorageManager : IStorageManager,
                        std::enable_shared_from_this<StorageManager> {
  explicit StorageManager(std::unique_ptr<LogCore> core);

  // auto getInMemoryLog() const noexcept -> InMemoryLog override;

  // auto removeFront(LogIndex stop) noexcept -> futures::Future<Result>
  // override;

  auto resign() -> std::unique_ptr<LogCore>;

 private:
  struct GuardedData {
    explicit GuardedData(std::unique_ptr<LogCore> core);
    InMemoryLog inMemoryLog;
    std::unique_ptr<LogCore> core;
  };
  Guarded<GuardedData> guardedData;
};

}  // namespace comp
}  // namespace replication2::replicated_log
}  // namespace arangodb
