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
  throttle.OnFlushBegin(nullptr, j);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  throttle.OnFlushCompleted(nullptr, j);
  for (size_t i = 0; i < 20; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    ASSERT_DOUBLE_EQ(throttle.getThrottle(), 0.);
  }

  ++j.table_properties.data_size;
  throttle.OnFlushBegin(nullptr, j);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  throttle.OnFlushCompleted(nullptr, j);
  ASSERT_DOUBLE_EQ(throttle.getThrottle(), 0.);

  uint64_t cur, last = 0;
  j.table_properties.data_size = triggerSize;
  for (size_t i = 0; i < 100; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    j.table_properties.data_size += 1000;
    throttle.OnFlushCompleted(nullptr, j);
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
  throttle.OnFlushBegin(nullptr, j);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  throttle.OnFlushCompleted(nullptr, j);
  ASSERT_DOUBLE_EQ(throttle.getThrottle(), 0.);

  ++j.table_properties.data_size;
  throttle.OnFlushBegin(nullptr, j);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  throttle.OnFlushCompleted(nullptr, j);
  ASSERT_DOUBLE_EQ(throttle.getThrottle(), 0.);

  j.table_properties.data_size = triggerSize;

  for (size_t i = 0; i < 100; ++i) {
    if (i > 0 && i % 10 == 0) {
      throttle.OnFlushBegin(nullptr, j);  // Briefly reset target speed
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
  throttle.OnFlushBegin(nullptr, j);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  throttle.OnFlushCompleted(nullptr, j);
  ASSERT_DOUBLE_EQ(throttle.getThrottle(), 0.);

  j.table_properties.data_size = (64 << 19) + 1;
  throttle.OnFlushBegin(nullptr, j);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  throttle.OnFlushCompleted(nullptr, j);

  for (size_t i = 0; i < 275; ++i) {
    fileDescriptorsCurrent(fileDescriptorsCurrent() + 5000);
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    throttle.OnFlushCompleted(nullptr, j);
    auto const cur = throttle.getThrottle();
    std::cout << cur << std::endl;
    fflush(stdout);
  }
}
