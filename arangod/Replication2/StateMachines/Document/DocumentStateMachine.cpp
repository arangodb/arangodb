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

#include "DocumentCore.h"
#include "DocumentFollowerState.h"
#include "DocumentLeaderState.h"
#include "DocumentStateMachine.h"
#include "DocumentStateStrategy.h"

#include "Logger/LogContextKeys.h"

#include <Basics/voc-errors.h>
#include <Futures/Future.h>

using namespace arangodb::replication2::replicated_state::document;

auto DocumentCoreParameters::toSharedSlice() -> velocypack::SharedSlice {
  VPackBuilder builder;
  velocypack::serialize(builder, *this);
  return builder.sharedSlice();
}

DocumentFactory::DocumentFactory(
    std::shared_ptr<IDocumentStateAgencyHandler> agencyReader,
    std::shared_ptr<IDocumentStateShardHandler> shardHandler)
    : _agencyReader(std::move(agencyReader)),
      _shardHandler(std::move(shardHandler)){};

auto DocumentFactory::constructFollower(std::unique_ptr<DocumentCore> core)
    -> std::shared_ptr<DocumentFollowerState> {
  return std::make_shared<DocumentFollowerState>(std::move(core));
}

auto DocumentFactory::constructLeader(std::unique_ptr<DocumentCore> core)
    -> std::shared_ptr<DocumentLeaderState> {
  return std::make_shared<DocumentLeaderState>(std::move(core));
}

auto DocumentFactory::constructCore(GlobalLogIdentifier gid,
                                    DocumentCoreParameters coreParameters)
    -> std::unique_ptr<DocumentCore> {
  LoggerContext logContext =
      LoggerContext(Logger::REPLICATED_STATE)
          .with<logContextKeyStateImpl>(DocumentState::NAME)
          .with<logContextKeyDatabaseName>(gid.database)
          .with<logContextKeyCollectionId>(coreParameters.collectionId)
          .with<logContextKeyLogId>(gid.id);
  return std::make_unique<DocumentCore>(
      std::move(gid), std::move(coreParameters), getAgencyReader(),
      getShardHandler(), std::move(logContext));
}

auto DocumentFactory::getAgencyReader()
    -> std::shared_ptr<IDocumentStateAgencyHandler> {
  return _agencyReader;
};

auto DocumentFactory::getShardHandler()
    -> std::shared_ptr<IDocumentStateShardHandler> {
  return _shardHandler;
};

#include "Replication2/ReplicatedState/ReplicatedState.tpp"

template struct arangodb::replication2::replicated_state::ReplicatedState<
    DocumentState>;
