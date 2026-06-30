////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>

#include <rocksdb/options.h>
#include <rocksdb/table.h>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBOptionFeatureOptions.h"
#include "RocksDBEngine/RocksDBOptionsProvider.h"

namespace arangodb {
class AgencyFeature;
namespace options {
class ProgramOptions;
}

// This feature is used to configure RocksDB in a central place.
//
// The RocksDB-Storage-Engine and the MMFiles-Persistent-Index
// that are never activated at the same time take options set
// in this feature

class RocksDBOptionFeature final
    : public application_features::ApplicationFeature,
      public RocksDBOptionsProvider {
 public:
  static constexpr std::string_view name() noexcept { return "RocksDBOption"; }

  explicit RocksDBOptionFeature(application_features::ApplicationServer& server,
                                AgencyFeature const* agencyFeature);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override;
  void start() override;

  rocksdb::TransactionDBOptions getTransactionDBOptions() const override;
  rocksdb::ColumnFamilyOptions getColumnFamilyOptions(
      RocksDBColumnFamilyManager::Family family) const override;

  bool exclusiveWrites() const noexcept override {
    return _options.exclusiveWrites;
  }
  bool useFileLogging() const noexcept override {
    return _options.useFileLogging;
  }
  bool limitOpenFilesAtStartup() const noexcept override {
    return _options.limitOpenFilesAtStartup;
  }
  uint64_t maxTotalWalSize() const noexcept override {
    return _options.maxTotalWalSize;
  }
  uint32_t numThreadsHigh() const noexcept override {
    return _options.numThreadsHigh;
  }
  uint32_t numThreadsLow() const noexcept override {
    return _options.numThreadsLow;
  }
  uint64_t periodicCompactionTtl() const noexcept override {
    return _options.periodicCompactionTtl;
  }

 protected:
  rocksdb::Options doGetOptions() const override;
  rocksdb::BlockBasedTableOptions doGetTableOptions() const override;

 private:
  RocksDBOptionFeatureOptions _options;
  AgencyFeature const* _agencyFeature{nullptr};
};

}  // namespace arangodb
