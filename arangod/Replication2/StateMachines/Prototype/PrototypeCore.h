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

#pragma once

#include "PrototypeLogEntry.h"

#include "Basics/overload.h"
#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of
// data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift
// intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/map.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

namespace arangodb {
struct LoggerContext;
}  // namespace arangodb

namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack

namespace arangodb::replication2::replicated_state::prototype {

struct IPrototypeStorageInterface;
struct PrototypeLogEntry;

struct PrototypeDump {
  replication2::LogIndex lastPersistedIndex;
  std::unordered_map<std::string, std::string> map;

  void toVelocyPack(velocypack::Builder&);
  static auto fromVelocyPack(velocypack::Slice) -> PrototypeDump;
};

template<class Inspector>
auto inspect(Inspector& f, PrototypeDump& x) {
  return f.object(x).fields(f.field("lastPersistedIndex", x.lastPersistedIndex),
                            f.field("map", x.map));
}

struct PrototypeCore {
  using StorageType = ::immer::map<std::string, std::string>;
  static constexpr std::size_t kFlushBatchSize = 1000;

  explicit PrototypeCore(GlobalLogIdentifier logId, LoggerContext loggerContext,
                         std::shared_ptr<IPrototypeStorageInterface> storage);

  template<typename EntryIterator>
  void applyEntries(std::unique_ptr<EntryIterator> ptr);

  auto getSnapshot() -> std::unordered_map<std::string, std::string>;
  void applySnapshot(
      std::unordered_map<std::string, std::string> const& snapshot);

  bool flush();
  void loadStateFromDB();

  auto getDump() -> PrototypeDump;
  auto get(std::string const& key) noexcept -> std::optional<std::string>;
  auto get(std::vector<std::string> const& keys)
      -> std::unordered_map<std::string, std::string>;

  [[nodiscard]] auto getLastPersistedIndex() const noexcept -> LogIndex const&;
  [[nodiscard]] auto getLogId() const noexcept -> GlobalLogIdentifier const&;

  LoggerContext const loggerContext;

 private:
  GlobalLogIdentifier _logId;
  LogIndex _lastPersistedIndex;
  LogIndex _lastAppliedIndex;
  StorageType _store;
  std::shared_ptr<IPrototypeStorageInterface> _storage;
};

/*
 * Ensure this stays idempotent.
 */
template<typename EntryIterator>
void PrototypeCore::applyEntries(std::unique_ptr<EntryIterator> ptr) {
  auto lastAppliedIndex = ptr->range().to.saturatedDecrement();
  while (auto entry = ptr->next()) {
    PrototypeLogEntry const& logEntry = entry->second;
    std::visit(overload{
                   [&](PrototypeLogEntry::InsertOperation const& op) {
                     for (auto const& [key, value] : op.map) {
                       _store = _store.set(key, value);
                     }
                   },
                   [&](PrototypeLogEntry::DeleteOperation const& op) {
                     for (auto const& it : op.keys) {
                       _store = _store.erase(it);
                     }
                   },
               },
               logEntry.op);
  }
  _lastAppliedIndex = std::move(lastAppliedIndex);
}

}  // namespace arangodb::replication2::replicated_state::prototype