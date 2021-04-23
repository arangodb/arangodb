
#ifndef ARANGODB3_TESTHELPER_H
#define ARANGODB3_TESTHELPER_H

#include "Replication2/ReplicatedLog.h"

namespace arangodb::replication2 {
struct DelayedFollowerLog : ReplicatedLog {
  using ReplicatedLog::ReplicatedLog;
  auto appendEntries(AppendEntriesRequest)
      -> arangodb::futures::Future<AppendEntriesResult> override;
  void runAsyncAppendEntries();

  using WaitForAsyncPromise = futures::Promise<std::optional<AppendEntriesResult>>;

  struct AsyncRequest {
    explicit AsyncRequest(AppendEntriesRequest request)
        : request(std::move(request)) {}
    AppendEntriesRequest request;
    WaitForAsyncPromise promise;
  };
  [[nodiscard]] auto pendingAppendEntries() const -> std::vector<AsyncRequest> const& { return _asyncQueue; }
  [[nodiscard]] auto hasPendingAppendEntries() const -> bool { return !_asyncQueue.empty(); }

 private:
  std::vector<AsyncRequest> _asyncQueue;
};
}

#endif  // ARANGODB3_TESTHELPER_H
