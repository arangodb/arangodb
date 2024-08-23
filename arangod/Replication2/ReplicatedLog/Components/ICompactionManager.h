////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Basics/ResultError.h"

namespace arangodb {
namespace futures {
template<typename T>
class Future;
}  // namespace futures

namespace replication2::replicated_log {
struct CompactionStatus;
inline namespace comp {

struct ICompactionManager {
  struct CompactResult {
    std::optional<result::Error> error;
    CompactionStopReason stopReason;
    LogRange compactedRange;
  };

  virtual ~ICompactionManager() = default;
  virtual void updateReleaseIndex(LogIndex) noexcept = 0;
  virtual void updateLowestIndexToKeep(LogIndex) noexcept = 0;
  virtual auto compact() noexcept -> futures::Future<CompactResult> = 0;
  [[nodiscard]] virtual auto getCompactionStatus() const noexcept
      -> CompactionStatus = 0;
};

}  // namespace comp
}  // namespace replication2::replicated_log
}  // namespace arangodb
