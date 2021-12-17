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
template<typename T>
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

struct IReplicatedLeaderStateBase {
  virtual ~IReplicatedLeaderStateBase() = default;
};

struct IReplicatedFollowerStateBase {
  virtual ~IReplicatedFollowerStateBase() = default;
};

template<typename S>
struct IReplicatedLeaderState : IReplicatedLeaderStateBase {
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Stream = streams::ProducerStream<EntryType>;
  using EntryIterator = typename Stream::Iterator;

  // TODO make functions protected
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

  // TODO make private
  std::shared_ptr<Stream> _stream;
};

template<typename S>
struct IReplicatedFollowerState : IReplicatedFollowerStateBase {
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Stream = streams::Stream<EntryType>;
  using EntryIterator = typename Stream::Iterator;

  // TODO make functions protected
  /**
   * Called by the state machine manager if new log entries have been committed
   * and are ready to be applied to the state machine. The implementation
   * ensures that this function not called again until the future returned is
   * fulfilled.
   *
   * Entries are not released after they are consumed by this function. Its the
   * state machines implementations responsibility to call release on the
   * stream.
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
  virtual auto acquireSnapshot(ParticipantId const& leader,
                               LogIndex localCommitIndex) noexcept
      -> futures::Future<Result> = 0;

  [[nodiscard]] auto getStream() const -> std::shared_ptr<Stream> const&;

  // TODO make private
  std::shared_ptr<Stream> _stream;
};
}  // namespace replicated_state
}  // namespace arangodb::replication2
