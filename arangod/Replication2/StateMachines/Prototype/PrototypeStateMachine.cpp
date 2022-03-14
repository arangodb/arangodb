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

#include "Basics/overload.h"
#include "Basics/voc-errors.h"
#include "Futures/Future.h"
#include "velocypack/Iterator.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

#include "PrototypeStateMachine.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::prototype;

template<typename EntryIterator>
void PrototypeCore::applyEntries(std::unique_ptr<EntryIterator> ptr) {
  while (auto entry = ptr->next()) {
    PrototypeLogEntry const& logEntry = entry->second;
    std::visit(overload{
                   [&](PrototypeLogEntry::DeleteOperation const& op) {
                     store = store.erase(op.key);
                   },
                   [&](PrototypeLogEntry::InsertOperation const& op) {
                     for (auto const& [key, value] : op.entries) {
                       store = store.set(key, value);
                     }
                   },
                   [&](PrototypeLogEntry::BulkDeleteOperation const& op) {
                     for (auto const& it : op.keys) {
                       store = store.erase(it);
                     }
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
  return guardedData.doUnderLock([ptr = move(ptr)](auto& core) mutable {
    if (!core) {
      return Result{TRI_ERROR_CLUSTER_NOT_LEADER};
    }
    core->applyEntries(std::move(ptr));
    return Result{TRI_ERROR_NO_ERROR};
  });
}

auto PrototypeLeaderState::set(
    std::unordered_map<std::string, std::string> entries)
    -> futures::Future<ResultT<LogIndex>> {
  auto stream = getStream();

  PrototypeLogEntry entry{PrototypeLogEntry::InsertOperation{entries}};
  auto idx = stream->insert(entry);

  return stream->waitFor(idx).thenValue(
      [self = shared_from_this(), idx = idx,
       entries = std::move(entries)](auto&& res) mutable {
        return self->guardedData.doUnderLock(
            [idx = idx,
             entries = std::move(entries)](auto& core) -> ResultT<LogIndex> {
              if (!core) {
                return ResultT<LogIndex>::error(TRI_ERROR_CLUSTER_NOT_LEADER);
              }
              for (auto const& [key, value] : entries) {
                core->store = core->store.set(key, value);
              }
              return idx;
            });
      });
}

auto PrototypeLeaderState::remove(std::string key)
    -> futures::Future<ResultT<LogIndex>> {
  auto stream = getStream();
  PrototypeLogEntry entry{PrototypeLogEntry::DeleteOperation{.key = key}};
  auto idx = stream->insert(entry);

  return stream->waitFor(idx).thenValue([self = shared_from_this(),
                                         key = std::move(key),
                                         idx = idx](auto&& res) mutable {
    return self->guardedData.doUnderLock(
        [key = std::move(key), idx = idx](auto& core) -> ResultT<LogIndex> {
          if (!core) {
            return ResultT<LogIndex>::error(TRI_ERROR_CLUSTER_NOT_LEADER);
          }
          core->store = core->store.erase(key);
          return idx;
        });
  });
}

auto PrototypeLeaderState::remove(std::vector<std::string> keys)
    -> futures::Future<ResultT<LogIndex>> {
  auto stream = getStream();

  PrototypeLogEntry entry{PrototypeLogEntry::BulkDeleteOperation{.keys = keys}};
  auto idx = stream->insert(entry);

  return stream->waitFor(idx).thenValue(
      [self = shared_from_this(), idx = idx,
       entries = std::move(keys)](auto&& res) mutable {
        return self->guardedData.doUnderLock(
            [entries = std::move(entries),
             idx = idx](auto& core) -> ResultT<LogIndex> {
              if (!core) {
                return ResultT<LogIndex>::error(TRI_ERROR_CLUSTER_NOT_LEADER);
              }
              for (auto it{entries.begin()}; it != entries.end(); ++it) {
                core->store = core->store.erase(*it);
              }
              return idx;
            });
      });
}

auto PrototypeLeaderState::get(std::string key) -> std::optional<std::string> {
  return guardedData.doUnderLock(
      [key = std::move(key)](auto& core) -> std::optional<std::string> {
        if (!core) {
          return std::nullopt;
        }
        if (auto it = core->store.find(key); it != nullptr) {
          return *it;
        }
        return std::nullopt;
      });
}

auto PrototypeLeaderState::getSnapshot()
    -> ResultT<std::unordered_map<std::string, std::string>> {
  return guardedData.doUnderLock(
      [](auto& core) -> ResultT<std::unordered_map<std::string, std::string>> {
        if (!core) {
          return ResultT<std::unordered_map<std::string, std::string>>::error(
              TRI_ERROR_CLUSTER_NOT_LEADER);
        }
        std::unordered_map<std::string, std::string> result{core->store.begin(),
                                                            core->store.end()};
        return result;
      });
}

PrototypeFollowerState::PrototypeFollowerState(
    std::unique_ptr<PrototypeCore> core)
    : guardedData(std::move(core)) {}

auto PrototypeFollowerState::acquireSnapshot(ParticipantId const& destination,
                                             LogIndex) noexcept
    -> futures::Future<Result> {
  // TODO
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

auto PrototypeFollowerState::get(std::string key)
    -> std::optional<std::string> {
  return guardedData.doUnderLock(
      [key = std::move(key)](auto& core) -> std::optional<std::string> {
        if (!core) {
          return std::nullopt;
        }
        if (auto it = core->store.find(key); it != nullptr) {
          return *it;
        }
        return std::nullopt;
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
  TRI_ASSERT(type.isString());

  if (type.isEqualString("delete")) {
    auto key = s.get("key");
    TRI_ASSERT(key.isString());
    return PrototypeLogEntry{
        .operation = PrototypeLogEntry::DeleteOperation{key.copyString()}};
  } else if (type.isEqualString("insert")) {
    auto mapOb = s.get("entries");
    TRI_ASSERT(mapOb.isObject());
    std::unordered_map<std::string, std::string> map;
    for (auto const& [key, value] : VPackObjectIterator(mapOb)) {
      map.emplace(key.copyString(), value.copyString());
    }
    return PrototypeLogEntry{
        .operation = PrototypeLogEntry::InsertOperation{std::move(map)}};
  } else if (type.isEqualString("bulkDelete")) {
    auto keysAr = s.get("keys");
    TRI_ASSERT(keysAr.isArray());
    std::vector<std::string> keys;
    for (auto const& it : VPackArrayIterator(keysAr)) {
      keys.emplace_back(it.copyString());
    }
    return PrototypeLogEntry{
        .operation = PrototypeLogEntry::BulkDeleteOperation{std::move(keys)}};
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
                 [&b](PrototypeLogEntry::DeleteOperation const& op) {
                   VPackObjectBuilder ob(&b);
                   b.add("type", "delete");
                   b.add("key", op.key);
                 },
                 [&b](PrototypeLogEntry::InsertOperation const& op) {
                   VPackObjectBuilder ob(&b);
                   b.add("type", "insert");
                   {
                     VPackObjectBuilder ob2(&b, "entries");
                     for (auto const& [key, value] : op.entries) {
                       b.add(key, value);
                     }
                   }
                 },
                 [&b](PrototypeLogEntry::BulkDeleteOperation const& op) {
                   VPackObjectBuilder ob(&b);
                   b.add("type", "bulkDelete");
                   {
                     VPackArrayBuilder ob2(&b, "keys");
                     for (auto const& it : op.keys) {
                       b.add(VPackValue(it));
                     }
                   }
                 },
             },
             e.operation);
}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"

template struct replicated_state::ReplicatedState<PrototypeState>;
