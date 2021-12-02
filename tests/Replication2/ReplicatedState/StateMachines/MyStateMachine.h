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
#include <string>
#include <unordered_map>

#include "Replication2/ReplicatedState/ReplicatedState.h"

namespace arangodb::replication2::test {

struct MyFactory;
struct MyEntryType;
struct MyLeaderState;
struct MyFollowerState;

struct MyState {
  using LeaderType = MyLeaderState;
  using FollowerType = MyFollowerState;
  using EntryType = MyEntryType;
  using FactoryType = MyFactory;
};

struct MyEntryType {
  std::string key, value;
};

struct MyStateBase {
  virtual ~MyStateBase() = default;
  std::unordered_map<std::string, std::string> store;

  void applyIterator(TypedLogRangeIterator<streams::StreamEntryView<MyEntryType>>& iter);
};

struct MyLeaderState : MyStateBase, replicated_state::ReplicatedLeaderState<MyState> {
  void set(std::string key, std::string value);
  auto wasRecoveryRun() -> bool { return recoveryRan; }
 protected:
  auto recoverEntries(std::unique_ptr<EntryIterator> ptr) -> futures::Future<Result> override;

  bool recoveryRan = false;
};

struct MyFollowerState : MyStateBase, replicated_state::ReplicatedFollowerState<MyState> {
 protected:
  auto acquireSnapshot(ParticipantId const& destination) noexcept -> futures::Future<Result> override;
  auto applyEntries(std::unique_ptr<EntryIterator> ptr) noexcept -> futures::Future<Result> override;
};

struct MyFactory {
  auto constructFollower() -> std::shared_ptr<MyFollowerState>;
  auto constructLeader() -> std::shared_ptr<MyLeaderState>;
};

}  // namespace arangodb::replication2::test

namespace arangodb::replication2 {
template <>
struct replicated_state::EntryDeserializer<test::MyEntryType> {
  auto operator()(streams::serializer_tag_t<test::MyEntryType>, velocypack::Slice s) const
      -> test::MyEntryType;
};

template <>
struct replicated_state::EntrySerializer<test::MyEntryType> {
  void operator()(streams::serializer_tag_t<test::MyEntryType>,
                  test::MyEntryType const& e, velocypack::Builder& b) const;
};
}  // namespace arangodb::replication2
