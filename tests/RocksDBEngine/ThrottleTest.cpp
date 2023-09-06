////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "RestServer/FileDescriptorsFeature.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBPrefixExtractor.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "RocksDBEngine/Listeners/RocksDBThrottle.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "Mocks/Servers.h"

using namespace arangodb;
using namespace arangodb::metrics;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

static arangodb::ServerID const DBSERVER_ID;

using namespace rocksdb;

class CocksDB : public DB {
 public:
  CocksDB(const DBOptions& options, const std::string& dbname,
          const bool seq_per_batch = false, const bool batch_per_txn = true,
          bool read_only = false)
      : _name(dbname),
        _seq_per_batch{seq_per_batch},
        _batch_per_txn{batch_per_txn},
        _read_only{read_only} {
    _options.num_levels = 7;
    _options.hard_pending_compaction_bytes_limit = 10 * 1024 * 1024;
  }
  virtual ~CocksDB();

  Status Put(const WriteOptions& options, ColumnFamilyHandle* column_family,
             const Slice& key, const Slice& value) override {
    return Status{};
  }
  Status Put(const WriteOptions& options, ColumnFamilyHandle* column_family,
             const Slice& key, const Slice& ts, const Slice& value) override {
    return Status{};
  }
  Status Delete(const WriteOptions& options, ColumnFamilyHandle* column_family,
                const Slice& key) override {
    return Status{};
  }
  Status Delete(const WriteOptions& options, ColumnFamilyHandle* column_family,
                const Slice& key, const Slice& ts) override {
    return Status{};
  }

  Status SingleDelete(const WriteOptions& options,
                      ColumnFamilyHandle* column_family,
                      const Slice& key) override {
    return Status{};
  }
  Status SingleDelete(const WriteOptions& options,
                      ColumnFamilyHandle* column_family, const Slice& key,
                      const Slice& ts) override {
    return Status{};
  }

  Status DeleteRange(const WriteOptions& options,
                     ColumnFamilyHandle* column_family, const Slice& begin_key,
                     const Slice& end_key) override {
    return Status{};
  }
  Status DeleteRange(const WriteOptions& options,
                     ColumnFamilyHandle* column_family, const Slice& begin_key,
                     const Slice& end_key, const Slice& ts) override {
    return Status{};
  }

  virtual Status Write(const WriteOptions& options,
                       WriteBatch* updates) override {
    return Status{};
  }

  Status Merge(const WriteOptions& options, ColumnFamilyHandle* column_family,
               const Slice& key, const Slice& value) override {
    return Status{};
  }

  virtual Status Get(const ReadOptions& options,
                     ColumnFamilyHandle* column_family, const Slice& key,
                     PinnableSlice* value) override {
    return Status{};
  }

  Status GetMergeOperands(const ReadOptions& options,
                          ColumnFamilyHandle* column_family, const Slice& key,
                          PinnableSlice* merge_operands,
                          GetMergeOperandsOptions* get_merge_operands_options,
                          int* number_of_operands) override {
    return Status{};
  }

  virtual std::vector<Status> MultiGet(
      const ReadOptions& options,
      const std::vector<ColumnFamilyHandle*>& column_family,
      const std::vector<Slice>& keys,
      std::vector<std::string>* values) override {
    return std::vector<Status>{};
  }

  virtual Iterator* NewIterator(const ReadOptions& _read_options,
                                ColumnFamilyHandle* column_family) override {
    return nullptr;
  }

  virtual const Snapshot* GetSnapshot() override { return nullptr; }
  virtual void ReleaseSnapshot(const Snapshot* snapshot) override {}

  virtual Status NewIterators(
      const ReadOptions& _read_options,
      const std::vector<ColumnFamilyHandle*>& column_families,
      std::vector<Iterator*>* iterators) override {
    return Status{};
  }
  virtual bool GetProperty(ColumnFamilyHandle* column_family,
                           const Slice& property, std::string* value) override {
    return true;
  }

  virtual bool GetMapProperty(
      ColumnFamilyHandle* column_family, const Slice& property,
      std::map<std::string, std::string>* value) override {
    return true;
  }
  virtual bool GetIntProperty(ColumnFamilyHandle* column_family,
                              const Slice& property, uint64_t* value) override {
    return true;
  }
  virtual bool GetAggregatedIntProperty(const Slice& property,
                                        uint64_t* aggregated_value) override {
    return true;
  }
  virtual Status GetApproximateSizes(const SizeApproximationOptions& options,
                                     ColumnFamilyHandle* column_family,
                                     const Range* range, int n,
                                     uint64_t* sizes) override {
    return Status{};
  }
  virtual void GetApproximateMemTableStats(ColumnFamilyHandle* column_family,
                                           const Range& range,
                                           uint64_t* const count,
                                           uint64_t* const size) override {}
  virtual Status CompactRange(const CompactRangeOptions& options,
                              ColumnFamilyHandle* column_family,
                              const Slice* begin, const Slice* end) override {
    return Status{};
  }
  virtual Status SetDBOptions(
      const std::unordered_map<std::string, std::string>& options_map)
      override {
    return Status{};
  }

  virtual Status CompactFiles(
      const CompactionOptions& compact_options,
      ColumnFamilyHandle* column_family,
      const std::vector<std::string>& input_file_names, const int output_level,
      const int output_path_id = -1,
      std::vector<std::string>* const output_file_names = nullptr,
      CompactionJobInfo* compaction_job_info = nullptr) override {
    return Status{};
  }
  virtual Status PauseBackgroundWork() override { return Status{}; }
  virtual Status ContinueBackgroundWork() override { return Status{}; }

  virtual Status EnableAutoCompaction(
      const std::vector<ColumnFamilyHandle*>& column_family_handles) override {
    return Status{};
  }

  virtual void EnableManualCompaction() override {}
  virtual void DisableManualCompaction() override {}

  Status SetOptions(ColumnFamilyHandle* column_family,
                    const std::unordered_map<std::string, std::string>&
                        options_map) override {
    return Status{};
  }

  virtual int NumberLevels(ColumnFamilyHandle* column_family) override {
    return 0;
  }
  virtual int MaxMemCompactionLevel(
      ColumnFamilyHandle* column_family) override {
    return 0;
  }
  virtual int Level0StopWriteTrigger(
      ColumnFamilyHandle* column_family) override {
    return 0;
  }
  virtual const std::string& GetName() const override { return _name; }
  virtual Env* GetEnv() const override { return nullptr; }
  virtual Status Flush(const FlushOptions& options,
                       ColumnFamilyHandle* column_family) override {
    return Status{};
  }
  virtual Status Flush(
      const FlushOptions& options,
      const std::vector<ColumnFamilyHandle*>& column_families) override {
    return Status{};
  }

  virtual Status SyncWAL() override { return Status{}; }
  virtual SequenceNumber GetLatestSequenceNumber() const override {
    return SequenceNumber{0};
  }

  virtual Status DisableFileDeletions() override { return Status{}; }
  virtual Status EnableFileDeletions(bool force) override { return Status{}; }

  virtual Options GetOptions(ColumnFamilyHandle* column_family) const override {
    return Options{};
  }

  Status IncreaseFullHistoryTsLowImpl(rocksdb::ColumnFamilyData* cfd,
                                      std::string ts_low) {
    return Status{};
  }

  Status GetFullHistoryTsLow(ColumnFamilyHandle* column_family,
                             std::string* ts_low) override {
    return Status{};
  }
  virtual Status GetLiveFiles(std::vector<std::string>&,
                              uint64_t* manifest_file_size,
                              bool flush_memtable = true) override {
    return Status{};
  }
  virtual Status GetSortedWalFiles(VectorLogPtr& files) override {
    return Status{};
  }
  virtual Status GetCurrentWalFile(
      std::unique_ptr<LogFile>* current_log_file) override {
    return Status{};
  }
  virtual Status GetCreationTimeOfOldestFile(uint64_t* creation_time) override {
    return Status{};
  }

  virtual Status GetUpdatesSince(
      SequenceNumber seq_number, std::unique_ptr<TransactionLogIterator>* iter,
      const TransactionLogIterator::ReadOptions& read_options =
          TransactionLogIterator::ReadOptions()) override {
    return Status{};
  }

  virtual Status DeleteFile(std::string name) override { return Status{}; }
  virtual Status GetLiveFilesChecksumInfo(
      FileChecksumList* checksum_list) override {
    return Status{};
  }
  virtual Status GetLiveFilesStorageInfo(
      const LiveFilesStorageInfoOptions& opts,
      std::vector<LiveFileStorageInfo>* files) override {
    return Status{};
  }
  virtual Status IngestExternalFile(
      ColumnFamilyHandle* column_family,
      const std::vector<std::string>& external_files,
      const IngestExternalFileOptions& ingestion_options) override {
    return Status{};
  }
  Status IncreaseFullHistoryTsLow(ColumnFamilyHandle* column_family,
                                  std::string ts_low) override {
    return Status{};
  };
  virtual Status IngestExternalFiles(
      const std::vector<IngestExternalFileArg>& args) override {
    return Status{};
  };

  virtual Status CreateColumnFamilyWithImport(
      const ColumnFamilyOptions& options, const std::string& column_family_name,
      const ImportColumnFamilyOptions& import_options,
      const ExportImportFilesMetaData& metadata,
      ColumnFamilyHandle** handle) override {
    return Status{};
  }

  virtual Status VerifyChecksum(const ReadOptions& /*read_options*/) override {
    return Status{};
  };
  virtual Status GetDbIdentity(std::string& identity) const override {
    return Status{};
  };
  virtual Status GetDbSessionId(std::string& session_id) const override {
    return Status{};
  };
  ColumnFamilyHandle* DefaultColumnFamily() const override { return nullptr; };
  virtual Status GetPropertiesOfAllTables(
      ColumnFamilyHandle* column_family,
      TablePropertiesCollection* props) override {
    return Status{};
  };
  virtual Status GetPropertiesOfTablesInRange(
      ColumnFamilyHandle* column_family, const Range* range, std::size_t n,
      TablePropertiesCollection* props) override {
    return Status{};
  };
  virtual DBOptions GetDBOptions() const override { return _options; }
  InstrumentedMutex* mutex() { return &_mutex; }
  // InstrumentedMutex& mutex() { return _mutex; }
  Options _options;
  InstrumentedMutex _mutex;
  std::string _name;
  bool _seq_per_batch, _batch_per_txn, _read_only;
};

CocksDB::~CocksDB() {}

class ThrottleTestDBServer
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AGENCY,
                                            arangodb::LogLevel::FATAL>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER,
                                            arangodb::LogLevel::FATAL> {
 protected:
  arangodb::tests::mocks::MockDBServer server;

  ThrottleTestDBServer()
      : server(DBSERVER_ID, false),
        _db(new CocksDB(_options, "foo")),
        _mf(server.getFeature<metrics::MetricsFeature>()),
        _fileDescriptorsCurrent(*static_cast<Gauge<uint64_t>*>(
            _mf.get({"arangodb_file_descriptors_current", ""}))),
        _fileDescriptorsLimit(*static_cast<Gauge<uint64_t>*>(
            _mf.get({"arangodb_file_descriptors_limit", ""}))),
        _memoryMapsCurrent(*static_cast<Gauge<uint64_t>*>(
            _mf.get({"arangodb_memory_maps_current", ""}))),
        _memoryMapsLimit(*static_cast<Gauge<uint64_t>*>(
            _mf.get({"arangodb_memory_maps_limit", ""}))) {
    server.startFeatures();
  }

  void fileDescriptorsCurrent(uint64_t f) {
    _fileDescriptorsCurrent.store(f, std::memory_order_relaxed);
  }
  uint64_t fileDescriptorsCurrent() const {
    return _fileDescriptorsCurrent.load();
  }
  void memoryMapsCurrent(uint64_t m) {
    _memoryMapsCurrent.store(m, std::memory_order_relaxed);
  }
  uint64_t memoryMapsCurrent() const { return _memoryMapsCurrent.load(); }
  rocksdb::DB* db() { return (rocksdb::DB*)_db; }
  rocksdb::DB const* db() const { return (rocksdb::DB const*)_db; }

  DBOptions _options;
  CocksDB* _db;
  MetricsFeature& _mf;
  metrics::Gauge<uint64_t>& _fileDescriptorsCurrent;
  metrics::Gauge<uint64_t>& _fileDescriptorsLimit;
  metrics::Gauge<uint64_t>& _memoryMapsCurrent;
  metrics::Gauge<uint64_t>& _memoryMapsLimit;
};

void arangodb::FileDescriptorsFeature::updateIntervalForUnitTests(
    uint64_t interval) {
  _countDescriptorsInterval.store(interval);
}

uint64_t numSlots{120};
uint64_t frequency{100};
uint64_t scalingFactor{17};
uint64_t maxWriteRate{0};
uint64_t slowdownWritesTrigger{1};
double fileDescriptorsSlowdownTrigger{0.5};
double fileDescriptorsStopTrigger{0.9};
double memoryMapsSlowdownTrigger{0.5};
double memoryMapsStopTrigger{0.9};
uint64_t lowerBoundBps{10 * 1024 * 1024};  // 10MB/s
uint64_t triggerSize{(64 << 19) + 1};

/// @brief test table data size
TEST_F(ThrottleTestDBServer, test_database_data_size) {
  std::shared_ptr<arangodb::options::ProgramOptions> po =
      std::make_shared<arangodb::options::ProgramOptions>(
          "test", std::string(), std::string(), "path");

  auto& mf = server.getFeature<MetricsFeature>();

  auto throttle = RocksDBThrottle(
      numSlots, frequency, scalingFactor, maxWriteRate, slowdownWritesTrigger,
      fileDescriptorsSlowdownTrigger, fileDescriptorsStopTrigger,
      memoryMapsSlowdownTrigger, memoryMapsStopTrigger, lowerBoundBps, mf);

  rocksdb::FlushJobInfo j{};

  // throttle should not start yet
  j.table_properties.data_size = triggerSize - 1;
  throttle.OnFlushBegin(db(), j);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  throttle.OnFlushCompleted(db(), j);
  for (size_t i = 0; i < 20; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    ASSERT_DOUBLE_EQ(throttle.getThrottle(), 0.);
  }

  ++j.table_properties.data_size;
  throttle.OnFlushBegin(db(), j);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  throttle.OnFlushCompleted(db(), j);
  ASSERT_DOUBLE_EQ(throttle.getThrottle(), 0.);

  uint64_t cur, last = 0;
  j.table_properties.data_size = triggerSize;
  for (size_t i = 0; i < 100; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    j.table_properties.data_size += 1000;
    throttle.OnFlushCompleted(db(), j);
    cur = throttle.getThrottle();
    ASSERT_TRUE(last >= cur || last == 0);
    last = cur;
  }

  // By now we're down on the ground
  ASSERT_DOUBLE_EQ(throttle.getThrottle(), lowerBoundBps);
}

/// @brief test throttle data table size on and off
TEST_F(ThrottleTestDBServer, test_database_data_size_variable) {
  std::shared_ptr<arangodb::options::ProgramOptions> po =
      std::make_shared<arangodb::options::ProgramOptions>(
          "test", std::string(), std::string(), "path");

  auto& mf = server.getFeature<MetricsFeature>();

  auto throttle = RocksDBThrottle(
      numSlots, frequency, scalingFactor, maxWriteRate, slowdownWritesTrigger,
      fileDescriptorsSlowdownTrigger, fileDescriptorsStopTrigger,
      memoryMapsSlowdownTrigger, memoryMapsStopTrigger, lowerBoundBps, mf);

  rocksdb::FlushJobInfo j{};

  // throttle should not start yet
  j.table_properties.data_size = triggerSize - 1;
  throttle.OnFlushBegin(db(), j);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  throttle.OnFlushCompleted(nullptr, j);
  ASSERT_DOUBLE_EQ(throttle.getThrottle(), 0.);

  ++j.table_properties.data_size;
  throttle.OnFlushBegin(db(), j);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  throttle.OnFlushCompleted(nullptr, j);
  ASSERT_DOUBLE_EQ(throttle.getThrottle(), 0.);

  j.table_properties.data_size = triggerSize;

  for (size_t i = 0; i < 100; ++i) {
    if (i > 0 && i % 10 == 0) {
      throttle.OnFlushBegin(db(), j);  // Briefly reset target speed
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    throttle.OnFlushCompleted(nullptr, j);
  }

  // By now we're converged to ca. 100MB/s
  ASSERT_TRUE(throttle.getThrottle() < (triggerSize * 10 - lowerBoundBps) / 3.);
  ASSERT_TRUE(throttle.getThrottle() > (triggerSize * 10 - lowerBoundBps) / 5.);
}

TEST_F(ThrottleTestDBServer, test_file_desciptors) {
  std::shared_ptr<arangodb::options::ProgramOptions> po =
      std::make_shared<arangodb::options::ProgramOptions>(
          "test", std::string(), std::string(), "path");

  auto& mf = server.getFeature<MetricsFeature>();
  auto& fdf = server.getFeature<FileDescriptorsFeature>();
  fdf.updateIntervalForUnitTests(
      0);  // disable updating the #fd from this process.

  auto throttle = RocksDBThrottle(
      numSlots, frequency, scalingFactor, maxWriteRate, slowdownWritesTrigger,
      fileDescriptorsSlowdownTrigger, fileDescriptorsStopTrigger,
      memoryMapsSlowdownTrigger, memoryMapsStopTrigger, lowerBoundBps, mf);

  rocksdb::FlushJobInfo j{};
  j.table_properties.data_size = (64 << 19) + 1;
  fileDescriptorsCurrent(1000);

  j.table_properties.data_size = (64 << 19);
  throttle.OnFlushBegin(db(), j);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  throttle.OnFlushCompleted(nullptr, j);
  ASSERT_DOUBLE_EQ(throttle.getThrottle(), 0.);

  j.table_properties.data_size = (64 << 19) + 1;
  throttle.OnFlushBegin(db(), j);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  throttle.OnFlushCompleted(nullptr, j);

  for (size_t i = 0; i < 25; ++i) {
    fileDescriptorsCurrent(fileDescriptorsCurrent() + 5000);
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    throttle.OnFlushCompleted(nullptr, j);
    auto const cur = throttle.getThrottle();
    std::cout << cur << std::endl;
    fflush(stdout);
  }
}
