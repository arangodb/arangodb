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

#include "Replication2/ReplicatedState/ReplicatedStateCore.h"
#include "Replication2/ReplicatedState/ReplicatedStateTraits.h"
#include "Replication2/ReplicatedState/StateStatus.h"
#include "Replication2/Streams/Streams.h"

namespace arangodb::futures {
template <typename T>
class Future;
}
namespace arangodb {
class Result;
}
namespace arangodb::replication2 {
namespace replicated_log {
struct ReplicatedLog;
struct ILogFollower;
struct ILogLeader;
}  // namespace replicated_log

namespace replicated_state {

template <typename S>
struct ReplicatedFollowerState;
template <typename S>
struct ReplicatedLeaderState;

struct ReplicatedLeaderStateBase {
  virtual ~ReplicatedLeaderStateBase() = default;
};

struct ReplicatedFollowerStateBase {
  virtual ~ReplicatedFollowerStateBase() = default;
};

/**
 * Common base class for all ReplicatedStates, hiding the type information.
 */
struct ReplicatedStateBase {
  virtual ~ReplicatedStateBase() = default;

  virtual void flush(std::unique_ptr<ReplicatedStateCore>) = 0;
  virtual auto getStatus() -> StateStatus = 0;
  auto getLeader() -> std::shared_ptr<ReplicatedLeaderStateBase> {
    return getLeaderBase();
  }
  auto getFollower() -> std::shared_ptr<ReplicatedFollowerStateBase> {
    return getFollowerBase();
  }

 private:
  virtual auto getLeaderBase() -> std::shared_ptr<ReplicatedLeaderStateBase> = 0;
  virtual auto getFollowerBase() -> std::shared_ptr<ReplicatedFollowerStateBase> = 0;
};

template <typename S>
struct ReplicatedState final : ReplicatedStateBase,
                               std::enable_shared_from_this<ReplicatedState<S>> {
  using Factory = typename ReplicatedStateTraits<S>::FactoryType;
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using FollowerType = typename ReplicatedStateTraits<S>::FollowerType;
  using LeaderType = typename ReplicatedStateTraits<S>::LeaderType;

  explicit ReplicatedState(std::shared_ptr<replicated_log::ReplicatedLog> log,
                           std::shared_ptr<Factory> factory);

  /**
   * Forces to rebuild the state machine depending on the replicated log state.
   */
  void flush(std::unique_ptr<ReplicatedStateCore> = std::make_unique<ReplicatedStateCore>()) override;

  /**
   * Returns the follower state machine. Returns nullptr if no follower state
   * machine is present. (i.e. this server is not a follower)
   */
  auto getFollower() const -> std::shared_ptr<FollowerType>;
  /**
   * Returns the leader state machine. Returns nullptr if no leader state
   * machine is present. (i.e. this server is not a leader)
   */
  auto getLeader() const -> std::shared_ptr<LeaderType>;

  auto getStatus() -> StateStatus final;

 private:
  auto getLeaderBase() -> std::shared_ptr<ReplicatedLeaderStateBase> final {
    return getLeader();
  }
  auto getFollowerBase() -> std::shared_ptr<ReplicatedFollowerStateBase> final {
    return getFollower();
  }

  friend struct ReplicatedFollowerState<S>;
  friend struct ReplicatedLeaderState<S>;

  struct StateBase {
    virtual ~StateBase() = default;
    virtual auto getStatus() -> StateStatus = 0;
  };

  struct LeaderState;
  struct FollowerState;

  void runLeader(std::shared_ptr<replicated_log::ILogLeader> logLeader,
                 std::unique_ptr<ReplicatedStateCore>);
  void runFollower(std::shared_ptr<replicated_log::ILogFollower> logFollower,
                   std::unique_ptr<ReplicatedStateCore>);

  std::shared_ptr<StateBase> currentState;
  StateGeneration generation{0};
  std::shared_ptr<replicated_log::ReplicatedLog> const log{};
  std::shared_ptr<Factory> const factory;
};

template <typename S>
struct ReplicatedLeaderState : ReplicatedLeaderStateBase {
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Stream = streams::ProducerStream<EntryType>;
  using EntryIterator = typename Stream::Iterator;
  virtual ~ReplicatedLeaderState() = default;

 protected:
  /**
   * This function is called once on a leader instance. The iterator contains
   * all log entries currently present in the replicated log. The state machine
   * manager awaits the return value. If the result is ok, the leader instance
   * is made available to the outside world.
   *
   * If the recovery fails, the server aborts.
   * @return Future to be fulfilled when recovery is done.
   */
  virtual auto recoverEntries(std::unique_ptr<EntryIterator>)
      -> futures::Future<Result> = 0;

  auto getStream() const -> std::shared_ptr<Stream> const&;

 private:
  friend struct ReplicatedState<S>::LeaderState;
  std::shared_ptr<Stream> _stream;
};

template <typename S>
struct ReplicatedFollowerState : ReplicatedFollowerStateBase {
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Stream = streams::Stream<EntryType>;
  using EntryIterator = typename Stream::Iterator;

  virtual ~ReplicatedFollowerState() = default;

 protected:
  /**
   * Called by the state machine manager if new log entries have been committed
   * and are ready to be applied to the state machine. The implementation ensures
   * that this function not called again until the future returned is fulfilled.
   *
   * Entries are not released after they are consumed by this function. Its the
   * state machines implementations responsibility to call release on the stream.
   *
   * @return Future with Result value. If the result contains an error, the
   *    operation is retried.
   */
  virtual auto applyEntries(std::unique_ptr<EntryIterator>) noexcept
      -> futures::Future<Result> = 0;

  /**
   * Called by the state machine manager if a follower is requested to pull
   * data from the leader in order to transfer the snapshot.
   * @param leader
   * @param localCommitIndex
   * @return Future with Result value. If the result contains an error,
   *    the operation is eventually retried.
   */
  virtual auto acquireSnapshot(ParticipantId const& leader, LogIndex localCommitIndex) noexcept
      -> futures::Future<Result> = 0;

  [[nodiscard]] auto getStream() const -> std::shared_ptr<Stream> const&;

 private:
  friend struct ReplicatedState<S>::FollowerState;
  std::shared_ptr<Stream> _stream;
};

template <typename S>
using ReplicatedStateStreamSpec = streams::stream_descriptor_set<streams::stream_descriptor<
    streams::StreamId{1}, typename ReplicatedStateTraits<S>::EntryType,
    streams::tag_descriptor_set<streams::tag_descriptor<1, typename ReplicatedStateTraits<S>::Deserializer, typename ReplicatedStateTraits<S>::Serializer>>>>;
}  // namespace replicated_state
}  // namespace arangodb::replication2
