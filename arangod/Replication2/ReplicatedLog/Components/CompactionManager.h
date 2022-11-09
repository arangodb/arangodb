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

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Basics/Guarded.h"

namespace arangodb {
template<typename T>
class ResultT;
namespace futures {
template<typename T>
class Future;
}

namespace replication2::replicated_log {
inline namespace comp {

struct IStorageManager;

struct ICompactionManager {
  virtual ~ICompactionManager() = default;
  virtual void updateReleaseIndex(LogIndex) noexcept = 0;
  virtual void updateLargestIndexToKeep(LogIndex) noexcept = 0;
  virtual auto compact() noexcept
      -> futures::Future<ResultT<CompactionResult>> = 0;
  virtual auto getCompactionStatus() const noexcept -> CompactionStatus = 0;
};

struct CompactionManager : ICompactionManager,
                           std::enable_shared_from_this<CompactionManager> {
  explicit CompactionManager(IStorageManager& storage);

  void updateReleaseIndex(LogIndex index) noexcept override;

  void updateLargestIndexToKeep(LogIndex index) noexcept override;

  auto compact() noexcept
      -> futures::Future<ResultT<CompactionResult>> override;

  auto getCompactionStatus() const noexcept -> CompactionStatus override;

 private:
  struct GuardedData {
    explicit GuardedData(IStorageManager& storage);

    void triggerAsyncCompaction();

    LogIndex releaseIndex;
    LogIndex largestIndexToKeep;
    CompactionStatus lastCompaction;
    IStorageManager& storage;
  };
  Guarded<GuardedData> guarded;
};
}  // namespace comp
}  // namespace replication2::replicated_log
}  // namespace arangodb
