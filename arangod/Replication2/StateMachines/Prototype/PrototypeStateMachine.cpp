////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include <Basics/voc-errors.h>
#include <Futures/Future.h>

#include "PrototypeStateMachine.h"
#include "Basics/overload.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::prototype;

template<typename EntryIterator>
void PrototypeCore::applyEntries(std::unique_ptr<EntryIterator> ptr) {
  while (auto entry = ptr->next()) {
    PrototypeLogEntry const& logEntry = entry->second;
    std::visit(overload{
                   [&](PrototypeLogEntry::InsertOperation const& op) {
                     store[op.key] = op.value;
                   },
                   [&](PrototypeLogEntry::DeleteOperation const& op) {
                     store.erase(op.key);
                   },
               },
               logEntry.operation);
  }
}

PrototypeLeaderState::PrototypeLeaderState(std::unique_ptr<PrototypeCore> core)
    : guardedData(std::move(core)) {}

auto PrototypeLeaderState::resign() && noexcept
    -> std::unique_ptr<PrototypeCore> {
  return guardedData.doUnderLock([](auto& core) {
    TRI_ASSERT(core != nullptr);
    return std::move(core);
  });
}

auto PrototypeLeaderState::recoverEntries(std::unique_ptr<EntryIterator> ptr)
    -> futures::Future<Result> {
  return guardedData.doUnderLock([ptr = std::move(ptr)](auto& core) mutable {
    if (!core) {
      return Result{TRI_ERROR_CLUSTER_NOT_LEADER};
    }
    core->applyEntries(std::move(ptr));
    return Result{TRI_ERROR_NO_ERROR};
  });
}

auto PrototypeLeaderState::set(std::string key, std::string value)
    -> futures::Future<Result> {
  auto stream = getStream();
  PrototypeLogEntry entry{
      PrototypeLogEntry::InsertOperation{.key = key, .value = value}};
  auto idx = stream->insert(entry);

  return stream->waitFor(idx).thenValue(
      [self = shared_from_this(), key = std::move(key),
       value = std::move(value)](auto&& res) mutable {
        return self->guardedData.template doUnderLock(
            [key = std::move(key),
             value = std::move(value)](auto& core) mutable {
              if (!core) {
                return Result{TRI_ERROR_CLUSTER_NOT_LEADER};
              }
              core->store[std::move(key)] = std::move(value);
              return Result{TRI_ERROR_NO_ERROR};
            });
      });
}

/*
auto PrototypeLeaderState::set(PrototypeCore::StorageType map) ->
futures::Future<Result> { auto stream = getStream(); PrototypeLogEntry entry{
      PrototypeLogEntry::InsertOperation{.key = key, .value = value}};
  auto idx = stream->insert(entry);

  return stream->waitFor(idx).thenValue(
      [self = shared_from_this(), key = std::move(key),
       value = std::move(value)](auto&& res) mutable {
        return self->guardedData.template doUnderLock(
            [key = std::move(key),
             value = std::move(value)](auto& core) mutable {
              if (!core) {
                return Result{TRI_ERROR_CLUSTER_NOT_LEADER};
              }
              core->store[std::move(key)] = std::move(value);
              return Result{TRI_ERROR_NO_ERROR};
            });
      });
}
 */

auto PrototypeLeaderState::remove(std::string key) -> futures::Future<Result> {
  auto stream = getStream();
  PrototypeLogEntry entry{PrototypeLogEntry::DeleteOperation{.key = key}};
  auto idx = stream->insert(entry);

  return stream->waitFor(idx).thenValue(
      [self = shared_from_this(), key = std::move(key)](auto&& res) mutable {
        return self->guardedData.template doUnderLock(
            [key = std::move(key)](auto& core) {
              if (!core) {
                return Result{TRI_ERROR_CLUSTER_NOT_LEADER};
              }
              core->store.erase(key);
              return Result{TRI_ERROR_NO_ERROR};
            });
      });
}

auto PrototypeLeaderState::get(std::string key) -> std::optional<std::string> {
  return guardedData.doUnderLock(
      [key = std::move(key)](auto& core) -> std::optional<std::string> {
        if (!core) {
          return std::nullopt;
        }
        if (auto it = core->store.find(key); it != std::end(core->store)) {
          return it->first;
        }
        return std::nullopt;
      });
}

template<typename Iterator, typename std::enable_if_t<std::is_same_v<
                                typename Iterator::value_type, std::string>>>
auto PrototypeLeaderState::get(Iterator const& begin, Iterator const& end)
    -> PrototypeCore::StorageType {
  return guardedData.template doUnderLock([&begin, &end](auto& core) {
    auto result = PrototypeCore::StorageType{};
    if (!core) {
      return result;
    }
    for (auto it{begin}; it != end; ++it) {
      if (auto found = core->store.find(*it); found != std::end(core->store)) {
        result[found->first] = found->second;
      }
    }
    return result;
  });
}

PrototypeFollowerState::PrototypeFollowerState(
    std::unique_ptr<PrototypeCore> core)
    : guardedData(std::move(core)) {}

auto PrototypeFollowerState::acquireSnapshot(ParticipantId const& destination,
                                             LogIndex) noexcept
    -> futures::Future<Result> {
  return {TRI_ERROR_NO_ERROR};
}

auto PrototypeFollowerState::applyEntries(
    std::unique_ptr<EntryIterator> ptr) noexcept -> futures::Future<Result> {
  return guardedData.doUnderLock([ptr = std::move(ptr)](auto& core) mutable {
    if (!core) {
      return Result{TRI_ERROR_CLUSTER_NOT_FOLLOWER};
    }
    core->applyEntries(std::move(ptr));
    return Result{TRI_ERROR_NO_ERROR};
  });
}

auto PrototypeFollowerState::resign() && noexcept
    -> std::unique_ptr<PrototypeCore> {
  return guardedData.doUnderLock([](auto& core) {
    TRI_ASSERT(core != nullptr);
    return std::move(core);
  });
}

auto PrototypeFactory::constructFollower(std::unique_ptr<PrototypeCore> core)
    -> std::shared_ptr<PrototypeFollowerState> {
  return std::make_shared<PrototypeFollowerState>(std::move(core));
}

auto PrototypeFactory::constructLeader(std::unique_ptr<PrototypeCore> core)
    -> std::shared_ptr<PrototypeLeaderState> {
  return std::make_shared<PrototypeLeaderState>(std::move(core));
}

auto replicated_state::EntryDeserializer<
    replicated_state::prototype::PrototypeLogEntry>::
operator()(
    streams::serializer_tag_t<replicated_state::prototype::PrototypeLogEntry>,
    velocypack::Slice s) const
    -> replicated_state::prototype::PrototypeLogEntry {
  auto type = s.get("type");
  TRI_ASSERT(!type.isNone());

  if (type.isEqualString("insert")) {
    auto key = s.get("key");
    auto value = s.get("value");
    TRI_ASSERT(!key.isNone() && !value.isNone());
    return PrototypeLogEntry{.operation = PrototypeLogEntry::InsertOperation{
                                 key.copyString(), value.copyString()}};
  } else if (type.isEqualString("delete")) {
    auto key = s.get("key");
    TRI_ASSERT(!key.isNone());
    return PrototypeLogEntry{
        .operation = PrototypeLogEntry::DeleteOperation{key.copyString()}};
  }
  TRI_ASSERT(false);
  return PrototypeLogEntry{};
}

void replicated_state::EntrySerializer<
    replicated_state::prototype::PrototypeLogEntry>::
operator()(
    streams::serializer_tag_t<replicated_state::prototype::PrototypeLogEntry>,
    prototype::PrototypeLogEntry const& e, velocypack::Builder& b) const {
  std::visit(overload{
                 [&b](PrototypeLogEntry::InsertOperation const& op) {
                   b.add("type", VPackValue("insert"));
                   b.add("key", VPackValue(op.key));
                   b.add("value", VPackValue(op.value));
                 },
                 [&b](PrototypeLogEntry::DeleteOperation const& op) {
                   b.add("type", VPackValue("delete"));
                   b.add("key", VPackValue(op.key));
                 },
             },
             e.operation);
}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"

template struct replicated_state::ReplicatedState<PrototypeState>;
