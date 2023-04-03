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

#include "Basics/Guarded.h"
#include "Replication2/ReplicatedLog/Components/IMessageIdManager.h"
#include "Replication2/ReplicatedLog/NetworkMessages.h"

namespace arangodb {
namespace replication2 {
namespace replicated_log {

inline namespace comp {

struct AppendEntriesMessageIdAcceptor {
  auto accept(MessageId) noexcept -> bool;
  auto get() const noexcept -> MessageId;

 private:
  MessageId lastId{0};
};

struct MessageIdManager : IMessageIdManager {
  [[nodiscard]] auto acceptReceivedMessageId(MessageId id) noexcept
      -> bool override;
  [[nodiscard]] auto getLastReceivedMessageId() const noexcept
      -> MessageId override;

 private:
  Guarded<AppendEntriesMessageIdAcceptor> messageIdAcceptor;
};
}  // namespace comp

}  // namespace replicated_log
}  // namespace replication2
}  // namespace arangodb
