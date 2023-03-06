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
#include <gtest/gtest.h>
#include <rocksdb/db.h>

#include "Basics/Exceptions.h"
#include "Basics/RocksDBUtils.h"
#include "Basics/files.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBPersistedLog.h"

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/Mocks/FakeStorageEngineMethods.h"
#include "Replication2/Mocks/FakeAsyncExecutor.h"

using namespace arangodb;
using namespace arangodb::replication2;

namespace arangodb::replication2::test {
struct RocksDBInstance {
  explicit RocksDBInstance(std::string path) : _path(std::move(path)) {
    rocksdb::Options options;
    options.create_if_missing = true;
    if (auto s = rocksdb::DB::Open(options, _path, &_db); !s.ok()) {
      auto res = rocksutils::convertStatus(s);
      THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
    }
  }

  ~RocksDBInstance() {
    delete _db;
    TRI_RemoveDirectory(_path.c_str());
  }

  [[nodiscard]] auto getDatabase() const -> rocksdb::DB* { return _db; }

 private:
  rocksdb::DB* _db = nullptr;
  std::string _path;
};

template<typename I>
struct SimpleIterator : PersistedLogIterator {
  SimpleIterator(I begin, I end) : current(begin), end(end) {}
  ~SimpleIterator() override = default;

  auto next() -> std::optional<PersistingLogEntry> override {
    if (current == end) {
      return std::nullopt;
    }

    return *(current++);
  }

  I current, end;
};

template<typename C, typename Iter = typename C::const_iterator>
auto make_iterator(C const& c) -> std::unique_ptr<SimpleIterator<Iter>> {
  return std::make_unique<SimpleIterator<Iter>>(c.begin(), c.end());
}
}  // namespace arangodb::replication2::test

template<typename Factory>
struct StorageEngineMethodsTest : ::testing::Test {
  static void SetUpTestCase() { Factory::SetUp(); }
  static void TearDownTestCase() { Factory::TearDown(); }
  void TearDown() override { Factory::Drop(std::move(methods)); }
  void SetUp() override {
    methods = Factory::BuildMethods(objectId, vocbaseId, logId, executor);
  }
  void dropAndRebuild() {
    TearDown();
    SetUp();
  }

  std::shared_ptr<RocksDBAsyncLogWriteBatcher::IAsyncExecutor> executor =
      std::make_shared<test::ThreadAsyncExecutor>();

  static constexpr std::uint64_t objectId = 1;
  static constexpr std::uint64_t vocbaseId = 1;
  static constexpr LogId logId = LogId{1};

  std::unique_ptr<replicated_state::IStorageEngineMethods> methods;
};

namespace arangodb::replication2::test {

struct RocksDBAsyncLogWriteBatcherMetricsMock
    : RocksDBAsyncLogWriteBatcherMetrics {
  RocksDBAsyncLogWriteBatcherMetricsMock() {
    numWorkerThreadsWaitForSync =
        makeMetric<arangodb_replication2_rocksdb_num_persistor_worker>();
    numWorkerThreadsNoWaitForSync =
        makeMetric<arangodb_replication2_rocksdb_num_persistor_worker>();
    queueLength = makeMetric<arangodb_replication2_rocksdb_queue_length>();
    writeBatchSize =
        makeMetric<arangodb_replication2_rocksdb_write_batch_size>();
    rocksdbWriteTimeInUs =
        makeMetric<arangodb_replication2_rocksdb_write_time>();
    rocksdbSyncTimeInUs = makeMetric<arangodb_replication2_rocksdb_sync_time>();
    operationLatencyInsert =
        makeMetric<arangodb_replication2_storage_operation_latency>();
    operationLatencyRemoveFront =
        makeMetric<arangodb_replication2_storage_operation_latency>();
    operationLatencyRemoveBack =
        makeMetric<arangodb_replication2_storage_operation_latency>();
  }

  template<typename Builder>
  auto makeMetric() -> typename Builder::MetricT* {
    static std::vector<std::shared_ptr<typename Builder::MetricT>> metrics;
    auto ptr =
        std::dynamic_pointer_cast<typename Builder::MetricT>(Builder{}.build());
    return metrics.emplace_back(ptr).get();
  }
};

struct RocksDBFactory {
  static void SetUp() {
    rocksutils::setRocksDBKeyFormatEndianess(RocksDBEndianness::Little);
    settings = std::make_shared<ReplicatedLogGlobalSettings>();
    rocksdb =
        std::make_shared<test::RocksDBInstance>("rocksdb-tests-replicated-log");
  }

  static void TearDown() { rocksdb.reset(); }

  static void Drop(
      std::unique_ptr<replicated_state::IStorageEngineMethods> methods) {
    auto& rocksdbMethods = dynamic_cast<RocksDBLogStorageMethods&>(*methods);
    rocksdbMethods.ctx.waitForCompletion();
    auto res = rocksdbMethods.drop();
    ASSERT_TRUE(res.ok());
  }

  static auto BuildMethods(
      std::uint64_t objectId, std::uint64_t vocbaseId, LogId logId,
      std::shared_ptr<RocksDBAsyncLogWriteBatcher::IAsyncExecutor> executor)
      -> std::unique_ptr<replicated_state::IStorageEngineMethods> {
    auto metrics = std::make_shared<RocksDBAsyncLogWriteBatcherMetricsMock>();

    std::shared_ptr<RocksDBAsyncLogWriteBatcher> writeBatcher =
        std::make_shared<RocksDBAsyncLogWriteBatcher>(
            rocksdb->getDatabase()->DefaultColumnFamily(),
            rocksdb->getDatabase(), std::move(executor), settings, metrics);

    return std::make_unique<RocksDBLogStorageMethods>(
        objectId, vocbaseId, logId, writeBatcher, rocksdb->getDatabase(),
        rocksdb->getDatabase()->DefaultColumnFamily(),
        rocksdb->getDatabase()->DefaultColumnFamily(), metrics);
  }

  static std::shared_ptr<test::RocksDBInstance> rocksdb;
  static std::shared_ptr<ReplicatedLogGlobalSettings> settings;
};

std::shared_ptr<test::RocksDBInstance> RocksDBFactory::rocksdb;
std::shared_ptr<ReplicatedLogGlobalSettings> RocksDBFactory::settings;

struct FakeFactory {
  static void SetUp() {}

  static void TearDown() {}

  static void Drop(
      std::unique_ptr<replicated_state::IStorageEngineMethods> methods) {}

  static auto BuildMethods(
      std::uint64_t objectId, std::uint64_t vocbaseId, LogId logId,
      std::shared_ptr<RocksDBAsyncLogWriteBatcher::IAsyncExecutor> executor)
      -> std::unique_ptr<replicated_state::IStorageEngineMethods> {
    context = std::make_shared<test::FakeStorageEngineMethodsContext>(
        objectId, logId, std::move(executor));
    return context->getMethods();
  };

  static std::shared_ptr<test::FakeStorageEngineMethodsContext> context;
};

std::shared_ptr<test::FakeStorageEngineMethodsContext> FakeFactory::context;
}  // namespace arangodb::replication2::test

using MyTypes = ::testing::Types<test::RocksDBFactory, test::FakeFactory>;
TYPED_TEST_SUITE(StorageEngineMethodsTest, MyTypes);

TYPED_TEST(StorageEngineMethodsTest, read_meta_data_not_found) {
  auto result = this->methods->readMetadata();
  ASSERT_EQ(result.errorNumber(), TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

TYPED_TEST(StorageEngineMethodsTest, write_meta_data) {
  replicated_state::PersistedStateInfo info;
  info.stateId = this->logId;
  info.snapshot.status = replicated_state::SnapshotStatus::kCompleted;
  {
    auto result = this->methods->updateMetadata(info);
    ASSERT_TRUE(result.ok());
  }
  {
    auto result = this->methods->readMetadata();
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result->snapshot.status,
              replicated_state::SnapshotStatus::kCompleted);
    EXPECT_EQ(result->stateId, this->logId);
  }

  info.snapshot.status = replicated_state::SnapshotStatus::kInvalidated;
  {
    auto result = this->methods->updateMetadata(info);
    ASSERT_TRUE(result.ok());
  }
  {
    auto result = this->methods->readMetadata();
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result->snapshot.status,
              replicated_state::SnapshotStatus::kInvalidated);
    EXPECT_EQ(result->stateId, this->logId);
  }
}

TYPED_TEST(StorageEngineMethodsTest, write_drop_data) {
  replicated_state::PersistedStateInfo info;
  info.stateId = this->logId;
  info.snapshot.status = replicated_state::SnapshotStatus::kCompleted;
  {
    auto result = this->methods->updateMetadata(info);
    ASSERT_TRUE(result.ok());
  }
  {
    auto result = this->methods->readMetadata();
    ASSERT_TRUE(result.ok());
  }

  this->dropAndRebuild();

  {
    auto result = this->methods->readMetadata();
    ASSERT_EQ(result.errorNumber(), TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  }
}

TYPED_TEST(StorageEngineMethodsTest, write_log_entries) {
  auto const entries = std::vector{
      PersistingLogEntry{LogTerm{1}, LogIndex{1},
                         LogPayload::createFromString("first")},
      PersistingLogEntry{LogTerm{1}, LogIndex{2},
                         LogPayload::createFromString("second")},
      PersistingLogEntry{LogTerm{2}, LogIndex{3},
                         LogPayload::createFromString("third")},
      PersistingLogEntry{LogTerm{2}, LogIndex{1000},
                         LogPayload::createFromString("thousand")},
  };

  {
    auto iter = test::make_iterator(entries);
    auto res = this->methods->insert(std::move(iter), {}).get();
    EXPECT_TRUE(res.ok());
  }

  {
    auto iter = this->methods->read(LogIndex{0});
    for (auto const& expected : entries) {
      ASSERT_EQ(iter->next(), expected);
    }
    ASSERT_EQ(iter->next(), std::nullopt);
  }
}

TYPED_TEST(StorageEngineMethodsTest, write_log_entries_remove_front_back) {
  auto const entries = std::vector{
      PersistingLogEntry{LogTerm{1}, LogIndex{1},
                         LogPayload::createFromString("first")},
      PersistingLogEntry{LogTerm{1}, LogIndex{2},
                         LogPayload::createFromString("second")},
      PersistingLogEntry{LogTerm{2}, LogIndex{3},
                         LogPayload::createFromString("third")},
      PersistingLogEntry{LogTerm{2}, LogIndex{1000},
                         LogPayload::createFromString("thousand")},
  };

  {
    auto iter = test::make_iterator(entries);
    auto res = this->methods->insert(std::move(iter), {}).get();
    EXPECT_TRUE(res.ok());
  }

  {
    auto result = this->methods->removeFront(LogIndex{2}, {}).get();
    ASSERT_TRUE(result.ok());
  }
  {
    auto result = this->methods->removeBack(LogIndex{3}, {}).get();
    ASSERT_TRUE(result.ok());
  }

  {
    auto iter = this->methods->read(LogIndex{0});
    auto next = iter->next();
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(next->logIndex(), LogIndex{2});
    EXPECT_EQ(next->logTerm(), LogTerm{1});
    ASSERT_EQ(iter->next(), std::nullopt);
  }
}

TYPED_TEST(StorageEngineMethodsTest, write_log_entries_iter_after_remove) {
  auto const entries = std::vector{
      PersistingLogEntry{LogTerm{1}, LogIndex{1},
                         LogPayload::createFromString("first")},
      PersistingLogEntry{LogTerm{1}, LogIndex{2},
                         LogPayload::createFromString("second")},
      PersistingLogEntry{LogTerm{2}, LogIndex{3},
                         LogPayload::createFromString("third")},
      PersistingLogEntry{LogTerm{2}, LogIndex{1000},
                         LogPayload::createFromString("thousand")},
  };

  {
    auto iter = test::make_iterator(entries);
    auto res = this->methods->insert(std::move(iter), {}).get();
    EXPECT_TRUE(res.ok());
  }

  // obtain iterator
  auto iter = this->methods->read(LogIndex{0});

  {
    // remove log entries
    auto result = this->methods->removeFront(LogIndex{1}, {}).get();
    ASSERT_TRUE(result.ok());
  }

  {
    // should see all log entries
    for (auto const& expected : entries) {
      ASSERT_EQ(iter->next(), expected);
    }
    ASSERT_EQ(iter->next(), std::nullopt);
  }
}
