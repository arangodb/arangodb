////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "../Mocks/StorageEngineMock.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/encoding.h"
#include "Cluster/ClusterFeature.h"
#include "gtest/gtest.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "GeneralServer/AuthenticationFeature.h"
#include "MMFiles/MMFilesWalRecoverState.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBRecoveryHelper.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/FlushThread.h"
#include "V8Server/V8DealerFeature.h"
#include "velocypack/Parser.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class FlushFeatureTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  FlushFeatureTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::WARN);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::ENGINES.name(),
                                    arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(),
                                    arangodb::LogLevel::FATAL);

    features.emplace_back(new arangodb::AuthenticationFeature(server), false);  // required for ClusterFeature::prepare()
    features.emplace_back(new arangodb::ClusterFeature(server),
                          false);  // required for V8DealerFeature::prepare()
    features.emplace_back(arangodb::DatabaseFeature::DATABASE =
                              new arangodb::DatabaseFeature(server),
                          false);  // required for MMFilesWalRecoverState constructor
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);  // required for TRI_vocbase_t
    features.emplace_back(new arangodb::V8DealerFeature(server),
                          false);  // required for DatabaseFeature::createDatabase(...)

#if USE_ENTERPRISE
    features.emplace_back(new arangodb::LdapFeature(server),
                          false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first->start();
      }
    }
  }

  ~FlushFeatureTest() {
    arangodb::application_features::ApplicationServer::server = nullptr;

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f : features) {
      f.first->unprepare();
    }

    arangodb::LogTopic::setLogLevel(arangodb::Logger::ENGINES.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(FlushFeatureTest, test_WAL_recover) {
  auto* dbFeature =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
          "Database");
  ASSERT_TRUE((dbFeature));
  TRI_vocbase_t* vocbase;
  ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testDatabase", vocbase)));

  arangodb::FlushFeature feature(server);
  feature.prepare();  // register handler
  arangodb::FlushFeature::registerFlushRecoveryCallback(
      "test_fail", [](TRI_vocbase_t const&, arangodb::velocypack::Slice const&) -> arangodb::Result {
        return arangodb::Result(TRI_ERROR_INTERNAL);
      });
  arangodb::FlushFeature::registerFlushRecoveryCallback(
      "test_pass", [](TRI_vocbase_t const&, arangodb::velocypack::Slice const&) -> arangodb::Result {
        return arangodb::Result();
      });

  // non-object body (MMFiles)
  {
    auto json = arangodb::velocypack::Parser::fromJson("[]");
    std::basic_string<uint8_t> buf;
    buf.resize(sizeof(::MMFilesMarker) + sizeof(TRI_voc_tick_t));  // reserve space for header
    arangodb::encoding::storeNumber(&buf[sizeof(::MMFilesMarker)],
                                    TRI_voc_tick_t(1), sizeof(TRI_voc_tick_t));
    buf.append(json->slice().begin(), json->slice().byteSize());
    auto* marker = reinterpret_cast<MMFilesMarker*>(&buf[0]);
    marker->setSize(static_cast<uint32_t>(buf.size()));
    marker->setType(::MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC);
    arangodb::MMFilesWalRecoverState state(false);
    EXPECT_TRUE((0 == state.errorCount));
    EXPECT_TRUE((arangodb::MMFilesWalRecoverState::ReplayMarker(marker, &state, nullptr)));
    EXPECT_TRUE((1 == state.errorCount));
  }

  // non-object body (RocksDB)
  {
    auto json = arangodb::velocypack::Parser::fromJson("[]");
    std::string buf;
    buf.push_back(static_cast<char>(arangodb::RocksDBLogType::FlushSync));
    arangodb::rocksutils::setRocksDBKeyFormatEndianess(arangodb::RocksDBEndianness::Big);  // required for uint64ToPersistent(...)
    arangodb::rocksutils::uint64ToPersistent(buf, 1);
    buf.append(json->slice().startAs<char>(), json->slice().byteSize());
    rocksdb::Slice marker(buf);
    size_t throwCount = 0;

    for (auto& helper : arangodb::RocksDBEngine::recoveryHelpers()) {  // one of them is for FlushFeature
      try {
        helper->LogData(marker);  // will throw on error
      } catch (...) {
        ++throwCount;
      }
    }

    EXPECT_TRUE((1 == throwCount));
  }

  // missing type (MMFiles)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    std::basic_string<uint8_t> buf;
    buf.resize(sizeof(::MMFilesMarker) + sizeof(TRI_voc_tick_t));  // reserve space for header
    arangodb::encoding::storeNumber(&buf[sizeof(::MMFilesMarker)],
                                    TRI_voc_tick_t(1), sizeof(TRI_voc_tick_t));
    buf.append(json->slice().begin(), json->slice().byteSize());
    auto* marker = reinterpret_cast<MMFilesMarker*>(&buf[0]);
    marker->setSize(static_cast<uint32_t>(buf.size()));
    marker->setType(::MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC);
    arangodb::MMFilesWalRecoverState state(false);
    EXPECT_TRUE((0 == state.errorCount));
    EXPECT_TRUE((arangodb::MMFilesWalRecoverState::ReplayMarker(marker, &state, nullptr)));
    EXPECT_TRUE((1 == state.errorCount));
  }

  // missing type (RocksDB)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    std::string buf;
    buf.push_back(static_cast<char>(arangodb::RocksDBLogType::FlushSync));
    arangodb::rocksutils::setRocksDBKeyFormatEndianess(arangodb::RocksDBEndianness::Big);  // required for uint64ToPersistent(...)
    arangodb::rocksutils::uint64ToPersistent(buf, 1);
    buf.append(json->slice().startAs<char>(), json->slice().byteSize());
    rocksdb::Slice marker(buf);
    size_t throwCount = 0;

    for (auto& helper : arangodb::RocksDBEngine::recoveryHelpers()) {  // one of them is for FlushFeature
      try {
        helper->LogData(marker);  // will throw on error
      } catch (...) {
        ++throwCount;
      }
    }

    EXPECT_TRUE((1 == throwCount));
  }

  // non-string type (MMFiles)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"type\": 42 }");
    std::basic_string<uint8_t> buf;
    buf.resize(sizeof(::MMFilesMarker) + sizeof(TRI_voc_tick_t));  // reserve space for header
    arangodb::encoding::storeNumber(&buf[sizeof(::MMFilesMarker)],
                                    TRI_voc_tick_t(1), sizeof(TRI_voc_tick_t));
    buf.append(json->slice().begin(), json->slice().byteSize());
    auto* marker = reinterpret_cast<MMFilesMarker*>(&buf[0]);
    marker->setSize(static_cast<uint32_t>(buf.size()));
    marker->setType(::MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC);
    arangodb::MMFilesWalRecoverState state(false);
    EXPECT_TRUE((0 == state.errorCount));
    EXPECT_TRUE((arangodb::MMFilesWalRecoverState::ReplayMarker(marker, &state, nullptr)));
    EXPECT_TRUE((1 == state.errorCount));
  }

  // non-string type (RocksDB)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"type\": 42 }");
    std::string buf;
    buf.push_back(static_cast<char>(arangodb::RocksDBLogType::FlushSync));
    arangodb::rocksutils::setRocksDBKeyFormatEndianess(arangodb::RocksDBEndianness::Big);  // required for uint64ToPersistent(...)
    arangodb::rocksutils::uint64ToPersistent(buf, 1);
    buf.append(json->slice().startAs<char>(), json->slice().byteSize());
    rocksdb::Slice marker(buf);
    size_t throwCount = 0;

    for (auto& helper : arangodb::RocksDBEngine::recoveryHelpers()) {  // one of them is for FlushFeature
      try {
        helper->LogData(marker);  // will throw on error
      } catch (...) {
        ++throwCount;
      }
    }

    EXPECT_TRUE((1 == throwCount));
  }

  // missing type handler (MMFiles)
  {
    auto json =
        arangodb::velocypack::Parser::fromJson("{ \"type\": \"test\" }");
    std::basic_string<uint8_t> buf;
    buf.resize(sizeof(::MMFilesMarker) + sizeof(TRI_voc_tick_t));  // reserve space for header
    arangodb::encoding::storeNumber(&buf[sizeof(::MMFilesMarker)],
                                    TRI_voc_tick_t(1), sizeof(TRI_voc_tick_t));
    buf.append(json->slice().begin(), json->slice().byteSize());
    auto* marker = reinterpret_cast<MMFilesMarker*>(&buf[0]);
    marker->setSize(static_cast<uint32_t>(buf.size()));
    marker->setType(::MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC);
    arangodb::MMFilesWalRecoverState state(false);
    EXPECT_TRUE((0 == state.errorCount));
    EXPECT_TRUE((arangodb::MMFilesWalRecoverState::ReplayMarker(marker, &state, nullptr)));
    EXPECT_TRUE((1 == state.errorCount));
  }

  // missing type handler (RocksDB)
  {
    auto json =
        arangodb::velocypack::Parser::fromJson("{ \"type\": \"test\" }");
    std::string buf;
    buf.push_back(static_cast<char>(arangodb::RocksDBLogType::FlushSync));
    arangodb::rocksutils::setRocksDBKeyFormatEndianess(arangodb::RocksDBEndianness::Big);  // required for uint64ToPersistent(...)
    arangodb::rocksutils::uint64ToPersistent(buf, 1);
    buf.append(json->slice().startAs<char>(), json->slice().byteSize());
    rocksdb::Slice marker(buf);
    size_t throwCount = 0;

    for (auto& helper : arangodb::RocksDBEngine::recoveryHelpers()) {  // one of them is for FlushFeature
      try {
        helper->LogData(marker);  // will throw on error
      } catch (...) {
        ++throwCount;
      }
    }

    EXPECT_TRUE((1 == throwCount));
  }

  // missing vocbase (MMFiles)
  {
    auto json =
        arangodb::velocypack::Parser::fromJson("{ \"type\": \"test_pass\" }");
    std::basic_string<uint8_t> buf;
    buf.resize(sizeof(::MMFilesMarker) + sizeof(TRI_voc_tick_t));  // reserve space for header
    arangodb::encoding::storeNumber(&buf[sizeof(::MMFilesMarker)],
                                    TRI_voc_tick_t(42), sizeof(TRI_voc_tick_t));
    buf.append(json->slice().begin(), json->slice().byteSize());
    auto* marker = reinterpret_cast<MMFilesMarker*>(&buf[0]);
    marker->setSize(static_cast<uint32_t>(buf.size()));
    marker->setType(::MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC);
    arangodb::MMFilesWalRecoverState state(false);
    EXPECT_TRUE((0 == state.errorCount));
    EXPECT_TRUE((arangodb::MMFilesWalRecoverState::ReplayMarker(marker, &state, nullptr)));
    EXPECT_TRUE((1 == state.errorCount));
  }

  // missing vocbase (RocksDB)
  {
    auto json =
        arangodb::velocypack::Parser::fromJson("{ \"type\": \"test_pass\" }");
    std::string buf;
    buf.push_back(static_cast<char>(arangodb::RocksDBLogType::FlushSync));
    arangodb::rocksutils::setRocksDBKeyFormatEndianess(arangodb::RocksDBEndianness::Big);  // required for uint64ToPersistent(...)
    arangodb::rocksutils::uint64ToPersistent(buf, 42);
    buf.append(json->slice().startAs<char>(), json->slice().byteSize());
    rocksdb::Slice marker(buf);
    size_t throwCount = 0;

    for (auto& helper : arangodb::RocksDBEngine::recoveryHelpers()) {  // one of them is for FlushFeature
      try {
        helper->LogData(marker);  // will throw on error
      } catch (...) {
        ++throwCount;
      }
    }

    EXPECT_TRUE((1 == throwCount));
  }

  // type handler processing fail (MMFiles)
  {
    auto json =
        arangodb::velocypack::Parser::fromJson("{ \"type\": \"test_fail\" }");
    std::basic_string<uint8_t> buf;
    buf.resize(sizeof(::MMFilesMarker) + sizeof(TRI_voc_tick_t));  // reserve space for header
    arangodb::encoding::storeNumber(&buf[sizeof(::MMFilesMarker)],
                                    TRI_voc_tick_t(1), sizeof(TRI_voc_tick_t));
    buf.append(json->slice().begin(), json->slice().byteSize());
    auto* marker = reinterpret_cast<MMFilesMarker*>(&buf[0]);
    marker->setSize(static_cast<uint32_t>(buf.size()));
    marker->setType(::MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC);
    arangodb::MMFilesWalRecoverState state(false);
    EXPECT_TRUE((0 == state.errorCount));
    EXPECT_TRUE((arangodb::MMFilesWalRecoverState::ReplayMarker(marker, &state, nullptr)));
    EXPECT_TRUE((1 == state.errorCount));
  }

  // type handler processing fail (RocksDB)
  {
    auto json =
        arangodb::velocypack::Parser::fromJson("{ \"type\": \"test_fail\" }");
    std::string buf;
    buf.push_back(static_cast<char>(arangodb::RocksDBLogType::FlushSync));
    arangodb::rocksutils::setRocksDBKeyFormatEndianess(arangodb::RocksDBEndianness::Big);  // required for uint64ToPersistent(...)
    arangodb::rocksutils::uint64ToPersistent(buf, 1);
    buf.append(json->slice().startAs<char>(), json->slice().byteSize());
    rocksdb::Slice marker(buf);
    size_t throwCount = 0;

    for (auto& helper : arangodb::RocksDBEngine::recoveryHelpers()) {  // one of them is for FlushFeature
      try {
        helper->LogData(marker);  // will throw on error
      } catch (...) {
        ++throwCount;
      }
    }

    EXPECT_TRUE((1 == throwCount));
  }

  // type handler processing pass (MMFiles)
  {
    auto json =
        arangodb::velocypack::Parser::fromJson("{ \"type\": \"test_pass\" }");
    std::basic_string<uint8_t> buf;
    buf.resize(sizeof(::MMFilesMarker) + sizeof(TRI_voc_tick_t));  // reserve space for header
    arangodb::encoding::storeNumber(&buf[sizeof(::MMFilesMarker)],
                                    TRI_voc_tick_t(1), sizeof(TRI_voc_tick_t));
    buf.append(json->slice().begin(), json->slice().byteSize());
    auto* marker = reinterpret_cast<MMFilesMarker*>(&buf[0]);
    marker->setSize(static_cast<uint32_t>(buf.size()));
    marker->setType(::MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC);
    arangodb::MMFilesWalRecoverState state(false);
    EXPECT_TRUE((0 == state.errorCount));
    EXPECT_TRUE((arangodb::MMFilesWalRecoverState::ReplayMarker(marker, &state, nullptr)));
    EXPECT_TRUE((0 == state.errorCount));
  }

  // type handler processing pass (MMFiles)
  {
    auto json =
        arangodb::velocypack::Parser::fromJson("{ \"type\": \"test_pass\" }");
    std::string buf;
    buf.push_back(static_cast<char>(arangodb::RocksDBLogType::FlushSync));
    arangodb::rocksutils::setRocksDBKeyFormatEndianess(arangodb::RocksDBEndianness::Big);  // required for uint64ToPersistent(...)
    arangodb::rocksutils::uint64ToPersistent(buf, 1);
    buf.append(json->slice().startAs<char>(), json->slice().byteSize());
    rocksdb::Slice marker(buf);
    size_t throwCount = 0;

    for (auto& helper : arangodb::RocksDBEngine::recoveryHelpers()) {  // one of them is for FlushFeature
      try {
        helper->LogData(marker);  // will throw on error
      } catch (...) {
        ++throwCount;
      }
    }

    EXPECT_TRUE((0 == throwCount));
  }
}

TEST_F(FlushFeatureTest, test_subscription_retention) {
  auto* dbFeature =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
          "Database");
  ASSERT_TRUE((dbFeature));
  TRI_vocbase_t* vocbase;
  ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testDatabase", vocbase)));
  ASSERT_NE(nullptr, vocbase);

  arangodb::FlushFeature feature(server);
  feature.prepare();

  {
    auto subscription = feature.registerFlushSubscription("subscription", *vocbase);
    ASSERT_NE(nullptr, subscription);

    size_t removed = 42;
    feature.releaseUnusedTicks(removed);
    ASSERT_EQ(0, removed); // reference is being held
  }

  size_t removed = 42;
  feature.releaseUnusedTicks(removed);
  ASSERT_EQ(1, removed); // stale subscription was removed
}
