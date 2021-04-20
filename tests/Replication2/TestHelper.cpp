#include "TestHelper.h"

using namespace arangodb::replication2;

void DelayedFollowerLog::runAsyncAppendEntries() {
  auto asyncQueue = std::move(_asyncQueue);
  _asyncQueue.clear();

  for (auto& p : asyncQueue) {
    p.promise.setValue(std::nullopt);
  }
}

auto DelayedFollowerLog::appendEntries(AppendEntriesRequest req)
    -> arangodb::futures::Future<AppendEntriesResult> {
  auto& entry = _asyncQueue.emplace_back(std::move(req));
  return entry.promise.getFuture().thenValue([this, &entry](auto&& result) mutable {
    if (result) {
      return futures::Future<AppendEntriesResult>(std::in_place, *result);
    }
    return InMemoryLog::appendEntries(std::move(entry.request));
  });
}
