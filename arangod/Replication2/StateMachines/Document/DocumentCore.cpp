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
  _shardId = _params.shardId;
  auto shardResult = _shardHandler->createLocalShard(
      _shardId, _params.collectionId, collectionProperties);
  TRI_ASSERT(shardResult.ok()) << "Shard creation failed for replicated state"
                               << _gid << ": " << shardResult;

  LOG_CTX("b7e0d", TRACE, this->loggerContext)
      << "Created shard " << _shardId << " for replicated state " << _gid;
}

auto DocumentCore::getShardId() -> ShardID const& { return _shardId; }

auto DocumentCore::getGid() -> GlobalLogIdentifier { return _gid; }

auto DocumentCore::getCollectionId() -> std::string const& {
  return _params.collectionId;
}

void DocumentCore::drop() {
  auto result = _shardHandler->dropLocalShard(_shardId, _params.collectionId);
  if (result.fail()) {
    LOG_CTX("b7f0d", FATAL, this->loggerContext)
        << "Failed to drop shard " << _shardId << " for replicated state "
        << _gid << ": " << result;
    FATAL_ERROR_EXIT();
  }
}

auto DocumentCore::getVocbase() -> TRI_vocbase_t& { return _vocbase; }

auto DocumentCore::getVocbase() const -> TRI_vocbase_t const& {
  return _vocbase;
}
