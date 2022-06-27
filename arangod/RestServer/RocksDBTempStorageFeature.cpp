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
/// @author Dr. Frank Celler
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBTempStorageFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "Basics/FileUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabasePathFeature.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

#include <rocksdb/slice_transform.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace {
void cleanUpTempStorageFiles(std::string const& path) {
  for (auto const& fileName : TRI_FullTreeDirectory(path.data())) {
    TRI_UnlinkFile(basics::FileUtils::buildFilename(path, fileName).data());
  }
}
}  // namespace

namespace arangodb {
class TwoPartComparator : public rocksdb::Comparator {
 public:
  TwoPartComparator() = default;
  int Compare(rocksdb::Slice const& lhs,
              rocksdb::Slice const& rhs) const override {
    int diff = memcmp(lhs.data(), rhs.data(), sizeof(uint64_t));
    if (diff != 0) {
      return diff;
    }
    auto p1 = lhs.data() + sizeof(uint64_t);
    auto p2 = rhs.data() + sizeof(uint64_t);

    int diffInId = 0;
    bool hasPrefixId1 =
        (static_cast<size_t>(p1 - lhs.data()) < lhs.size()) ? true : false;
    bool hasPrefixId2 =
        (static_cast<size_t>(p2 - rhs.data()) < rhs.size()) ? true : false;
    if (hasPrefixId1 && hasPrefixId2) {
      diffInId = memcmp(p1, p2, sizeof(uint64_t));
      p1 += sizeof(uint64_t);
      p2 += sizeof(uint64_t);
    } else if (hasPrefixId1) {
      diffInId = 1;
    } else if (hasPrefixId2) {
      diffInId = -1;
    }
    while (static_cast<size_t>(p1 - lhs.data()) < lhs.size() &&
           static_cast<size_t>(p2 - rhs.data()) < rhs.size()) {
      arangodb::velocypack::Slice slice1(reinterpret_cast<uint8_t const*>(p1));
      p1 += slice1.byteSize();
      arangodb::velocypack::Slice slice2(reinterpret_cast<uint8_t const*>(p2));
      p2 += slice2.byteSize();
      char order1 = *p1;
      char order2 = *p2;
      TRI_ASSERT(order1 == order2);

      diff = basics::VelocyPackHelper::compare(slice1, slice2, true);

      if (diff != 0) {
        return (order1 == '1') ? diff : -diff;
      }
      ++p1;
      ++p2;
    }

    return diffInId;
  }

  // Ignore the following methods for now:
  const char* Name() const override { return "TwoPartComparator"; }
  void FindShortestSeparator(std::string*,
                             const rocksdb::Slice&) const override {}
  void FindShortSuccessor(std::string*) const override {}
};
}  // namespace arangodb

RocksDBTempStorageFeature::RocksDBTempStorageFeature(Server& server)
    : ArangodFeature{server, *this},
      _db(nullptr),
      _comp(std::make_unique<arangodb::TwoPartComparator>()) {
  startsAfter<BasicFeaturePhaseServer>();

  startsAfter<AuthenticationFeature>();
  startsAfter<CacheManagerFeature>();
  startsAfter<EngineSelectorFeature>();
  startsAfter<RocksDBOptionFeature>();
  startsAfter<LanguageFeature>();
  startsAfter<LanguageCheckFeature>();
  startsAfter<InitDatabaseFeature>();
  startsAfter<StorageEngineFeature>();
  startsAfter<RocksDBEngine>();
}

RocksDBTempStorageFeature::~RocksDBTempStorageFeature() {}

void RocksDBTempStorageFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("temp-rocksdb-storage", "temp rocksdb storage options");
}

void RocksDBTempStorageFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {}

void RocksDBTempStorageFeature::start() {
  TRI_ASSERT(server().getFeature<EngineSelectorFeature>().isRocksDB());
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  std::string path(dataPath());
  cleanUpTempStorageFiles(path);

  RocksDBEngine& rocksDBEngine = static_cast<RocksDBEngine&>(engine);
  _dbOptions = rocksDBEngine.rocksDBOptions();
  _options.create_missing_column_families = true;
  _options.create_if_missing = true;
  _options.env = rocksdb::Env::Default();

#ifdef USE_ENTERPRISE
  rocksDBEngine.configureEnterpriseRocksDBOptions(_options, true);
#endif
  _options.prefix_extractor.reset(
      rocksdb::NewFixedPrefixTransform(sizeof(uint64_t)));

  std::vector<rocksdb::ColumnFamilyDescriptor> columnFamilies;

  rocksdb::ColumnFamilyOptions cfOptions;
  cfOptions.comparator = _comp.get();
  columnFamilies.emplace_back(
      rocksdb::ColumnFamilyDescriptor("SortCF", cfOptions));
  columnFamilies.emplace_back(rocksdb::ColumnFamilyDescriptor(
      rocksdb::kDefaultColumnFamilyName, rocksdb::ColumnFamilyOptions()));

  rocksdb::Status status =
      rocksdb::DB::Open(_options, path, columnFamilies, &_cfHandles, &_db);

  if (!status.ok()) {
    std::string error;
    if (status.IsIOError()) {
      error =
          "; Maybe your filesystem doesn't provide required features? (Cifs? "
          "NFS?)";
    }

    LOG_TOPIC("58b44", FATAL, arangodb::Logger::STARTUP)
        << "unable to initialize RocksDB engine: " << status.ToString()
        << error;
    FATAL_ERROR_EXIT();
  }
}

void RocksDBTempStorageFeature::beginShutdown() {}

void RocksDBTempStorageFeature::stop() {}

void RocksDBTempStorageFeature::unprepare() {}

void RocksDBTempStorageFeature::prepare() {
  auto& databasePathFeature = server().getFeature<DatabasePathFeature>();
  _basePath = databasePathFeature.directory();

  TRI_ASSERT(!_basePath.empty());
}