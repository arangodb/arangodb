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

#include "Indexes/Index.h"
#include "RestServer/DatabaseFeature.h"
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

  using IndexType = arangodb::Index::IndexType;
  DatabaseFeature& databaseFeature =
      vocbase.server().getFeature<DatabaseFeature>();
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
          RocksDBIndex* rocksDBIndex = dynamic_cast<RocksDBIndex*>(index.get());
          if (rocksDBIndex != nullptr) {
            // The above types will all be instances of RocksDBIndex, but
            // we check just in case!
            auto objectId = rocksDBIndex->objectId();
            auto bounds = rocksDBIndex->getBounds(objectId);
          }
        }
      }
    });
  }

  return {};
}

Result migrateVPackIndexSorting(VPackBuilder& result) { return {}; }

// On coordinators:
Result handleVPackSortMigrationTest(VPackBuilder& result) { return {}; }

Result handleVPackSortMigrationAction(VPackBuilder& result) { return {}; }

}  // namespace arangodb
