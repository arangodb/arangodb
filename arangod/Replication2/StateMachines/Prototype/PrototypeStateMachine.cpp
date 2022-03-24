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
#include "Basics/Exceptions.h"
#include "Futures/Future.h"
#include "velocypack/Iterator.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

#include "PrototypeStateMachine.h"

#include <rocksdb/utilities/transaction_db.h>
#include <utility>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::prototype;

/*
 * Ensure this stays idempotent.
 */
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
  resolvePromises(lastAppliedIndex);
}

bool PrototypeCore::flush() {
  if (lastAppliedIndex <= lastPersistedIndex + kFlushBatchSize) {
    // no need to flush
    return false;
  }
  auto result = storage->put(logId, getDump());
  if (result.ok()) {
    lastPersistedIndex = lastAppliedIndex;
    return true;
  } else {
    // TODO can I pass the loggerContext from ReplicatedState when building the
    // core? LOG_CTX("98433", ERR, loggerContext);
  }
  return false;
}

void PrototypeCore::loadStateFromDB() {
  auto result = storage->get(logId);
  if (result.ok()) {
    auto dump = result.get();
    lastPersistedIndex = lastAppliedIndex = dump.lastPersistedIndex;
    for (auto [k, v] : dump.map) {
      store = store.set(k, v);
    }
  } else {
    THROW_ARANGO_EXCEPTION(result.result());
  }
}

void PrototypeCore::applySnapshot(
    std::unordered_map<std::string, std::string> snapshot) {
  for (auto& [k, v] : snapshot) {
    store = store.set(k, v);
  }
}

/*
 * Resolve waitForApplied promises up to and including appliedIndex.
 */
void PrototypeCore::resolvePromises(LogIndex appliedIndex) {
  TRI_ASSERT(appliedIndex <= lastAppliedIndex);
  auto const end = waitForAppliedQueue.upper_bound(appliedIndex);
  for (auto it = waitForAppliedQueue.begin(); it != end; ++it) {
    it->second.setValue();
  }
  waitForAppliedQueue.erase(waitForAppliedQueue.begin(), end);
}

auto PrototypeCore::getDump() -> PrototypeCoreDump {
  // after we write to DB, we set lastPersistedIndex to lastAppliedIndex
  // we want to persist the already updated value of lastPersistedIndex
  return PrototypeCoreDump{
      lastAppliedIndex,
      std::unordered_map<std::string, std::string>(store.begin(), store.end())};
}

void PrototypeCoreDump::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  // after we write to DB, we set lastPersistedIndex to lastAppliedIndex
  // we want to persist the already updated value of lastPersistedIndex
  builder.add("lastPersistedIndex", lastPersistedIndex);
  builder.add(VPackValue("map"));
  {
    VPackObjectBuilder mapOb(&builder);
    for (auto const& [key, value] : map) {
      builder.add(key, value);
    }
  }
}

auto PrototypeCoreDump::fromVelocyPack(velocypack::Slice slice)
    -> PrototypeCoreDump {
  PrototypeCoreDump dump;
  if (auto s = slice.get("lastPersistedIndex"); !s.isNone()) {
    dump.lastPersistedIndex = LogIndex{s.getUInt()};
  }
  if (auto s = slice.get("map"); s.isObject()) {
    for (auto [k, v] : VPackObjectIterator(s)) {
      dump.map.emplace(k.copyString(), v.copyString());
    }
  }
  return dump;
}

auto PrototypeCore::waitForApplied(LogIndex index)
    -> futures::Future<futures::Unit> {
  if (lastAppliedIndex >= index) {
    return futures::Future<futures::Unit>{std::in_place};
  }
  auto it = waitForAppliedQueue.emplace(index, WaitForAppliedPromise{});
  auto f = it->second.getFuture();
  TRI_ASSERT(f.valid());
  return f;
}

PrototypeCore::PrototypeCore(
    GlobalLogIdentifier logId,
    std::shared_ptr<IPrototypeStorageInterface> storage)
    : logId(std::move(logId)), storage(std::move(storage)) {
  loadStateFromDB();
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
  return guardedData.doUnderLock(
      [self = shared_from_this(), ptr = move(ptr)](auto& core) mutable {
        if (!core) {
          return Result{TRI_ERROR_CLUSTER_NOT_LEADER};
        }
        core->lastAppliedIndex = ptr->range().to.saturatedDecrement();
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
            [self = self, idx = idx,
             entries = std::move(entries)](auto& core) -> ResultT<LogIndex> {
              if (!core) {
                return ResultT<LogIndex>::error(TRI_ERROR_CLUSTER_NOT_LEADER);
              }
              for (auto const& [key, value] : entries) {
                core->store = core->store.set(key, value);
              }
              core->lastAppliedIndex = idx;
              core->resolvePromises(idx);
              if (core->flush()) {
                // auto stream = self->getStream();
                // stream->release(core->lastPersistedIndex);
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
        [self = self, key = std::move(key),
         idx = idx](auto& core) -> ResultT<LogIndex> {
          if (!core) {
            return ResultT<LogIndex>::error(TRI_ERROR_CLUSTER_NOT_LEADER);
          }
          core->store = core->store.erase(key);
          core->lastAppliedIndex = idx;
          core->resolvePromises(idx);
          if (core->flush()) {
            // auto stream = self->getStream();
            // stream->release(core->lastPersistedIndex);
          }
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
            [self = self, entries = std::move(entries),
             idx = idx](auto& core) -> ResultT<LogIndex> {
              if (!core) {
                return ResultT<LogIndex>::error(TRI_ERROR_CLUSTER_NOT_LEADER);
              }
              for (auto it{entries.begin()}; it != entries.end(); ++it) {
                core->store = core->store.erase(*it);
              }
              core->lastAppliedIndex = idx;
              core->resolvePromises(idx);
              if (core->flush()) {
                // auto stream = self->getStream();
                // stream->release(core->lastPersistedIndex);
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

auto PrototypeLeaderState::getSnapshot(LogIndex waitForIndex)
    -> futures::Future<ResultT<std::unordered_map<std::string, std::string>>> {
  return guardedData.doUnderLock([waitForIndex,
                                  self = shared_from_this()](auto& core)
                                     -> futures::Future<
                                         ResultT<std::unordered_map<
                                             std::string, std::string>>> {
    if (!core) {
      return ResultT<std::unordered_map<std::string, std::string>>::error(
          TRI_ERROR_CLUSTER_NOT_LEADER);
    }
    // TODO use then
    return core->waitForApplied(waitForIndex)
        .thenValue([&core, self](auto&& r)
                       -> ResultT<
                           std::unordered_map<std::string, std::string>> {
          if (!core) {
            return ResultT<std::unordered_map<std::string, std::string>>::error(
                TRI_ERROR_CLUSTER_NOT_LEADER);
          }
          std::unordered_map<std::string, std::string> result{
              core->store.begin(), core->store.end()};
          return ResultT<std::unordered_map<std::string, std::string>>::success(
              result);
        });
  });
}

PrototypeFollowerState::PrototypeFollowerState(
    std::unique_ptr<PrototypeCore> core,
    std::shared_ptr<IPrototypeNetworkInterface> networkInterface)
    : logIdentifier(core->logId),
      guardedData(std::move(core)),
      networkInterface(std::move(networkInterface)) {}

auto PrototypeFollowerState::acquireSnapshot(ParticipantId const& destination,
                                             LogIndex waitForIndex) noexcept
    -> futures::Future<Result> {
  ResultT<std::shared_ptr<IPrototypeLeaderInterface>> leader =
      networkInterface->getLeaderInterface(destination);
  if (leader.fail()) {
    return leader.result();
  }
  return leader.get()
      ->getSnapshot(logIdentifier, waitForIndex)
      .thenValue([self = shared_from_this()](auto&& result) -> Result {
        if (result.fail()) {
          return result.result();
        }

        auto map = result.get();
        self->guardedData.doUnderLock(
            [self, map = std::move(map)](auto& core) mutable {
              core->applySnapshot(std::move(map));
            });
        return TRI_ERROR_NO_ERROR;
      });
}

auto PrototypeFollowerState::applyEntries(
    std::unique_ptr<EntryIterator> ptr) noexcept -> futures::Future<Result> {
  return guardedData.doUnderLock(
      [self = shared_from_this(), ptr = std::move(ptr)](auto& core) mutable {
        if (!core) {
          return Result{TRI_ERROR_CLUSTER_NOT_FOLLOWER};
        }
        core->lastAppliedIndex = ptr->range().to.saturatedDecrement();
        core->applyEntries(std::move(ptr));
        if (core->flush()) {
          // auto stream = self->getStream();
          // stream->release(core->lastPersistedIndex);
        }
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

auto PrototypeFollowerState::dumpContent()
    -> std::unordered_map<std::string, std::string> {
  return guardedData.doUnderLock([](auto& core) {
    TRI_ASSERT(core != nullptr);
    std::unordered_map<std::string, std::string> map(core->store.begin(),
                                                     core->store.end());
    return map;
  });
}

auto PrototypeFactory::constructFollower(std::unique_ptr<PrototypeCore> core)
    -> std::shared_ptr<PrototypeFollowerState> {
  return std::make_shared<PrototypeFollowerState>(std::move(core),
                                                  networkInterface);
}

auto PrototypeFactory::constructLeader(std::unique_ptr<PrototypeCore> core)
    -> std::shared_ptr<PrototypeLeaderState> {
  return std::make_shared<PrototypeLeaderState>(std::move(core));
}
PrototypeFactory::PrototypeFactory(
    std::shared_ptr<IPrototypeNetworkInterface> networkInterface,
    std::shared_ptr<IPrototypeStorageInterface> storageInterface)
    : networkInterface(std::move(networkInterface)),
      storageInterface(std::move(storageInterface)) {}

auto PrototypeFactory::constructCore(GlobalLogIdentifier const& gid)
    -> std::unique_ptr<PrototypeCore> {
  return std::make_unique<PrototypeCore>(gid, storageInterface);
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
