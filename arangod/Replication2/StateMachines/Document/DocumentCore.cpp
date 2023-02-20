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

#include "Replication2/StateMachines/Document/DocumentCore.h"

#include "Replication2/StateMachines/Document/DocumentStateAgencyHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Basics/application-exit.h"

using namespace arangodb::replication2::replicated_state::document;

DocumentCore::DocumentCore(
    TRI_vocbase_t& vocbase, GlobalLogIdentifier gid,
    DocumentCoreParameters coreParameters,
    std::shared_ptr<IDocumentStateHandlersFactory> const& handlersFactory,
    LoggerContext loggerContext)
    : loggerContext(std::move(loggerContext)),
      _vocbase(vocbase),
      _gid(std::move(gid)),
      _params(std::move(coreParameters)),
      _agencyHandler(handlersFactory->createAgencyHandler(_gid)),
      _shardHandler(handlersFactory->createShardHandler(_gid)) {
  auto collectionProperties =
      _agencyHandler->getCollectionPlan(_params.collectionId);

  // TODO this is currently still required. once the snapshot transfer contains
  //  a list of shards that exist for this log, we can remove this.
  auto shardId = _params.shardId;
  auto shardResult = _shardHandler->createLocalShard(
      shardId, _params.collectionId, collectionProperties);
  TRI_ASSERT(shardResult.ok()) << "Shard creation failed for replicated state"
                               << _gid << ": " << shardResult;

  auto shardProperties = ShardProperties{
      .collectionId = _params.collectionId,
      .properties = std::move(collectionProperties),
  };
  _shards.emplace(shardId, std::move(shardProperties));
  LOG_CTX("b7e0d", TRACE, this->loggerContext)
      << "Created shard " << shardId << " for replicated state " << _gid;
}

auto DocumentCore::getGid() -> GlobalLogIdentifier { return _gid; }

void DocumentCore::drop() {
  if (auto result = dropAllShards(); result.fail()) {
    LOG_CTX("b7f0d", FATAL, this->loggerContext)
        << "Failed to drop all shards for replicated state: " << result;
    FATAL_ERROR_EXIT();
  }
}

auto DocumentCore::getVocbase() -> TRI_vocbase_t& { return _vocbase; }

auto DocumentCore::getVocbase() const -> TRI_vocbase_t const& {
  return _vocbase;
}

auto DocumentCore::createShard(ShardID shardId, CollectionID collectionId,
                               velocypack::SharedSlice properties) -> Result {
  // TODO remove this unnecessary copy when api is better
  auto propertiesCopy = std::make_shared<VPackBuilder>();
  propertiesCopy->add(properties.slice());

  auto result =
      _shardHandler->createLocalShard(shardId, collectionId, propertiesCopy);
  if (result.ok()) {
    auto shardProperties = ShardProperties{collectionId, propertiesCopy};
    _shards.emplace(std::move(shardId), std::move(shardProperties));
  }
  return result;
}

auto DocumentCore::dropShard(ShardID shardId, CollectionID collectionId)
    -> Result {
  if (!_shards.contains(shardId)) {
    LOG_CTX("d335b", DEBUG, loggerContext) << fmt::format(
        "Skipping dropping of shard {}, because it is already dropped.",
        shardId);
    return {};
  }
  auto result = _shardHandler->dropLocalShard(shardId, collectionId);
  if (result.ok()) {
    _shards.erase(shardId);
  }
  return result;
}

auto DocumentCore::dropAllShards() -> Result {
  for (auto it = _shards.begin(); it != _shards.end(); it = _shards.erase(it)) {
    auto const& [shardId, shardProperties] = *it;
    auto result =
        _shardHandler->dropLocalShard(shardId, shardProperties.collectionId);
    if (result.fail()) {
      return Result{result.errorNumber(),
                    fmt::format("Failed to drop shard {}: {}", shardId,
                                result.errorMessage())};
    }
  }
  return {};
}

auto DocumentCore::ensureShard(ShardID shardId, CollectionID collectionId,
                               velocypack::SharedSlice properties) -> Result {
  if (_shards.contains(shardId)) {
    LOG_CTX("69bf5", DEBUG, loggerContext) << fmt::format(
        "Skipping creation of shard {}, because it is already created.",
        shardId);
    return {};
  }
  return createShard(std::move(shardId), std::move(collectionId),
                     std::move(properties));
}

auto DocumentCore::isShardAvailable(ShardID const& shardId) -> bool {
  return _shards.contains(shardId);
}

auto DocumentCore::getShardMap() -> ShardMap const& { return _shards; }
