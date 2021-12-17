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

#pragma once

#include "Futures/Future.h"

#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/StateInterfaces.h"
#include "Replication2/Streams/Streams.h"

namespace arangodb::replication2 {
namespace test {

/**
 * Empty follower. Always immediately responds with success.
 * @tparam S state description
 */
template<typename S>
struct EmptyFollowerType : replicated_state::IReplicatedFollowerState<S> {
  using EntryIterator =
      typename replicated_state::IReplicatedFollowerState<S>::EntryIterator;

 protected:
  auto applyEntries(std::unique_ptr<EntryIterator>) noexcept
      -> futures::Future<Result> override {
    return futures::Future<Result>{std::in_place};
  }
  auto acquireSnapshot(ParticipantId const&, LogIndex) noexcept
      -> futures::Future<Result> override {
    return futures::Future<Result>{std::in_place};
  }
};

/**
 * Empty leader. Always immediately responds with success.
 * @tparam S
 */
template<typename S>
struct EmptyLeaderType : replicated_state::IReplicatedLeaderState<S> {
  using EntryIterator =
      typename replicated_state::IReplicatedLeaderState<S>::EntryIterator;

 protected:
  auto recoverEntries(std::unique_ptr<EntryIterator> ptr)
      -> futures::Future<Result> override {
    return futures::Future<Result>(std::in_place);
  }
};

template<typename Input, typename Result>
struct AsyncOperationMarker {
  auto trigger(Input inValue) -> futures::Future<Result> {
    TRI_ASSERT(in.has_value() == false);
    in.emplace(std::move(inValue));
    return promise.emplace().getFuture();
  }

  auto resolveWith(Result res) {
    TRI_ASSERT(triggered);
    TRI_ASSERT(!promise->isFulfilled());
    promise->setValue(std::move(res));
  }

  auto inspectValue() const -> Input const& { return *in; }

  [[nodiscard]] auto wasTriggered() const noexcept -> bool { return triggered; }

  void reset() {
    TRI_ASSERT(triggered);
    TRI_ASSERT(promise->isFulfilled());
    triggered = false;
    promise.reset();
  }

 private:
  bool triggered = false;
  std::optional<Input> in;
  std::optional<futures::Promise<Result>> promise;
};

template<typename S>
struct FakeLeaderType : replicated_state::IReplicatedLeaderState<S> {
  using EntryIterator =
      typename replicated_state::IReplicatedLeaderState<S>::EntryIterator;

  [[nodiscard]] auto hasReceivedRecovery() const noexcept -> bool {
    return recovery.wasTriggered();
  }

  void resolveRecoveryOk() noexcept { resolveRecovery(Result{}); }

  void resolveRecovery(Result res) noexcept {
    recovery.resolveWith(std::move(res));
  }

  AsyncOperationMarker<std::unique_ptr<EntryIterator>, Result> recovery;
 protected:
  auto recoverEntries(std::unique_ptr<EntryIterator> iter)
      -> futures::Future<Result> override {
    return recovery.trigger(std::move(iter));
  }
};

template<typename S>
struct FakeFollowerType : replicated_state::IReplicatedFollowerState<S> {
  using EntryIterator =
      typename replicated_state::IReplicatedFollowerState<S>::EntryIterator;

  AsyncOperationMarker<std::unique_ptr<EntryIterator>, Result> apply;
  AsyncOperationMarker<std::pair<ParticipantId, LogIndex>, Result> acquire;
 protected:
  auto applyEntries(std::unique_ptr<EntryIterator> ptr) noexcept
      -> futures::Future<Result> override {
    return apply.trigger(std::move(ptr));
  }
  auto acquireSnapshot(const ParticipantId& leader,
                       LogIndex localCommitIndex) noexcept
      -> futures::Future<Result> override {
    return acquire.trigger(std::make_pair(leader, localCommitIndex));
  }
};

/**
 * DefaultFactory simply makes the LeaderType or FollowerType shared.
 * @tparam LeaderType
 * @tparam FollowerType
 */
template<typename LeaderType, typename FollowerType>
struct DefaultFactory {
  auto constructLeader() -> std::shared_ptr<LeaderType> {
    return std::make_shared<LeaderType>();
  }
  auto constructFollower() -> std::shared_ptr<FollowerType> {
    return std::make_shared<FollowerType>();
  }
};

/**
 * DefaultEntryType contains a key and value. Should be enough for most tests.
 */
struct DefaultEntryType {
  std::string key, value;
};
}  // namespace test

template<>
struct replicated_state::EntryDeserializer<test::DefaultEntryType> {
  auto operator()(streams::serializer_tag_t<test::DefaultEntryType>,
                  velocypack::Slice s) const -> test::DefaultEntryType;
};

template<>
struct replicated_state::EntrySerializer<test::DefaultEntryType> {
  void operator()(streams::serializer_tag_t<test::DefaultEntryType>,
                  test::DefaultEntryType const& e,
                  velocypack::Builder& b) const;
};
}  // namespace arangodb::replication2
