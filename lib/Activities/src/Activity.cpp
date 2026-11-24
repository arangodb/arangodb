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
      auto id = n->thread;
      out.emplace_back(id.kernel_id, id.name());
    }
    return out;
  });
}

auto Activity::link(ThreadNode& node) noexcept -> void {
  _threads.doUnderLock([&node](ThreadList& list) {
    node.prev = list.tail;
    node.next = nullptr;
    (list.tail != nullptr ? list.tail->next : list.head) = &node;
    list.tail = &node;
  });
}

auto Activity::unlink(ThreadNode& node) noexcept -> void {
  _threads.doUnderLock([&node](ThreadList& list) {
    (node.prev != nullptr ? node.prev->next : list.head) = node.next;
    (node.next != nullptr ? node.next->prev : list.tail) = node.prev;
    node.prev = node.next = nullptr;
  });
}

}  // namespace arangodb::activities
