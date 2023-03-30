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
#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Basics/Guarded.h"

namespace arangodb {
namespace replication2 {
struct LogIndex;
namespace replicated_log {

inline namespace comp {

struct IStorageManager;

struct IWaitQueueManager {
  virtual ~IWaitQueueManager() = default;
  virtual auto waitFor(LogIndex index) noexcept
      -> ILogParticipant::WaitForFuture = 0;
  virtual auto waitForIterator(LogIndex index) noexcept
      -> ILogParticipant::WaitForIteratorFuture = 0;

  virtual auto resolveIndex(LogIndex, WaitForResult) noexcept
      -> DeferredAction = 0;
};

struct WaitQueueManager : IWaitQueueManager,
                          std::enable_shared_from_this<WaitQueueManager> {
  explicit WaitQueueManager(std::shared_ptr<IStorageManager> storage);
  auto waitFor(LogIndex index) noexcept
      -> ILogParticipant::WaitForFuture override;
  auto waitForIterator(LogIndex index) noexcept
      -> ILogParticipant::WaitForIteratorFuture override;
  auto resolveIndex(LogIndex, WaitForResult) noexcept
      -> DeferredAction override;

  void resign() noexcept;

 private:
  using ResolvePromise = futures::Promise<WaitForResult>;
  using ResolveFuture = futures::Future<WaitForResult>;
  using WaitForQueue = std::multimap<LogIndex, ResolvePromise>;

  struct GuardedData {
    LogIndex resolveIndex{0};
    WaitForQueue waitQueue;
    bool isResigned{false};
  };

  Guarded<GuardedData> guardedData;
  std::shared_ptr<IStorageManager> const storage;
};

}  // namespace comp

}  // namespace replicated_log
}  // namespace replication2
}  // namespace arangodb
