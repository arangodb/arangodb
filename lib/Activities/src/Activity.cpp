#include "Activities/Activity.h"

namespace arangodb::activities {

auto Activity::parentId() const noexcept -> std::optional<ActivityId> {
  if (_parent == nullptr) {
    return std::nullopt;
  } else {
    return _parent->id();
  }
}

auto Activity::threads() const noexcept -> std::vector<basics::ThreadInfo> {
  auto threads = _threads.copy();
  std::vector<basics::ThreadInfo> out;
  for (auto const& thread : threads) {
    out.emplace_back(thread.get_ref().value());
  }
  return out;
}

auto Activity::addCurrentThread() -> ThreadList::iterator {
  return _threads.doUnderLock([](auto& threads) {
    threads.push_back({basics::ThreadInfo::current()});
    return std::prev(threads.end());
  });
}

auto Activity::removeThread(ThreadList::iterator it) -> void {
  _threads.doUnderLock([it](auto& threads) { threads.erase(it); });
}

}  // namespace arangodb::activities
