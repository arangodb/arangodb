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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "RestServer/arangod.h"

#include <rocksdb/db.h>
#include <rocksdb/comparator.h>
#include <rocksdb/options.h>

struct TRI_vocbase_t;

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
}  // namespace arangodb

namespace arangodb {
namespace velocypack {
class Builder;  // forward declaration
class Slice;    // forward declaration
}  // namespace velocypack
}  // namespace arangodb

namespace arangodb {

class RocksDBTempStorageFeature : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept {
    return "RocksDBTempStorage";
  }

  explicit RocksDBTempStorageFeature(Server& server);
  ~RocksDBTempStorageFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override final;
  void start() override final;
  void beginShutdown() override final;
  void stop() override final;
  void unprepare() override final;
  void prepare() override final;
  std::string dataPath() const {
    return _basePath + TRI_DIR_SEPARATOR_STR + std::string(kTempPath);
  }
  std::string databasePath(TRI_vocbase_t const* /*vocbase*/) const {
    return _basePath;
  }
  std::vector<rocksdb::ColumnFamilyHandle*>& cfHandles() { return _cfHandles; }
  rocksdb::DB* tempDB() const { return _db; }
  rocksdb::Options const& tempDBOptions() { return _options; }

 private:
  static constexpr std::string_view kTempPath = "temp-rocksdb";
  std::string _basePath;
  rocksdb::DB* _db;
  rocksdb::Options _options;

  rocksdb::DBOptions _dbOptions;
  std::unique_ptr<rocksdb::Comparator> _comp;
  std::vector<rocksdb::ColumnFamilyHandle*> _cfHandles;
};

}  // namespace arangodb
