////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "VPackSortMigration.h"
#include <rocksdb/utilities/transaction_db.h>

#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {

// On dbservers, agents and single servers:
Result analyzeVPackIndexSorting(TRI_vocbase_t& vocbase, VPackBuilder& result) {
  // Just for the sake of completeness, restrict ourselves to the RocksDB
  // storage engine:
  auto& engineSelectorFeature =
      vocbase.server().getFeature<EngineSelectorFeature>();
  if (!engineSelectorFeature.isRocksDB()) {
    {
      VPackObjectBuilder guard(&result);
      guard->add("error", VPackValue(false));
      guard->add("errorCode", VPackValue(0));
      guard->add(
          "errorMessage",
          VPackValue("VPack sorting migration is unnecessary for storage "
                     "engines other than RocksDB"));
    }
    return {};
  }

  auto& engine = engineSelectorFeature.engine<RocksDBEngine>();
  auto* db = engine.db();

  using IndexType = arangodb::Index::IndexType;
  DatabaseFeature& databaseFeature =
      vocbase.server().getFeature<DatabaseFeature>();
  auto newComparator = RocksDBVPackComparator<
      arangodb::basics::VelocyPackHelper::SortingMethod::Correct>();

  // Collect problems here:
  result.clear();
  {
    VPackObjectBuilder guardO(&result);
    bool problemFound = false;
    guardO->add(VPackValue("result"));  // just write key
    {
      VPackArrayBuilder guardA(&result);

      for (auto const& name : databaseFeature.getDatabaseNames()) {
        auto database = databaseFeature.useDatabase(name);
        if (database == nullptr) {
          continue;
        }
        database->processCollections([&](LogicalCollection* collection) {
          for (auto& index : collection->getPhysical()->getReadyIndexes()) {
            if (auto type = index->type();
                type == IndexType::TRI_IDX_TYPE_PERSISTENT_INDEX ||
                type == IndexType::TRI_IDX_TYPE_HASH_INDEX ||
                type == IndexType::TRI_IDX_TYPE_SKIPLIST_INDEX ||
                type == IndexType::TRI_IDX_TYPE_MDI_PREFIXED_INDEX) {
              RocksDBIndex* rocksDBIndex =
                  dynamic_cast<RocksDBIndex*>(index.get());
              if (rocksDBIndex != nullptr) {
                // The above types will all be instances of RocksDBIndex, but
                // we check just in case!
                auto objectId = rocksDBIndex->objectId();
                auto bounds = rocksDBIndex->getBounds(objectId);
                auto columnFamily = rocksDBIndex->columnFamily();
                rocksDBIndex->compact();  // trigger a compaction, just in case

                // Note that the below iterator will automatically create a
                // RocksDB read snapshot for
                rocksdb::Slice const end = bounds.end();
                rocksdb::ReadOptions options;
                options.iterate_upper_bound =
                    &end;  // safe to use on rocksb::DB directly
                options.prefix_same_as_start = true;
                options.verify_checksums = false;
                options.fill_cache = false;
                std::unique_ptr<rocksdb::Iterator> it(
                    db->NewIterator(options, columnFamily));
                rocksdb::Slice prev_key;  // starts empty
                bool alarm = false;
                for (it->Seek(bounds.start()); it->Valid(); it->Next()) {
                  auto key = it->key();
                  if (!prev_key.empty() &&
                      newComparator.Compare(prev_key, key) > 0) {
                    alarm = true;
                    break;
                  }
                }
                if (alarm) {
                  problemFound = true;
                  {
                    VPackObjectBuilder guardO2(&result);
                    guardO2->add("database", VPackValue(name));
                    guardO2->add("collection", VPackValue(collection->name()));
                    guardO2->add("indexId", VPackValue(index->id().id()));
                    guardO2->add("indexName", VPackValue(index->name()));
                  }
                }
              }
            }
          }
        });
      }
    }

    if (!problemFound) {
      guardO->add("error", VPackValue(false));
      guardO->add("errorCode", VPackValue(0));
      guardO->add("errorMessage", VPackValue("all good with sorting order"));
    } else {
      guardO->add("error", VPackValue(true));
      guardO->add("errorCode",
                  VPackValue(TRI_ERROR_INDEX_HAS_LEGACY_SORTED_KEYS));
      guardO->add("errorMessage",
                  VPackValue("some indexes have legacy sorted keys"));
    }
  }

  // Finally, wait for all compaction jobs to have finished, this gives us
  // some level of confidence that potential bad keys and tomb stones which
  // we can no longer see in the index have been flushed out of the system.

  return {};
}

Result migrateVPackIndexSorting(VPackBuilder& result) { return {}; }

// On coordinators:
Result handleVPackSortMigrationTest(VPackBuilder& result) { return {}; }

Result handleVPackSortMigrationAction(VPackBuilder& result) { return {}; }

}  // namespace arangodb
