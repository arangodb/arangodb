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

#include "Replication2/StateMachines/Document/DocumentStateMachine.h"

using namespace arangodb::replication2::replicated_state::document;

DocumentCore::DocumentCore(
    GlobalLogIdentifier gid, DocumentCoreParameters coreParameters,
    std::shared_ptr<IDocumentStateHandlersFactory> const& handlersFactory,
    LoggerContext loggerContext)
    : loggerContext(std::move(loggerContext)),
      _gid(std::move(gid)),
      _params(std::move(coreParameters)),
      _agencyHandler(handlersFactory->createAgencyHandler(_gid)),
      _shardHandler(handlersFactory->createShardHandler(_gid)) {
  auto collectionProperties =
      _agencyHandler->getCollectionPlan(_params.collectionId);

  auto shardResult = _shardHandler->createLocalShard(_params.collectionId,
                                                     collectionProperties);
  TRI_ASSERT(shardResult.ok())
      << "Shard creation failed for replicated state " << _gid;
  _shardId = shardResult.get();

  auto commResult = _agencyHandler->reportShardInCurrent(
      _params.collectionId, _shardId, collectionProperties);
  TRI_ASSERT(shardResult.ok())
      << "Failed to report shard in current for replicated state " << _gid;

  LOG_CTX("b7e0d", TRACE, this->loggerContext)
      << "Created shard " << _shardId << " for replicated state " << _gid;
}

auto DocumentCore::getShardId() -> std::string_view { return _shardId; }

auto DocumentCore::getGid() -> GlobalLogIdentifier { return _gid; }
