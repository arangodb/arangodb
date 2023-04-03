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
#include <string_view>

namespace arangodb {
class Result;
}
namespace arangodb::replication2::replicated_log {
struct MessageId;
inline namespace comp {
enum class SnapshotState { MISSING, AVAILABLE };
auto to_string(SnapshotState) noexcept -> std::string_view;

struct ISnapshotManager {
  virtual ~ISnapshotManager() = default;
  [[nodiscard]] virtual auto invalidateSnapshotState() -> Result = 0;
  [[nodiscard]] virtual auto checkSnapshotState() const noexcept
      -> SnapshotState = 0;
  [[nodiscard]] virtual auto setSnapshotStateAvailable(MessageId msgId,
                                                       std::uint64_t version)
      -> Result = 0;
};
}  // namespace comp
}  // namespace arangodb::replication2::replicated_log
