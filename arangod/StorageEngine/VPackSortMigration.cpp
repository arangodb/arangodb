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

#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"

#include "Network/NetworkFeature.h"
#include "Network/Methods.h"

namespace arangodb {

// On dbservers, agents and single servers:
Result analyzeVPackIndexSorting(TRI_vocbase_t& vocbase, VPackBuilder& result) {
  // Just for the sake of completeness, restrict ourselves to the RocksDB
  // storage engine:
  auto& engineSelectorFeature =
      vocbase.server().getFeature<EngineSelectorFeature>();
  if (!engineSelectorFeature.isRocksDB()) {
    return Result(TRI_ERROR_NOT_IMPLEMENTED,
                  "VPack sorting migration is unnecessary for storage engines "
                  "other than RocksDB");
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
    guardO->add(VPackValue("affected"));  // just write key
    {
      VPackArrayBuilder guardA(&result);

      for (auto const& name : databaseFeature.getDatabaseNames()) {
        auto database = databaseFeature.useDatabase(name);
        if (database == nullptr) {
          continue;
        }
        LOG_TOPIC("76521", DEBUG, Logger::ENGINES)
            << "Checking VPackSortMigration for database " << name;
        database->processCollections([&](LogicalCollection* collection) {
          LOG_TOPIC("42537", DEBUG, Logger::ENGINES)
              << "Checking VPackSortMigration for collection "
              << collection->name();
          for (auto& index : collection->getPhysical()->getReadyIndexes()) {
            LOG_TOPIC("42538", DEBUG, Logger::ENGINES)
                << "Checking VPackSortMigration for index with ID "
                << index->id().id() << " and name " << index->name();
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
                LOG_TOPIC("42539", DEBUG, Logger::ENGINES)
                    << "VPackSortMigration: RocksDBIndex, objectId: "
                    << std::hex << objectId;
                auto bounds = rocksDBIndex->getBounds(objectId);
                auto columnFamily = rocksDBIndex->columnFamily();

                // Note that the below iterator will automatically create a
                // RocksDB read snapshot
                rocksdb::Slice const end = bounds.end();
                rocksdb::ReadOptions options;
                options.iterate_upper_bound =
                    &end;  // safe to use on rocksb::DB directly
                options.prefix_same_as_start = true;
                options.verify_checksums = false;
                options.fill_cache = false;
                std::unique_ptr<rocksdb::Iterator> it(
                    db->NewIterator(options, columnFamily));
                std::string prev_key;  // copy the previous key, since the
                                       // slice to which the it->key() points
                                       // is not guaranteed to keep its value!
                bool alarm = false;
                for (it->Seek(bounds.start()); it->Valid(); it->Next()) {
                  rocksdb::Slice key = it->key();
                  if (!prev_key.empty()) {
                    LOG_TOPIC("42540", TRACE, Logger::ENGINES)
                        << "VPackSortMigration: comparing keys: "
                        << key.ToString(true) << " with "
                        << rocksdb::Slice(prev_key.data(), prev_key.size())
                               .ToString(true);
                    // Note that there is an implicit conversion from
                    // std::string to rocksdb::Slice:
                    if (newComparator.Compare(prev_key, key) > 0) {
                      LOG_TOPIC("42541", WARN, Logger::ENGINES)
                          << "VPackSortMigration: found problematic key for "
                             "database "
                          << name << ", collection " << collection->name()
                          << ", index with ID " << index->id().id()
                          << " and name " << index->name();
                      alarm = true;
                      break;
                    }
                  }
                  prev_key.assign(key.data(), key.size());
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
      guardO->add(StaticStrings::Error, VPackValue(false));
      guardO->add(StaticStrings::ErrorCode, VPackValue(0));
      guardO->add(StaticStrings::ErrorMessage,
                  VPackValue("all good with sorting order"));
    } else {
      guardO->add(StaticStrings::Error, VPackValue(true));
      guardO->add(StaticStrings::ErrorCode,
                  VPackValue(TRI_ERROR_ARANGO_INDEX_HAS_LEGACY_SORTED_KEYS));
      guardO->add(StaticStrings::ErrorMessage,
                  VPackValue("some indexes have legacy sorted keys"));
    }
  }

  return {};
}

Result migrateVPackIndexSorting(TRI_vocbase_t& vocbase, VPackBuilder& result) {
  // Just for the sake of completeness, restrict ourselves to the RocksDB
  // storage engine:
  result.clear();
  auto& engineSelectorFeature =
      vocbase.server().getFeature<EngineSelectorFeature>();
  if (!engineSelectorFeature.isRocksDB()) {
    return Result(TRI_ERROR_NOT_IMPLEMENTED,
                  "VPack sorting migration is unnecessary for storage engines "
                  "other than RocksDB");
  }

  auto& engine = engineSelectorFeature.engine<RocksDBEngine>();
  Result res = engine.writeSortingFile(
      arangodb::basics::VelocyPackHelper::SortingMethod::Correct);

  if (res.ok()) {
    VPackObjectBuilder guard(&result);
    guard->add(StaticStrings::Error, VPackValue(false));
    guard->add(StaticStrings::ErrorCode, VPackValue(0));
    guard->add(StaticStrings::ErrorMessage,
               VPackValue("VPack sorting migration done."));
    return {};
  }
  return res;
}

Result statusVPackIndexSorting(TRI_vocbase_t& vocbase, VPackBuilder& result) {
  // Just for the sake of completeness, restrict ourselves to the RocksDB
  // storage engine:
  auto& engineSelectorFeature =
      vocbase.server().getFeature<EngineSelectorFeature>();
  if (!engineSelectorFeature.isRocksDB()) {
    return Result(TRI_ERROR_NOT_IMPLEMENTED,
                  "VPack sorting migration is unnecessary for storage engines "
                  "other than RocksDB");
  }

  auto& engine = engineSelectorFeature.engine<RocksDBEngine>();
  auto sortingFileMethod = engine.readSortingFile();
  {
    VPackObjectBuilder guard(&result);
    std::string next =
        sortingFileMethod == basics::VelocyPackHelper::SortingMethod::Correct
            ? "CORRECT"
            : "LEGACY";
    std::string current =
        engine.currentSortingMethod() ==
                basics::VelocyPackHelper::SortingMethod::Correct
            ? "CORRECT"
            : "LEGACY";
    guard->add("next", VPackValue(next));
    guard->add("current", VPackValue(current));
  }

  return {};
}

// On coordinators:
async<Result> fanOutRequests(TRI_vocbase_t& vocbase, fuerte::RestVerb verb,
                             std::string_view subCommand,
                             VPackBuilder& result) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());

  auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
  auto& nf = vocbase.server().getFeature<NetworkFeature>();

  auto dbs = ci.getCurrentDBServers();
  std::vector<futures::Future<network::Response>> requests;
  requests.reserve(dbs.size());

  network::RequestOptions opts;
  if (verb == fuerte::RestVerb::Get && subCommand == "check") {
    // We want to use a long timeout for the check, since going through
    // the indexes might take a while. The user is supposed to call this
    // route asynchronously anyway:
    opts.timeout = arangodb::network::Timeout(12 * 3600.0);
  }
  opts.database = vocbase.name();

  std::string path =
      absl::StrCat("/_admin/cluster/vpackSortMigration/", subCommand);
  for (auto const& server : dbs) {
    auto f = network::sendRequest(nf.pool(), "server:" + server, verb, path, {},
                                  opts);
    requests.emplace_back(std::move(f).then(
        [server](auto&& response) { return std::move(response).get(); }));
  }

  LOG_TOPIC("22535", DEBUG, Logger::ENGINES)
      << "VPackSortMigration: awaiting all results";
  auto responses = co_await futures::collectAll(requests);
  LOG_TOPIC("22536", DEBUG, Logger::ENGINES)
      << "VPackSortMigration: all servers responded";

  {
    VPackObjectBuilder b(&result);
    for (auto& resp : responses) {
      auto res = basics::catchToResultT([&] { return std::move(resp).get(); });

      {
        // result.add(VPackValue(*(server++)));
        std::string server = res.get().destination;
        if (server.starts_with("server:")) {
          server = server.substr(7);
        }
        result.add(VPackValue(server));
        if (res.fail()) {
          VPackObjectBuilder ob{&result};
          result.add(StaticStrings::Error, VPackValue(true));
          result.add(StaticStrings::ErrorMessage,
                     VPackValue(res.errorMessage()));
          result.add(StaticStrings::ErrorCode, VPackValue(res.errorNumber()));
        } else {
          result.add(res.get().slice());
        }
      }
    }
  }

  co_return {};
}

}  // namespace arangodb
