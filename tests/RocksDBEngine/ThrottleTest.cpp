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

DECLARE_GAUGE(
    arangodb_file_descriptors_current, uint64_t,
    "Number of currently open file descriptors for the arangod process");
DECLARE_GAUGE(
    arangodb_file_descriptors_limit, uint64_t,
    "Limit for the number of open file descriptors for the arangod process");
DECLARE_GAUGE(arangodb_memory_maps_current, uint64_t,
              "Number of currently mapped memory pages");
DECLARE_GAUGE(
    arangodb_memory_maps_limit, uint64_t,
    "Limit for the number of memory mappings for the arangod process");

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
        _fileDescriptorsCurrent(
            server.getFeature<metrics::MetricsFeature>().add(
                arangodb_file_descriptors_current{})),
        _fileDescriptorsLimit(
            server.getFeature<metrics::MetricsFeature>().add(
                arangodb_file_descriptors_limit{})),
      _memoryMapsCurrent(
          server.getFeature<metrics::MetricsFeature>().add(
              arangodb_memory_maps_current{})),
      _memoryMapsLimit(
          server.getFeature<metrics::MetricsFeature>().add(
              arangodb_memory_maps_limit{})) {
    server.startFeatures();
  }

  void fileDescriptorsCurrent(uint64_t f) {
    _fileDescriptorsLimit.store(f, std::memory_order_relaxed);
  }
  uint64_t fileDescriptorsCurrent() const {
    return _fileDescriptorsLimit.load();
  }
  void memoryMapsCurrent(uint64_t m) {
    _memoryMapsLimit.store(m, std::memory_order_relaxed);
  }
  uint64_t memoryMapsCurrent() const {
    return _memoryMapsLimit.load();
  }

  metrics::Gauge<uint64_t>& _fileDescriptorsCurrent;
  metrics::Gauge<uint64_t>& _fileDescriptorsLimit;
  metrics::Gauge<uint64_t>& _memoryMapsCurrent;
  metrics::Gauge<uint64_t>& _memoryMapsLimit;
};

uint64_t numSlots{120};
uint64_t frequency{100};
uint64_t scalingFactor{17};
uint64_t maxWriteRate{0};
uint64_t slowdownWritesTrigger{1};
double fileDescriptorsSlowdownTrigger{512.};
double fileDescriptorsStopTrigger{1024.};
double memoryMapsSlowdownTrigger{1024.};
double memoryMapsStopTrigger{2048.};
uint64_t lowerBoundBps{10 * 1024 * 1024};  // 10MB/s

/// @brief test database
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
  j.table_properties.data_size = (64 << 19);
  throttle.OnFlushBegin(nullptr, j);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  throttle.OnFlushCompleted(nullptr, j);
  ASSERT_DOUBLE_EQ(throttle.getThrottle(), 0.);
  
  ++j.table_properties.data_size;
  throttle.OnFlushBegin(nullptr, j);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  throttle.OnFlushCompleted(nullptr, j);
  ASSERT_DOUBLE_EQ(throttle.getThrottle(), 0.);

  uint64_t last;
  for (size_t i = 0; i < 175; ++i) {
    if (i > 0 && i % 10 == 0) {
      if (j.table_properties.data_size == (64 << 19)) {
        j.table_properties.data_size = (64 << 19) + 1;
      } else {
        j.table_properties.data_size = (64 << 19);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    throttle.OnFlushCompleted(nullptr, j);
    auto const cur = throttle.getThrottle();
    if (need) {
      ASSERT_TRUE(last >= cur);
    } else {
      ASSERT_DOUBLE_EQ(last, cur);
    }
    last = cur;
  }

  // By now we're down on the ground
  ASSERT_DOUBLE_EQ(throttle.getThrottle(), lowerBoundBps);
  
}
§
