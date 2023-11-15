////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"

#include "Replication2/StateMachines/Document/MaintenanceActionExecutor.h"
#include "VocBase/vocbase.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/VocBaseLogManager.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#ifdef USE_V8
#include "Transaction/V8Context.h"
#endif
#include "Utils/SingleCollectionTransaction.h"

namespace arangodb::replication2::replicated_state::document {

DocumentStateShardHandler::DocumentStateShardHandler(
    TRI_vocbase_t& vocbase, GlobalLogIdentifier gid,
    std::shared_ptr<IMaintenanceActionExecutor> maintenance)
    : _gid(std::move(gid)),
      _maintenance(std::move(maintenance)),
      _vocbase(vocbase) {}

auto DocumentStateShardHandler::ensureShard(
    ShardID const& shard, TRI_col_type_e collectionType,
    velocypack::SharedSlice const& properties) noexcept -> Result {
  auto res =
      _maintenance->executeCreateCollection(shard, collectionType, properties);
  std::ignore = _maintenance->addDirty();
  return res;
}

auto DocumentStateShardHandler::modifyShard(
    ShardID shard, CollectionID colId,
    velocypack::SharedSlice properties) noexcept -> Result {
  auto col = lookupShard(shard);
  if (col.fail()) {
    return {col.errorNumber(),
            fmt::format("Error while modifying shard: {}", col.errorMessage())};
  }

  auto res = _maintenance->executeModifyCollection(
      std::move(col).get(), std::move(colId), std::move(properties));
  std::ignore = _maintenance->addDirty();

  return res;
}

auto DocumentStateShardHandler::dropShard(ShardID const& shard) noexcept
    -> Result {
  auto col = lookupShard(shard);
  if (col.fail()) {
    return {col.errorNumber(),
            fmt::format("Error while dropping shard: {}", col.errorMessage())};
  }

  auto res = _maintenance->executeDropCollection(std::move(*col));
  std::ignore = _maintenance->addDirty();

  return res;
}

auto DocumentStateShardHandler::dropAllShards() noexcept -> Result {
  auto collections =
      basics::catchToResultT([&]() { return getAvailableShards(); });
  if (collections.fail()) {
    return {collections.errorNumber(),
            fmt::format("Replicated log {} failed to get available shards: {}",
                        _gid, collections.errorMessage())};
  }

  std::stringstream err;
  auto res = Result{};
  for (auto&& col : *collections) {
    if (auto r = _maintenance->executeDropCollection(col); r.fail()) {
      err << col->name() << ": " << r << ", ";
      res.reset(std::move(r));
    }
  }

  std::ignore = _maintenance->addDirty();

  if (res.fail()) {
    res.reset(
        res.errorNumber(),
        fmt::format("Replicated log {} failed to drop shards: {} (error: {})",
                    _gid, err.str(), res.errorMessage()));
  }
  return res;
}

auto DocumentStateShardHandler::getAvailableShards()
    -> std::vector<std::shared_ptr<LogicalCollection>> {
  auto collections = _vocbase.collections(false);
  std::erase_if(collections, [&](auto&& col) {
    return col->replicatedStateId() != _gid.id;
  });
  return collections;
}

auto DocumentStateShardHandler::ensureIndex(
    ShardID shard, velocypack::SharedSlice properties,
    std::shared_ptr<methods::Indexes::ProgressTracker> progress) noexcept
    -> Result {
  auto col = lookupShard(shard);
  if (col.fail()) {
    return {col.errorNumber(),
            fmt::format("Error while ensuring index: {}", col.errorMessage())};
  }

  auto res = _maintenance->executeCreateIndex(std::move(col).get(), properties,
                                              std::move(progress));
  std::ignore = _maintenance->addDirty();

  if (res.fail()) {
    res.reset(
        res.errorNumber(),
        fmt::format(
            "Error: {}! Replicated log {} failed to ensure index on shard {}! "
            "Index: {}",
            res.errorMessage(), _gid, std::move(shard), properties.toJson()));
  }
  return res;
}

auto DocumentStateShardHandler::dropIndex(
    ShardID shard, velocypack::SharedSlice index) noexcept -> Result {
  auto col = lookupShard(shard);
  if (col.fail()) {
    return {col.errorNumber(),
            fmt::format("Error while dropping index: {}", col.errorMessage())};
  }

  auto indexId = index.toString();
  auto res =
      _maintenance->executeDropIndex(std::move(col).get(), std::move(index));
  std::ignore = _maintenance->addDirty();

  if (res.fail()) {
    res = Result{res.errorNumber(),
                 fmt::format("Error: {}! Replicated log {} failed to drop "
                             "index on shard {}! Index: {}",
                             res.errorMessage(), _gid, std::move(shard),
                             std::move(indexId))};
  }
  return res;
}

auto DocumentStateShardHandler::lookupShard(ShardID const& shard) noexcept
    -> ResultT<std::shared_ptr<LogicalCollection>> {
  auto col = _vocbase.lookupCollection(shard);
  if (col == nullptr) {
    return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  fmt::format("Replicated log {} failed to lookup shard {}",
                              _gid, shard)};
  }
  return col;
}

auto DocumentStateShardHandler::lockShard(ShardID const& shard,
                                          AccessMode::Type accessType,
                                          transaction::OperationOrigin origin)
    -> ResultT<std::unique_ptr<transaction::Methods>> {
  auto col = lookupShard(shard);
  if (col.fail()) {
    return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  fmt::format("Failed to lookup shard {} in database {} while "
                              "locking it for operation {}",
                              shard, _vocbase.name(), origin.description)};
  }

#ifdef USE_V8
  auto ctx =
      transaction::V8Context::createWhenRequired(_vocbase, origin, false);
#else
  auto ctx = transaction::StandaloneContext::create(_vocbase, origin);
#endif

  // Not replicating this transaction
  transaction::Options options;
  options.requiresReplication = false;

  auto trx = std::make_unique<SingleCollectionTransaction>(
      std::move(ctx), *col.get(), accessType, options);
  Result res = trx->begin();
  if (res.fail()) {
    return Result{res.errorNumber(),
                  fmt::format("Failed to lock shard {} in database "
                              "{} for operation {}. Error: {}",
                              shard, _vocbase.name(), origin.description,
                              res.errorMessage())};
  }
  return trx;
}

}  // namespace arangodb::replication2::replicated_state::document
