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

#include "MyStateMachine.h"

#include <velocypack/Slice.h>

#include <Basics/Result.h>
#include <Basics/voc-errors.h>
#include <Futures/Future.h>
#include <Logger/LogMacros.h>

#include "Replication2/ReplicatedState/ReplicatedStateTraits.h"
#include "Replication2/Streams/Streams.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::test;

void MyStateBase::applyIterator(
    TypedLogRangeIterator<streams::StreamEntryView<MyEntryType>>& iter) {
  while (auto entry = iter.next()) {
    auto& [idx, modification] = *entry;
    store[modification.key] = modification.value;
  }
}

void MyLeaderState::set(std::string key, std::string value) {
  auto entry = MyEntryType{key, value};
  auto idx = getStream()->insert(entry);
  getStream()->waitFor(idx).thenValue(
      [this, key, value](auto&& res) { store[key] = value; });
}

auto MyFollowerState::acquireSnapshot(ParticipantId const& destination) noexcept
    -> futures::Future<Result> {
  return futures::Future<Result>{TRI_ERROR_NO_ERROR};
}

auto MyLeaderState::recoverEntries(std::unique_ptr<EntryIterator> ptr)
    -> futures::Future<Result> {
  applyIterator(*ptr);
  recoveryRan = true;
  return futures::Future<Result>{TRI_ERROR_NO_ERROR};
}

auto MyFactory::constructLeader() -> std::shared_ptr<MyLeaderState> {
  return std::make_shared<MyLeaderState>();
}

auto MyFactory::constructFollower() -> std::shared_ptr<MyFollowerState> {
  return std::make_shared<MyFollowerState>();
}

auto MyFollowerState::applyEntries(std::unique_ptr<EntryIterator> ptr) noexcept
    -> futures::Future<Result> {
  applyIterator(*ptr);
  getStream()->release(ptr->range().to.saturatedDecrement());
  return futures::Future<Result>{TRI_ERROR_NO_ERROR};
}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"

template struct replicated_state::ReplicatedState<MyState>;
template struct streams::LogMultiplexer<
    replicated_state::ReplicatedStateStreamSpec<MyState>>;

auto replicated_state::EntryDeserializer<MyEntryType>::operator()(
    streams::serializer_tag_t<MyEntryType>, velocypack::Slice s) const
    -> MyEntryType {
  auto key = s.get("key").copyString();
  auto value = s.get("value").copyString();
  return MyEntryType{.key = key, .value = value};
}

void replicated_state::EntrySerializer<MyEntryType>::operator()(
    streams::serializer_tag_t<MyEntryType>, MyEntryType const& e,
    velocypack::Builder& b) const {
  velocypack::ObjectBuilder ob(&b);
  b.add("key", velocypack::Value(e.key));
  b.add("value", velocypack::Value(e.value));
}
