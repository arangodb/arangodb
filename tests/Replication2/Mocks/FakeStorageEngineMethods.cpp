////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2022 ArangoDB GmbH, Cologne, Germany
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
#include <utility>
#include "FakeStorageEngineMethods.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::test;

namespace {
template<typename F, typename R = std::invoke_result_t<F>>
auto invoke_on_executor(
    std::shared_ptr<RocksDBAsyncLogWriteBatcher::IAsyncExecutor> const&
        executor,
    F&& fn) -> futures::Future<R> {
  auto p = futures::Promise<R>{};
  auto f = p.getFuture();

  executor->operator()(
      [p = std::move(p), fn = std::forward<F>(fn)]() mutable noexcept {
        p.setValue(std::forward<F>(fn)());
      });

  return std::move(f);
}
}  // namespace

auto FakeStorageEngineMethods::updateMetadata(
    replicated_state::PersistedStateInfo info) -> Result {
  meta = std::move(info);
  return {};
}

auto FakeStorageEngineMethods::readMetadata()
    -> arangodb::ResultT<replicated_state::PersistedStateInfo> {
  if (meta.has_value()) {
    return {*meta};
  } else {
    return {TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND};
  }
}

auto FakeStorageEngineMethods::read(replication2::LogIndex start)
    -> std::unique_ptr<PersistedLogIterator> {
  struct ContainerIterator : PersistedLogIterator {
    using Container = FakeStorageEngineMethods::LogContainerType;
    using Iterator = Container ::iterator;
    ContainerIterator(Container store, LogIndex start)
        : _store(std::move(store)),
          _current(_store.lower_bound(start)),
          _end(_store.end()) {}

    auto next() -> std::optional<PersistingLogEntry> override {
      if (_current != _end) {
        auto it = _current;
        ++_current;
        return it->second;
      }
      return std::nullopt;
    }

    Container _store;
    Iterator _current;
    Iterator _end;
  };

  return std::make_unique<ContainerIterator>(log, start);
}

auto FakeStorageEngineMethods::insert(
    std::unique_ptr<PersistedLogIterator> iter,
    const replicated_state::IStorageEngineMethods::WriteOptions& options)
    -> arangodb::futures::Future<arangodb::ResultT<
        replicated_state::IStorageEngineMethods::SequenceNumber>> {
  return invoke_on_executor(
      executor,
      [this, iter = std::move(iter), options]()
          -> arangodb::ResultT<
              replicated_state::IStorageEngineMethods::SequenceNumber> {
        auto lastIndex = LogIndex{0};
        while (auto entry = iter->next()) {
          auto const res = log.try_emplace(entry->logIndex(), entry.value());
          TRI_ASSERT(res.second);

          TRI_ASSERT(entry->logIndex() > lastIndex);
          lastIndex = entry->logIndex();
          if (options.waitForSync) {
            writtenWithWaitForSync.insert(entry->logIndex());
          }
        }
        return {lastSequenceNumber++};
      });
}
auto FakeStorageEngineMethods::removeFront(
    LogIndex stop,
    const replicated_state::IStorageEngineMethods::WriteOptions& options)
    -> arangodb::futures::Future<arangodb::ResultT<
        replicated_state::IStorageEngineMethods::SequenceNumber>> {
  return invoke_on_executor(
      executor,
      [this, stop]()
          -> arangodb::ResultT<
              replicated_state::IStorageEngineMethods::SequenceNumber> {
        log.erase(log.begin(), log.lower_bound(stop));
        return {lastSequenceNumber++};
      });
}

auto FakeStorageEngineMethods::removeBack(
    LogIndex start,
    const replicated_state::IStorageEngineMethods::WriteOptions& options)
    -> arangodb::futures::Future<arangodb::ResultT<
        replicated_state::IStorageEngineMethods::SequenceNumber>> {
  return invoke_on_executor(
      executor,
      [this, start]()
          -> arangodb::ResultT<
              replicated_state::IStorageEngineMethods::SequenceNumber> {
        log.erase(log.lower_bound(start), log.end());
        return {lastSequenceNumber++};
      });
}

auto FakeStorageEngineMethods::getObjectId() -> std::uint64_t {
  return objectId;
}
auto FakeStorageEngineMethods::getLogId() -> LogId { return logId; }
auto FakeStorageEngineMethods::getSyncedSequenceNumber()
    -> replicated_state::IStorageEngineMethods::SequenceNumber {
  TRI_ASSERT(false) << "not implemented";
}
auto FakeStorageEngineMethods::waitForSync(
    replicated_state::IStorageEngineMethods::SequenceNumber number)
    -> arangodb::futures::Future<arangodb::futures::Unit> {
  TRI_ASSERT(false) << "not implemented";
}

FakeStorageEngineMethods::FakeStorageEngineMethods(
    std::uint64_t objectId, LogId logId,
    std::shared_ptr<RocksDBAsyncLogWriteBatcher::IAsyncExecutor> executor)
    : objectId(objectId), logId(logId), executor(std::move(executor)) {}
