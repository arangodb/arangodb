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

#include "PrototypeCore.h"
#include "PrototypeStateMachine.h"
#include "PrototypeLeaderState.h"
#include "PrototypeFollowerState.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::prototype;

PrototypeCore::PrototypeCore(
    GlobalLogIdentifier logId, LoggerContext loggerContext,
    std::shared_ptr<IPrototypeStorageInterface> storage)
    : loggerContext(std::move(loggerContext)),
      _logId(std::move(logId)),
      _storage(std::move(storage)) {
  loadStateFromDB();
}

bool PrototypeCore::flush() {
  if (_lastAppliedIndex <= _lastPersistedIndex + kFlushBatchSize) {
    // no need to flush
    return false;
  }
  auto result = _storage->put(_logId, getDump());
  if (result.ok()) {
    _lastPersistedIndex = _lastAppliedIndex;
    LOG_CTX("af38a", TRACE, loggerContext)
        << "Prototype FLUSH successful, persisted index: "
        << _lastPersistedIndex;
    return true;
  } else {
    LOG_CTX("af0f6", ERR, loggerContext)
        << "Prototype FLUSH failed: " << result;
  }
  return false;
}

void PrototypeCore::loadStateFromDB() {
  auto result = _storage->get(_logId);
  if (result.ok()) {
    auto dump = std::move(result).get();
    _lastPersistedIndex = _lastAppliedIndex = dump.lastPersistedIndex;
    for (auto const& [k, v] : dump.map) {
      _store = _store.set(k, v);
    }
    LOG_CTX("e4cfb", TRACE, loggerContext)
        << "Prototype loaded state from DB, last index: " << _lastAppliedIndex;
  } else {
    THROW_ARANGO_EXCEPTION(result.result());
  }
}

auto PrototypeCore::getSnapshot()
    -> std::unordered_map<std::string, std::string> {
  auto snapshot = _store;
  return std::unordered_map<std::string, std::string>{snapshot.begin(),
                                                      snapshot.end()};
}

void PrototypeCore::applySnapshot(
    std::unordered_map<std::string, std::string> const& snapshot) {
  // Once the first applyEntries is executed, _lastAppliedIndex will have the
  // correct value.
  for (auto& [k, v] : snapshot) {
    _store = _store.set(k, v);
  }
}

auto PrototypeCore::getDump() -> PrototypeDump {
  // After we write to DB, we set lastPersistedIndex to lastAppliedIndex,
  // because we want to persist the already updated value of lastPersistedIndex.
  return PrototypeDump{_lastAppliedIndex, getSnapshot()};
}

auto PrototypeCore::get(std::string const& key) noexcept
    -> std::optional<std::string> {
  if (auto it = _store.find(key); it != nullptr) {
    return *it;
  }
  return std::nullopt;
}

auto PrototypeCore::get(std::vector<std::string> const& keys)
    -> std::unordered_map<std::string, std::string> {
  std::unordered_map<std::string, std::string> result;
  auto snapshot = _store;
  for (auto const& it : keys) {
    if (auto found = _store.find(it); found != nullptr) {
      result.emplace(it, *found);
    }
  }
  return result;
}

auto PrototypeCore::getLastPersistedIndex() const noexcept -> LogIndex const& {
  return _lastPersistedIndex;
}

auto PrototypeCore::getLogId() const noexcept -> GlobalLogIdentifier const& {
  return _logId;
}

void PrototypeDump::toVelocyPack(velocypack::Builder& b) {
  serialize<PrototypeDump>(b, *this);
}

auto PrototypeDump::fromVelocyPack(velocypack::Slice s) -> PrototypeDump {
  return deserialize<PrototypeDump>(s);
}
