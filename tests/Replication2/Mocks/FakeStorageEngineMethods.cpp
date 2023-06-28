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

#include "FakeStorageEngineMethods.h"

#include <utility>

namespace {
template<typename F, typename R = std::invoke_result_t<F>>
auto invoke_on_executor(
    std::shared_ptr<arangodb::replication2::storage::rocksdb::
                        AsyncLogWriteBatcher::IAsyncExecutor> const& executor,
    F&& fn) -> arangodb::futures::Future<R> {
  auto p = arangodb::futures::Promise<R>{};
  auto f = p.getFuture();

  executor->operator()(
      [p = std::move(p), fn = std::forward<F>(fn)]() mutable noexcept {
        p.setValue(std::forward<F>(fn)());
      });

  return f;
}
}  // namespace

namespace arangodb::replication2::storage::test {

auto FakeStorageEngineMethods::updateMetadata(storage::PersistedStateInfo info)
    -> Result {
  _self.meta = std::move(info);
  return {};
}

auto FakeStorageEngineMethods::readMetadata()
    -> arangodb::ResultT<storage::PersistedStateInfo> {
  if (_self.meta.has_value()) {
    return {*_self.meta};
  } else {
    return {TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND};
  }
}

auto FakeStorageEngineMethods::read(replication2::LogIndex start)
    -> std::unique_ptr<PersistedLogIterator> {
  struct ContainerIterator : PersistedLogIterator {
    using Container = FakeStorageEngineMethodsContext::LogContainerType;
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

  return std::make_unique<ContainerIterator>(_self.log, start);
}

auto FakeStorageEngineMethods::insert(
    std::unique_ptr<PersistedLogIterator> iter,
    const storage::IStorageEngineMethods::WriteOptions& options)
    -> arangodb::futures::Future<
        arangodb::ResultT<storage::IStorageEngineMethods::SequenceNumber>> {
  return invoke_on_executor(
      _self.executor,
      [this, iter = std::move(iter), options]()
          -> arangodb::ResultT<storage::IStorageEngineMethods::SequenceNumber> {
        auto lastIndex = LogIndex{0};
        while (auto entry = iter->next()) {
          auto const res =
              _self.log.try_emplace(entry->logIndex(), entry.value());
          TRI_ASSERT(res.second)
              << "duplicated log entry " << entry->logIndex();

          TRI_ASSERT(entry->logIndex() > lastIndex);
          lastIndex = entry->logIndex();
          if (options.waitForSync) {
            _self.writtenWithWaitForSync.insert(entry->logIndex());
          }
        }
        return {_self.lastSequenceNumber++};
      });
}

auto FakeStorageEngineMethods::removeFront(
    LogIndex stop, const storage::IStorageEngineMethods::WriteOptions& options)
    -> arangodb::futures::Future<
        arangodb::ResultT<storage::IStorageEngineMethods::SequenceNumber>> {
  return invoke_on_executor(
      _self.executor,
      [this, stop]()
          -> arangodb::ResultT<storage::IStorageEngineMethods::SequenceNumber> {
        _self.log.erase(_self.log.begin(), _self.log.lower_bound(stop));
        return {_self.lastSequenceNumber++};
      });
}

auto FakeStorageEngineMethods::removeBack(
    LogIndex start, const storage::IStorageEngineMethods::WriteOptions& options)
    -> arangodb::futures::Future<
        arangodb::ResultT<storage::IStorageEngineMethods::SequenceNumber>> {
  return invoke_on_executor(
      _self.executor,
      [this, start]()
          -> arangodb::ResultT<storage::IStorageEngineMethods::SequenceNumber> {
        _self.log.erase(_self.log.lower_bound(start), _self.log.end());
        return {_self.lastSequenceNumber++};
      });
}

auto FakeStorageEngineMethods::getObjectId() -> std::uint64_t {
  return _self.objectId;
}
auto FakeStorageEngineMethods::getLogId() -> LogId { return _self.logId; }
auto FakeStorageEngineMethods::getSyncedSequenceNumber()
    -> storage::IStorageEngineMethods::SequenceNumber {
  TRI_ASSERT(false) << "not implemented";
  std::abort();
}
auto FakeStorageEngineMethods::waitForSync(
    storage::IStorageEngineMethods::SequenceNumber number)
    -> arangodb::futures::Future<Result> {
  futures::Promise<Result> promise;
  promise.setValue(Result{});
  return promise.getFuture();
}

FakeStorageEngineMethods::FakeStorageEngineMethods(
    FakeStorageEngineMethodsContext& self)
    : _self(self) {}

void FakeStorageEngineMethods::waitForCompletion() noexcept {}

auto FakeStorageEngineMethodsContext::getMethods()
    -> std::unique_ptr<storage::IStorageEngineMethods> {
  auto methods = std::make_unique<FakeStorageEngineMethods>(*this);
  return methods;
}

FakeStorageEngineMethodsContext::FakeStorageEngineMethodsContext(
    std::uint64_t objectId, arangodb::replication2::LogId logId,
    std::shared_ptr<rocksdb::AsyncLogWriteBatcher::IAsyncExecutor> executor,
    LogRange range, std::optional<storage::PersistedStateInfo> meta)
    : objectId(objectId),
      logId(logId),
      executor(std::move(executor)),
      meta(std::move(meta)) {
  emplaceLogRange(range, LogTerm{1});
}

void FakeStorageEngineMethodsContext::emplaceLogRange(LogRange range,
                                                      LogTerm term) {
  for (auto idx : range) {
    log.emplace(idx, PersistingLogEntry{term, idx,
                                        LogPayload::createFromString(
                                            "(" + to_string(term) + "," +
                                            to_string(idx) + ")")});
  }
}

}  // namespace arangodb::replication2::storage::test
