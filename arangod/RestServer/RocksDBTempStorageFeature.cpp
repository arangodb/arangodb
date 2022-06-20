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
#include "Logger/LogMacros.h"
#include "RestServer/DatabasePathFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

RocksDBTempStorageFeature::RocksDBTempStorageFeature(Server& server)
    : ArangodFeature{server, *this}, _db(nullptr), _started(false) {
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

  RocksDBEngine& rocksDBEngine = (RocksDBEngine&)(engine);
  _dbOptions = rocksDBEngine.rocksDBOptions();

  _options.create_missing_column_families = false;
  _options.create_if_missing = true;
  _options.env = _dbOptions.env;
  rocksdb::Status status = rocksdb::DB::Open(_options, dataPath(), &_db);
  // rocksdb::Status status = rocksdb::DB::Open((rocksdb::Options&)_dbOptions,
  // dataPath(), &_db);

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
  _started.store(true);
}

void RocksDBTempStorageFeature::beginShutdown() {}

void RocksDBTempStorageFeature::stop() {}

void RocksDBTempStorageFeature::unprepare() {}

void RocksDBTempStorageFeature::prepare() {
  auto& databasePathFeature = server().getFeature<DatabasePathFeature>();
  _basePath = databasePathFeature.directory();

  TRI_ASSERT(!_basePath.empty());
}

bool RocksDBTempStorageFeature::started() const noexcept {
  return _started.load(std::memory_order_relaxed);
}
