////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "Activities/Activity.h"

namespace arangodb::activities {

auto Activity::parentId() const noexcept -> std::optional<ActivityId> {
  if (_parent == nullptr) {
    return std::nullopt;
  } else {
    return _parent->id();
  }
}

auto Activity::threads() const -> std::vector<basics::ThreadInfo> {
  return _threads.doUnderLock([](ThreadList const& list) {
    std::vector<basics::ThreadInfo> out;
    for (auto* n = list.head; n != nullptr; n = n->next) {
      auto id = n->threadId;
      out.emplace_back(id.kernel_id, id.name());
    }
    return out;
  });
}

// Attach a ThreadNode at the end of _thread
auto Activity::link(ThreadNode& node) noexcept -> void {
  _threads.doUnderLock([&node](ThreadList& list) {
    node.prev = list.tail;
    node.next = nullptr;
    (list.tail != nullptr ? list.tail->next : list.head) = &node;
    list.tail = &node;
  });
}

// Remove the ThreadNode from the linked list of _thread
auto Activity::unlink(ThreadNode& node) noexcept -> void {
  _threads.doUnderLock([&node](ThreadList& list) {
    (node.prev != nullptr ? node.prev->next : list.head) = node.next;
    (node.next != nullptr ? node.next->prev : list.tail) = node.prev;
    node.prev = node.next = nullptr;
  });
}

}  // namespace arangodb::activities
