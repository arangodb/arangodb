////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/ReplicatedLog/LogStatus.h"

#include "AsyncLeader.h"

#include <utility>

using namespace arangodb;
using namespace replication2;
using namespace replication2::replicated_log;

auto test::AsyncLeader::getStatus() const -> LogStatus {
  return _leader->getStatus();
}
auto test::AsyncLeader::resign() && -> std::tuple<
    std::unique_ptr<replicated_log::LogCore>, DeferredAction> {
  return std::move(*_leader).resign();
}
auto test::AsyncLeader::waitFor(LogIndex index) -> WaitForFuture {
  return resolveFutureAsync(_leader->waitFor(index));
}
auto test::AsyncLeader::waitForIterator(replication2::LogIndex index)
    -> WaitForIteratorFuture {
  return resolveFutureAsync(_leader->waitForIterator(index));
}
replication2::LogIndex test::AsyncLeader::getCommitIndex() const noexcept {
  return _leader->getCommitIndex();
}
Result test::AsyncLeader::release(replication2::LogIndex doneWithIdx) {
  return _leader->release(doneWithIdx);
}
replication2::LogIndex test::AsyncLeader::insert(
    replication2::LogPayload payload, bool waitForSync) {
  return _leader->insert(payload, waitForSync);
}
replication2::LogIndex test::AsyncLeader::insert(
    replication2::LogPayload payload, bool waitForSync,
    replication2::replicated_log::ILogLeader::DoNotTriggerAsyncReplication) {
  return _leader->insert(payload, waitForSync, doNotTriggerAsyncReplication);
}

void test::AsyncLeader::triggerAsyncReplication() {
  _leader->triggerAsyncReplication();
}

bool test::AsyncLeader::isLeadershipEstablished() const noexcept {
  return _leader->isLeadershipEstablished();
}
replication2::replicated_log::ILogParticipant::WaitForFuture
test::AsyncLeader::waitForLeadership() {
  return resolveFutureAsync(_leader->waitForLeadership());
}
replication2::replicated_log::InMemoryLog test::AsyncLeader::copyInMemoryLog()
    const {
  return _leader->copyInMemoryLog();
}

test::AsyncLeader::AsyncLeader(
    std::shared_ptr<replicated_log::ILogLeader> _leader)
    : _leader(std::move(_leader)),
      _asyncResolver([this] { this->runWorker(); }) {}

template<typename T>
auto test::AsyncLeader::resolveFutureAsync(futures::Future<T> f)
    -> futures::Future<T> {
  futures::Promise<T> promise;
  auto future = promise.getFuture();
  std::move(f).thenFinal(
      [self = shared_from_this(), promise = std::move(promise)](
          futures::Try<T>&& result) mutable noexcept {
        self->resolvePromiseAsync(std::move(promise), std::move(result));
      });

  return future;
}

template<typename T>
void test::AsyncLeader::resolvePromiseAsync(futures::Promise<T> p,
                                            futures::Try<T> t) noexcept {
  struct PromiseResolveAction : Action {
    PromiseResolveAction(futures::Promise<T> p, futures::Try<T> t)
        : p(std::move(p)), t(std::move(t)) {}
    void execute() noexcept override { p.setTry(std::move(t)); }
    futures::Promise<T> p;
    futures::Try<T> t;
  };

  std::unique_lock guard(_mutex);
  _queue.emplace_back(
      std::make_unique<PromiseResolveAction>(std::move(p), std::move(t)));
  _cv.notify_all();
}

void test::AsyncLeader::runWorker() {
  while (true) {
    std::vector<std::unique_ptr<Action>> actions;
    {
      std::unique_lock guard(_mutex);
      if (_stopping) {
        break;
      }
      if (!_queue.empty()) {
        std::swap(actions, _queue);
      } else {
        _cv.wait(guard);
      }
    }

    for (auto& action : actions) {
      action->execute();
    }
  }
}

void test::AsyncLeader::stop() noexcept {
  {
    std::unique_lock guard(_mutex);
    _stopping = true;
    _cv.notify_all();
  }

  TRI_ASSERT(_asyncResolver.joinable());
  _asyncResolver.join();
}

test::AsyncLeader::~AsyncLeader() {
  if (!_stopping) {
    stop();
  }
}
