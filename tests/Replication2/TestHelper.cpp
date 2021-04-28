#include "TestHelper.h"

using namespace arangodb::replication2;

void DelayedFollowerLog::runAsyncAppendEntries() {
  auto asyncQueue = _asyncQueue.doUnderLock([](auto& _queue) {
    auto queue = std::move(_queue);
    _queue.clear();
    return queue;
  });

  for (auto& p : asyncQueue) {
    p->promise.setValue(std::nullopt);
  }
}

auto DelayedFollowerLog::appendEntries(AppendEntriesRequest req)
    -> arangodb::futures::Future<AppendEntriesResult> {
  auto entry = _asyncQueue.doUnderLock([&](auto& queue) {
    return queue.emplace_back(std::make_shared<AsyncRequest>(std::move(req)));
  });
  return entry->promise.getFuture().thenValue([this, &entry](auto&& result) mutable {
    if (result) {
      return futures::Future<AppendEntriesResult>(std::in_place, *result);
    }
    return ReplicatedLog::appendEntries(std::move(entry->request));
  });
}
