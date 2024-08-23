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

#include "DelayedLogFollower.h"

namespace arangodb::replication2::test {

DelayedLogFollower::DelayedLogFollower(
    std::shared_ptr<replicated_log::ILogFollower> follower)
    : _participantId(follower->getParticipantId()),
      _follower(std::move(follower)) {
  // Let's not accidentally wrap a DelayedLogFollower in another
  ADB_PROD_ASSERT(std::dynamic_pointer_cast<DelayedLogFollower>(follower) ==
                  nullptr);
}

DelayedLogFollower::DelayedLogFollower(ParticipantId participantId)
    : _participantId(std::move(participantId)), _follower(nullptr) {}

void DelayedLogFollower::replaceFollowerWith(
    std::shared_ptr<replicated_log::ILogFollower> follower) {
  TRI_ASSERT(!scheduler.hasWork())
      << "You should empty the DelayedFollower's scheduler before replacing "
         "its follower.";
  _follower = std::move(follower);
  TRI_ASSERT(_follower->getParticipantId() == _participantId)
      << "Trying to replace the follower " << _participantId
      << " with an instance of " << _follower->getParticipantId();
}

void DelayedLogFollower::swapFollowerAndQueueWith(DelayedLogFollower& other) {
  std::swap(_follower, other._follower);
  std::swap(_asyncQueue, other._asyncQueue);
  std::swap(scheduler, other.scheduler);
}

auto DelayedLogFollower::appendEntries(replicated_log::AppendEntriesRequest req)
    -> arangodb::futures::Future<replicated_log::AppendEntriesResult> {
  auto p =
      _asyncQueue.emplace_back(std::make_shared<AsyncRequest>(std::move(req)));

  scheduler.queue([p, this]() {
    p->promise.setValue(std::move(p->request));
    auto it = std::find(_asyncQueue.begin(), _asyncQueue.end(), p);
    if (it != _asyncQueue.end()) {
      _asyncQueue.erase(it);
    }
  });

  return p->promise.getFuture().thenValue([this](auto&& result) mutable {
    return _follower->appendEntries(std::forward<decltype(result)>(result));
  });
}

auto DelayedLogFollower::runAsyncAppendEntries() -> std::size_t {
  return scheduler.runAllCurrent();
}

void DelayedLogFollower::runAllAsyncAppendEntries() {
  while (hasPendingAppendEntries()) {
    runAsyncAppendEntries();
  }
}

auto DelayedLogFollower::pendingAppendEntries() const
    -> std::list<std::shared_ptr<AsyncRequest>> const& {
  return _asyncQueue;
}

auto DelayedLogFollower::hasPendingAppendEntries() const -> bool {
  return scheduler.hasWork();
}

}  // namespace arangodb::replication2::test
