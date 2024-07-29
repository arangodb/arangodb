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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEngine/RocksDBOptionsProvider.h"

#include "Basics/VelocyPackHelper.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBPrefixExtractor.h"

#include <rocksdb/slice_transform.h>

namespace arangodb {

RocksDBOptionsProvider::RocksDBOptionsProvider()
    : _vpackCmp(
          std::make_unique<RocksDBVPackComparator<
              arangodb::basics::VelocyPackHelper::SortingMethod::Correct>>()) {}

rocksdb::Options const& RocksDBOptionsProvider::getOptions() const {
  if (!_options) {
    _options = doGetOptions();
  }
  return *_options;
}

rocksdb::BlockBasedTableOptions const& RocksDBOptionsProvider::getTableOptions()
    const {
  if (!_tableOptions) {
    _tableOptions = doGetTableOptions();
  }
  return *_tableOptions;
}

rocksdb::ColumnFamilyOptions RocksDBOptionsProvider::getColumnFamilyOptions(
    RocksDBColumnFamilyManager::Family family) const {
  rocksdb::ColumnFamilyOptions result(getOptions());

  switch (family) {
    case RocksDBColumnFamilyManager::Family::Definitions:
    case RocksDBColumnFamilyManager::Family::Invalid:
      break;

    case RocksDBColumnFamilyManager::Family::Documents:
      // in the documents column family, it is totally unexpected to not
      // find a document by local document id. that means even in the lowest
      // levels we expect to find the document when looking it up.
      result.optimize_filters_for_hits = true;
      [[fallthrough]];

    case RocksDBColumnFamilyManager::Family::PrimaryIndex:
    case RocksDBColumnFamilyManager::Family::GeoIndex:
    case RocksDBColumnFamilyManager::Family::FulltextIndex:
    case RocksDBColumnFamilyManager::Family::MdiIndex: {
      // fixed 8 byte object id prefix
      result.prefix_extractor = std::shared_ptr<rocksdb::SliceTransform const>(
          rocksdb::NewFixedPrefixTransform(RocksDBKey::objectIdSize()));
      break;
    }
    case RocksDBColumnFamilyManager::Family::ReplicatedLogs: {
      // fixed 8 byte object id prefix
      result.prefix_extractor = std::shared_ptr<rocksdb::SliceTransform const>(
          rocksdb::NewFixedPrefixTransform(RocksDBKey::objectIdSize()));
      result.enable_blob_files = true;
      result.min_blob_size = 64;  // TODO just some random value
      break;
    }

    case RocksDBColumnFamilyManager::Family::EdgeIndex: {
      result.prefix_extractor = std::make_shared<RocksDBPrefixExtractor>();
      // also use hash-search based SST file format
      rocksdb::BlockBasedTableOptions tableOptions(getTableOptions());
      tableOptions.index_type =
          rocksdb::BlockBasedTableOptions::IndexType::kHashSearch;
      result.table_factory = std::shared_ptr<rocksdb::TableFactory>(
          rocksdb::NewBlockBasedTableFactory(tableOptions));
      break;
    }
    case RocksDBColumnFamilyManager::Family::MdiVPackIndex:
    case RocksDBColumnFamilyManager::Family::VPackIndex: {
      // velocypack based index variants with custom comparator
      rocksdb::BlockBasedTableOptions tableOptions(getTableOptions());
      tableOptions.filter_policy.reset();  // intentionally no bloom filter here
      result.table_factory = std::shared_ptr<rocksdb::TableFactory>(
          rocksdb::NewBlockBasedTableFactory(tableOptions));
      result.comparator = _vpackCmp.get();
      break;
    }
  }

  // set TTL for .sst file compaction
  result.ttl = periodicCompactionTtl();

  return result;
}
}  // namespace arangodb
