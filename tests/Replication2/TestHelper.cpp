#include "TestHelper.h"

using namespace arangodb::replication2;

void DelayedFollowerLog::runAsyncAppendEntries() {
  auto asyncQueue = _asyncQueue.doUnderLock([](auto& _queue) {
    auto queue = std::move(_queue);
    _queue.clear();
    return queue;
  });

  for (auto& p : asyncQueue) {
    p->promise.setValue(std::move(p->request));
  }
}

auto DelayedFollowerLog::appendEntries(AppendEntriesRequest req)
    -> arangodb::futures::Future<AppendEntriesResult> {
  auto future = _asyncQueue.doUnderLock([&](auto& queue) {
    return queue.emplace_back(std::make_shared<AsyncRequest>(std::move(req)))->promise.getFuture();
  });
  return std::move(future).thenValue([this](auto&& result) mutable {
    if (!result.has_value()) {
      return futures::Future<AppendEntriesResult>{AppendEntriesResult{false}};
    }
    return ReplicatedLog::appendEntries(std::move(result).value());
  });
}
