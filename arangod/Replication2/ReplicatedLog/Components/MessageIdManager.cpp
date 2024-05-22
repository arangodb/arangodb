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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "MessageIdManager.h"

namespace arangodb {
namespace replication2 {
namespace replicated_log {

auto AppendEntriesMessageIdAcceptor::accept(MessageId id) noexcept -> bool {
  bool const isNew = id > lastId;
  if (isNew) {
    lastId = id;
  }

  return isNew;
}

auto AppendEntriesMessageIdAcceptor::get() const noexcept -> MessageId {
  return lastId;
}

auto MessageIdManager::getLastReceivedMessageId() const noexcept -> MessageId {
  return messageIdAcceptor.getLockedGuard()->get();
}

bool MessageIdManager::acceptReceivedMessageId(MessageId id) noexcept {
  return messageIdAcceptor.getLockedGuard()->accept(id);
}

}  // namespace replicated_log
}  // namespace replication2
}  // namespace arangodb