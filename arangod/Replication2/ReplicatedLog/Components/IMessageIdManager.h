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

namespace arangodb {
namespace replication2 {
namespace replicated_log {

struct MessageId;

inline namespace comp {
struct IMessageIdManager {
  virtual ~IMessageIdManager() = default;

  [[nodiscard]] virtual auto acceptReceivedMessageId(MessageId) noexcept
      -> bool = 0;
  [[nodiscard]] virtual auto getLastReceivedMessageId() const noexcept
      -> MessageId = 0;
};

}  // namespace comp

}  // namespace replicated_log
}  // namespace replication2
}  // namespace arangodb
