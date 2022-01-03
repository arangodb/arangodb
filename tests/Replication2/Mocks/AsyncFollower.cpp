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
#include "AsyncFollower.h"

#include <utility>

#include "Replication2/ReplicatedLog/ILogParticipant.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogStatus.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

auto AsyncFollower::getStatus() const -> LogStatus {
  return _follower->getStatus();
}

auto AsyncFollower::resign() && -> std::tuple<std::unique_ptr<LogCore>,
                                              DeferredAction> {
  return std::move(*_follower).resign();
}

auto AsyncFollower::waitFor(arangodb::replication2::LogIndex index)
    -> WaitForFuture {
  return _follower->waitFor(index);
}

auto AsyncFollower::release(arangodb::replication2::LogIndex doneWithIdx)
    -> Result {
  return _follower->release(doneWithIdx);
}

auto AsyncFollower::getParticipantId() const noexcept -> ParticipantId const& {
  return _follower->getParticipantId();
}

auto AsyncFollower::appendEntries(AppendEntriesRequest request)
    -> futures::Future<AppendEntriesResult> {
  std::unique_lock guard(_mutex);
  _cv.notify_all();
  return _requests.emplace_back(std::move(request)).promise.getFuture();
}

AsyncFollower::AsyncFollower(
    std::shared_ptr<replicated_log::LogFollower> follower)
    : _follower(std::move(follower)),
      _asyncWorker([this] { this->runWorker(); }) {}

AsyncFollower::~AsyncFollower() noexcept {
  if (!_stopping) {
    stop();
  }
}

void AsyncFollower::runWorker() {
  while (true) {
    std::vector<AsyncRequest> requests;
    {
      std::unique_lock guard(_mutex);
      if (_stopping) {
        break;
      }
      if (!_requests.empty()) {
        std::swap(requests, _requests);
      } else {
        _cv.wait(guard);
      }
    }

    for (auto& req : requests) {
      _follower->appendEntries(req.request)
          .thenFinal([promise = std::move(req.promise)](auto&& res) mutable {
            promise.setValue(std::forward<decltype(res)>(res));
          });
    }
  }
}

void AsyncFollower::stop() noexcept {
  {
    std::unique_lock guard(_mutex);
    _stopping = true;
    _cv.notify_all();
  }

  TRI_ASSERT(_asyncWorker.joinable());
  _asyncWorker.join();
}

AsyncFollower::AsyncRequest::AsyncRequest(AppendEntriesRequest request)
    : request(std::move(request)) {}
