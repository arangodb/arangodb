////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <list>
#include <memory>

#include "Replication2/ReplicatedLog/Components/LogFollower.h"
#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Replication2/Storage/IStorageEngineMethods.h"

#include "Replication2/Mocks/SchedulerMocks.h"
#include "Basics/SourceLocation.h"

namespace arangodb::replication2::test {

// Queues appendEntries calls on its own DelayedScheduler. Keeps a queue of
// pending requests to inspect.
// It also serves as a proxy to the follower instance: The internal follower can
// be replaced without the leader noticing, like a reinstantiation on the other
// DBServer would.
// When requests in the queue are to be delivered, the current follower is
// looked up and used, rather than the follower being saved on the appendEntries
// call.
struct DelayedLogFollower : replicated_log::ILogFollower {
  explicit DelayedLogFollower(
      std::shared_ptr<replicated_log::ILogFollower> follower);
  // Instantiate this without a follower.
  // Just don't do anything with it except calling appendEntries and
  // getParticipantId().
  explicit DelayedLogFollower(ParticipantId _participantId);

  void replaceFollowerWith(
      std::shared_ptr<replicated_log::ILogFollower> follower);

  void swapFollowerAndQueueWith(DelayedLogFollower& other);

  auto appendEntries(replicated_log::AppendEntriesRequest req)
      -> arangodb::futures::Future<
          replicated_log::AppendEntriesResult> override;

  // TODO rename to sth like deliver-delayed-append-entries-requests or so
  auto runAsyncAppendEntries() -> std::size_t;

  void runAllAsyncAppendEntries();

  using WaitForAsyncPromise =
      futures::Promise<replicated_log::AppendEntriesRequest>;

  struct AsyncRequest {
    explicit AsyncRequest(replicated_log::AppendEntriesRequest request)
        : request(std::move(request)) {}
    replicated_log::AppendEntriesRequest request;
    WaitForAsyncPromise promise;
  };
  [[nodiscard]] auto pendingAppendEntries() const
      -> std::list<std::shared_ptr<AsyncRequest>> const&;
  [[nodiscard]] auto hasPendingAppendEntries() const -> bool;
  [[nodiscard]] auto currentRequest() const
      -> replicated_log::AppendEntriesRequest const& {
    TRI_ASSERT(_asyncQueue.size() == 1);
    return _asyncQueue.front()->request;
  }

  auto getParticipantId() const noexcept -> ParticipantId const& override {
    if (_follower != nullptr) {
      TRI_ASSERT(follower().getParticipantId() == _participantId);
    }
    return _participantId;
  }

  auto getStatus() const -> replicated_log::LogStatus override {
    return follower().getStatus();
  }

  auto getQuickStatus() const -> replicated_log::QuickLogStatus override {
    return follower().getQuickStatus();
  }

  [[nodiscard]] auto resign() && -> std::tuple<
      std::unique_ptr<storage::IStorageEngineMethods>,
      std::unique_ptr<replicated_log::IReplicatedStateHandle>,
      DeferredAction> override {
    return std::move(*_follower).resign();
  }

  auto waitFor(LogIndex index) -> WaitForFuture override {
    return follower().waitFor(index);
  }

  auto waitForIterator(LogIndex index) -> WaitForIteratorFuture override {
    return follower().waitForIterator(index);
  }

  auto compact() -> ResultT<replicated_log::CompactionResult> override {
    return follower().compact();
  }

  auto getInternalLogIterator(std::optional<LogRange> bounds) const
      -> std::unique_ptr<LogIterator> override {
    return follower().getInternalLogIterator(bounds);
  }

  DelayedScheduler scheduler;

 private:
  auto follower() -> replicated_log::ILogFollower& {
    TRI_ASSERT(_follower != nullptr)
        << "Accessing follower before it has been installed.";
    return *_follower;
  }
  auto follower() const -> replicated_log::ILogFollower const& {
    TRI_ASSERT(_follower != nullptr)
        << "Accessing follower before it has been installed.";
    return *_follower;
  }

  ParticipantId const _participantId;
  std::list<std::shared_ptr<AsyncRequest>> _asyncQueue;
  std::shared_ptr<replicated_log::ILogFollower> _follower;
};

}  // namespace arangodb::replication2::test
