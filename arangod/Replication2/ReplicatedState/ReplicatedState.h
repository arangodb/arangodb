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

#include <gtest/gtest.h>


#include "Replication2/ReplicatedState/ReplicatedStateTraits.h"
#include "Replication2/Streams/Streams.h"

namespace arangodb::futures {
template<typename T>
class Future;
}
namespace arangodb {
class Result;
}
namespace arangodb::basics {

template <typename Char>
struct TransparentBasicStringHash {
  using is_transparent = void;
  using hash_type = std::hash<std::basic_string_view<Char>>;
  auto operator()(std::basic_string<Char> const& str) const noexcept {
    return hash_type{}(str);
  }
  auto operator()(std::basic_string_view<Char> const& str) const noexcept {
    return hash_type{}(str);
  }
  auto operator()(const Char* str) const noexcept { return hash_type{}(str); }
};

using TransparentStringHash = TransparentBasicStringHash<char>;

}  // namespace arangodb::basics

namespace arangodb::replication2::replicated_log {
struct ReplicatedLog;
class LogFollower;
class LogLeader;
}  // namespace arangodb::replication2::replicated_log

namespace arangodb::replication2::replicated_state {

template <typename S>
struct ReplicatedLeaderState;

template <typename S>
struct ReplicatedFollowerState;

struct ReplicatedStateBase {
  virtual ~ReplicatedStateBase() = default;
};

template <typename S, typename Factory = typename ReplicatedStateTraits<S>::FactoryType>
struct ReplicatedState : ReplicatedStateBase,
                         std::enable_shared_from_this<ReplicatedState<S, Factory>> {
  explicit ReplicatedState(std::shared_ptr<replicated_log::ReplicatedLog> log,
                           std::shared_ptr<Factory> factory);

  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using FollowerType = typename ReplicatedStateTraits<S>::FollowerType;
  using LeaderType = typename ReplicatedStateTraits<S>::LeaderType;

  void flush();

  auto getFollower() const -> std::shared_ptr<FollowerType>;

  auto getLeader() const -> std::shared_ptr<LeaderType>;

 private:
  friend struct ReplicatedFollowerState<S>;
  friend struct ReplicatedLeaderState<S>;

  struct StateBase {
    virtual ~StateBase() = default;
  };

  struct LeaderState;

  void runLeader(std::shared_ptr<replicated_log::LogLeader> logLeader);

  struct FollowerState;

  void runFollower(std::shared_ptr<replicated_log::LogFollower> logFollower);

  std::shared_ptr<StateBase> currentState;
  std::shared_ptr<replicated_log::ReplicatedLog> log{};
  std::shared_ptr<Factory> factory;
};

template <typename S>
struct ReplicatedLeaderState {
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Stream = streams::ProducerStream<EntryType>;
  using EntryIterator = typename Stream::Iterator;
  virtual ~ReplicatedLeaderState() = default;

 protected:
  friend struct ReplicatedState<S>::LeaderState;
  virtual auto installSnapshot(ParticipantId const& destination)
      -> futures::Future<Result> = 0;
  virtual auto recoverEntries(std::unique_ptr<EntryIterator>)
      -> futures::Future<Result> = 0;

  auto getStream() const -> std::shared_ptr<Stream>;

 private:
  std::shared_ptr<Stream> _stream;
};

template <typename S>
struct ReplicatedFollowerState {
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Stream = streams::Stream<EntryType>;
  using EntryIterator = typename Stream::Iterator;

  virtual ~ReplicatedFollowerState() = default;

 protected:
  friend struct ReplicatedState<S>::FollowerState;
  virtual auto applyEntries(std::unique_ptr<EntryIterator>)
      -> futures::Future<Result> = 0;

  [[nodiscard]] auto getStream() const -> std::shared_ptr<Stream>;

 private:
  std::shared_ptr<Stream> _stream;
};

template <typename S>
using ReplicatedStateStreamSpec = streams::stream_descriptor_set<streams::stream_descriptor<
    streams::StreamId{1}, typename ReplicatedStateTraits<S>::EntryType,
    streams::tag_descriptor_set<streams::tag_descriptor<1, typename ReplicatedStateTraits<S>::Deserializer, typename ReplicatedStateTraits<S>::Serializer>>>>;
}  // namespace arangodb::replication2::replicated_state
