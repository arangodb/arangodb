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

#include "Basics/ResultT.h"
#include "Basics/UnshackledMutex.h"
#include "Futures/Future.h"
#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/StateInterfaces.h"

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
#include <variant>
#include <unordered_map>

namespace rocksdb {
class TransactionDB;
}

namespace arangodb::replication2::replicated_state {
/**
 * This prototype state machine acts as a simple key value store. It is meant to
 * be used during integration tests.
 * Doesn't deal with persisting data.
 * Snapshot transfers are supported.
 */
namespace prototype {

/*
 * This doesn't work yet with AppleClang.
template<class T>
concept StringIterator =
    std::same_as<typename std::iterator_traits<T>::value_type, std::string>;

template<class T>
concept MapStringIterator =
    std::same_as<typename std::iterator_traits<T>::value_type,
                 std::pair<const std::string, std::string>> ||
    std::same_as<typename std::iterator_traits<T>::value_type,
                 std::pair<std::string, std::string>>;
*/

struct PrototypeFactory;
struct PrototypeLogEntry;
struct PrototypeLeaderState;
struct PrototypeFollowerState;
struct PrototypeCore;
struct PrototypeCoreDump;
struct IPrototypeStorageInterface;

struct PrototypeState {
  using LeaderType = PrototypeLeaderState;
  using FollowerType = PrototypeFollowerState;
  using EntryType = PrototypeLogEntry;
  using FactoryType = PrototypeFactory;
  using CoreType = PrototypeCore;
};

struct PrototypeLogEntry {
  struct InsertOperation {
    std::unordered_map<std::string, std::string> entries;
  };
  struct DeleteOperation {
    std::string key;
  };
  struct BulkDeleteOperation {
    std::vector<std::string> keys;
  };

  std::variant<DeleteOperation, InsertOperation, BulkDeleteOperation> operation;
};

struct PrototypeCore {
  using StorageType = ::immer::map<std::string, std::string>;
  using WaitForAppliedPromise = futures::Promise<futures::Unit>;
  using WaitForAppliedQueue = std::multimap<LogIndex, WaitForAppliedPromise>;

  static constexpr std::size_t kFlushBatchSize = 1;

  StorageType store;
  // TODO move outside
  WaitForAppliedQueue waitForAppliedQueue;
  LogIndex lastAppliedIndex;
  LogIndex lastPersistedIndex;
  GlobalLogIdentifier logId;
  std::shared_ptr<IPrototypeStorageInterface> storage;

  explicit PrototypeCore(GlobalLogIdentifier logId,
                         std::shared_ptr<IPrototypeStorageInterface> storage);
  template<typename EntryIterator>
  void applyEntries(std::unique_ptr<EntryIterator> ptr);

  void applySnapshot(std::unordered_map<std::string, std::string> snapshot);

  void resolvePromises(LogIndex index);

  // TODO use DeferredAction
  auto waitForApplied(LogIndex index) -> futures::Future<futures::Unit>;

  bool flush();
  void loadStateFromDB();

  auto getDump() -> PrototypeCoreDump;
};

struct PrototypeLeaderState
    : IReplicatedLeaderState<PrototypeState>,
      std::enable_shared_from_this<PrototypeLeaderState> {
  explicit PrototypeLeaderState(std::unique_ptr<PrototypeCore> core);

  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<PrototypeCore> override;

  auto recoverEntries(std::unique_ptr<EntryIterator> ptr)
      -> futures::Future<Result> override;

  auto set(std::unordered_map<std::string, std::string> entries)
      -> futures::Future<ResultT<LogIndex>>;
  template<class Iterator>
  auto set(Iterator begin, Iterator end) -> futures::Future<ResultT<LogIndex>>;

  auto remove(std::string key) -> futures::Future<ResultT<LogIndex>>;
  auto remove(std::vector<std::string> keys)
      -> futures::Future<ResultT<LogIndex>>;
  template<class Iterator>
  auto remove(Iterator begin, Iterator end)
      -> futures::Future<ResultT<LogIndex>>;

  auto get(std::string key) -> std::optional<std::string>;
  template<class Iterator>
  auto get(Iterator begin, Iterator end)
      -> std::unordered_map<std::string, std::string>;
  auto getSnapshot(LogIndex waitForIndex)
      -> futures::Future<ResultT<std::unordered_map<std::string, std::string>>>;

  Guarded<std::unique_ptr<PrototypeCore>, basics::UnshackledMutex> guardedData;
};

template<class Iterator>
auto PrototypeLeaderState::get(Iterator begin, Iterator end)
    -> std::unordered_map<std::string, std::string> {
  return guardedData.doUnderLock([begin, end](auto& core) {
    std::unordered_map<std::string, std::string> result;
    if (!core) {
      return result;
    }
    for (auto it{begin}; it != end; ++it) {
      if (auto found = core->store.find(*it); found != nullptr) {
        result.emplace(*it, *found);
      }
    }
    return result;
  });
}

template<class Iterator>
auto PrototypeLeaderState::set(Iterator begin, Iterator end)
    -> futures::Future<ResultT<LogIndex>> {
  auto stream = getStream();

  PrototypeLogEntry entry{
      PrototypeLogEntry::InsertOperation{.entries = {begin, end}}};
  auto idx = stream->insert(entry);

  return stream->waitFor(idx).thenValue(
      [self = shared_from_this(), idx, begin, end](auto&& res) {
        return self->guardedData.doUnderLock(
            [idx, begin, end](auto& core) -> ResultT<LogIndex> {
              if (!core) {
                return Result{TRI_ERROR_CLUSTER_NOT_LEADER};
              }
              for (auto it{begin}; it != end; ++it) {
                core->store = core->store.set(it->first, it->second);
              }
              return idx;
              core->lastAppliedIndex = idx;
              core->resolvePromises(idx);
              core->flush();
            });
      });
}

template<class Iterator>
auto PrototypeLeaderState::remove(Iterator begin, Iterator end)
    -> futures::Future<ResultT<LogIndex>> {
  auto stream = getStream();

  PrototypeLogEntry entry{
      PrototypeLogEntry::BulkDeleteOperation{.keys = {begin, end}}};
  auto idx = stream->insert(entry);

  return stream->waitFor(idx).thenValue([self = shared_from_this(), begin, end,
                                         idx](auto&& res) {
    return self->guardedData.doUnderLock(
        [begin, end, idx](auto& core) -> futures::Future<ResultT<LogIndex>> {
          if (!core) {
            return ResultT<LogIndex>::error(TRI_ERROR_CLUSTER_NOT_LEADER);
          }
          for (auto it{begin}; it != end; ++it) {
            core->store = core->store.erase(*it);
          }
          return idx;
          core->lastAppliedIndex = idx;
          core->resolvePromises(idx);
          core->flush();
        });
  });
}

struct IPrototypeLeaderInterface {
  virtual ~IPrototypeLeaderInterface() = default;
  virtual auto getSnapshot(GlobalLogIdentifier const& logId,
                           LogIndex waitForIndex)
      -> futures::Future<
          ResultT<std::unordered_map<std::string, std::string>>> = 0;
};

struct IPrototypeNetworkInterface {
  virtual ~IPrototypeNetworkInterface() = default;
  virtual auto getLeaderInterface(ParticipantId id)
      -> ResultT<std::shared_ptr<IPrototypeLeaderInterface>> = 0;
};

struct IPrototypeStorageInterface {
  virtual ~IPrototypeStorageInterface() = default;
  virtual auto put(GlobalLogIdentifier const& logId, PrototypeCoreDump dump)
      -> Result = 0;
  virtual auto get(GlobalLogIdentifier const& logId)
      -> ResultT<PrototypeCoreDump> = 0;
};

struct PrototypeCoreDump {
  LogIndex lastPersistedIndex;
  std::unordered_map<std::string, std::string> map;

  void toVelocyPack(velocypack::Builder& builder) const;
  auto fromVelocyPack(velocypack::Slice slice) -> PrototypeCoreDump;
};

struct PrototypeFollowerState
    : IReplicatedFollowerState<PrototypeState>,
      std::enable_shared_from_this<PrototypeFollowerState> {
  explicit PrototypeFollowerState(std::unique_ptr<PrototypeCore>,
                                  std::shared_ptr<IPrototypeNetworkInterface>);

  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<PrototypeCore> override;

  auto acquireSnapshot(ParticipantId const& destination, LogIndex) noexcept
      -> futures::Future<Result> override;

  auto applyEntries(std::unique_ptr<EntryIterator> ptr) noexcept
      -> futures::Future<Result> override;

  auto get(std::string key) -> std::optional<std::string>;
  auto dumpContent() -> std::unordered_map<std::string, std::string>;

  GlobalLogIdentifier const logIdentifier;
  Guarded<std::unique_ptr<PrototypeCore>, basics::UnshackledMutex> guardedData;
  std::shared_ptr<IPrototypeNetworkInterface> const networkInterface;
};

struct PrototypeFactory {
  explicit PrototypeFactory(
      std::shared_ptr<IPrototypeNetworkInterface> networkInterface,
      std::shared_ptr<IPrototypeStorageInterface> storageInterface);

  auto constructFollower(std::unique_ptr<PrototypeCore> core)
      -> std::shared_ptr<PrototypeFollowerState>;
  auto constructLeader(std::unique_ptr<PrototypeCore> core)
      -> std::shared_ptr<PrototypeLeaderState>;
  auto constructCore(GlobalLogIdentifier const&)
      -> std::unique_ptr<PrototypeCore>;

  std::shared_ptr<IPrototypeNetworkInterface> const networkInterface;
  std::shared_ptr<IPrototypeStorageInterface> const storageInterface;
};

}  // namespace prototype

template<>
struct EntryDeserializer<prototype::PrototypeLogEntry> {
  auto operator()(streams::serializer_tag_t<prototype::PrototypeLogEntry>,
                  velocypack::Slice s) const -> prototype::PrototypeLogEntry;
};

template<>
struct EntrySerializer<prototype::PrototypeLogEntry> {
  void operator()(streams::serializer_tag_t<prototype::PrototypeLogEntry>,
                  prototype::PrototypeLogEntry const& e,
                  velocypack::Builder& b) const;
};

extern template struct replicated_state::ReplicatedState<
    prototype::PrototypeState>;
}  // namespace arangodb::replication2::replicated_state
