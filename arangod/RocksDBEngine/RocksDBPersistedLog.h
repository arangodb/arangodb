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

#include <rocksdb/db.h>

#include "Replication2/ReplicatedLog/PersistedLog.h"
#include "RocksDBKeyBounds.h"

#include <array>
#include <variant>
#include <memory>

namespace arangodb {

struct RocksDBLogPersistor : std::enable_shared_from_this<RocksDBLogPersistor> {
  struct Executor {
    virtual ~Executor() = default;
    virtual void operator()(fu2::unique_function<void() noexcept>) = 0;
  };

  RocksDBLogPersistor(
      rocksdb::ColumnFamilyHandle* cf, rocksdb::DB* db,
      std::shared_ptr<Executor> executor,
      std::shared_ptr<replication2::ReplicatedLogGlobalSettings const> options);

  struct WriteOptions {
    bool waitForSync = false;
  };

  struct InsertEntries {
    std::unique_ptr<arangodb::replication2::PersistedLogIterator> iter;
  };

  struct RemoveFront {
    replication2::LogIndex stop;
  };

  using Action = std::variant<InsertEntries, RemoveFront>;

  struct PersistRequest {
    PersistRequest(
        std::shared_ptr<arangodb::replication2::replicated_log::PersistedLog>
            log,
        Action action, futures::Promise<Result> promise)
        : log(std::move(log)),
          action(std::move(action)),
          promise(std::move(promise)) {}

    auto execute(rocksdb::WriteBatch& wb) -> Result;

    std::shared_ptr<arangodb::replication2::replicated_log::PersistedLog> log;
    Action action;
    futures::Promise<Result> promise;
  };

  auto persist(
      std::shared_ptr<arangodb::replication2::replicated_log::PersistedLog> log,
      Action action, WriteOptions const& options) -> futures::Future<Result>;

  struct Lane {
    Lane() = delete;

    struct WaitForSync {};
    struct DontWaitForSync {};
    explicit Lane(WaitForSync) : _waitForSync(true) {}
    explicit Lane(DontWaitForSync) : _waitForSync(false) {}

    std::mutex _persistorMutex;
    std::vector<PersistRequest> _pendingPersistRequests;
    std::atomic<unsigned> _activePersistorThreads = 0;
    bool const _waitForSync;
  };

  void runPersistorWorker(Lane& lane) noexcept;

  std::array<Lane, 2> _lanes = {Lane{Lane::WaitForSync{}},
                                Lane{Lane::DontWaitForSync{}}};

  rocksdb::ColumnFamilyHandle* const _cf;
  rocksdb::DB* const _db;
  std::shared_ptr<Executor> _executor;
  std::shared_ptr<replication2::ReplicatedLogGlobalSettings const> const
      _options;
};

class RocksDBPersistedLog
    : public replication2::replicated_log::PersistedLog,
      public std::enable_shared_from_this<RocksDBPersistedLog> {
 public:
  ~RocksDBPersistedLog() override = default;
  RocksDBPersistedLog(replication2::LogId id, uint64_t objectId,
                      std::shared_ptr<RocksDBLogPersistor> persistor);

  auto insert(replication2::PersistedLogIterator& iter, WriteOptions const&)
      -> Result override;
  auto insertAsync(std::unique_ptr<replication2::PersistedLogIterator> iter,
                   WriteOptions const&) -> futures::Future<Result> override;
  auto read(replication2::LogIndex start)
      -> std::unique_ptr<replication2::PersistedLogIterator> override;
  auto removeFront(replication2::LogIndex stop)
      -> futures::Future<Result> override;
  auto removeBack(replication2::LogIndex start) -> Result override;

  // On success, iter will be completely consumed and written to wb.
  auto prepareWriteBatch(replication2::PersistedLogIterator& iter,
                         rocksdb::WriteBatch& wb) -> Result;
  auto prepareRemoveFront(replication2::LogIndex stop, rocksdb::WriteBatch&)
      -> Result;

  uint64_t objectId() const { return _objectId; }

  auto drop() -> Result override;

  RocksDBKeyBounds getBounds() const {
    return RocksDBKeyBounds::LogRange(_objectId);
  }

 private:
  uint64_t const _objectId;
  std::shared_ptr<RocksDBLogPersistor> _persistor;
};

}  // namespace arangodb
