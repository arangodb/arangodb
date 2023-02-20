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

#include "Replication2/StateMachines/Document/DocumentStateMachine.h"

#include "Replication2/StateMachines/Document/DocumentCore.h"
#include "Replication2/StateMachines/Document/DocumentFollowerState.h"
#include "Replication2/StateMachines/Document/DocumentLeaderState.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Transaction/Manager.h"

#include <Basics/voc-errors.h>
#include <Futures/Future.h>
#include <Logger/LogContextKeys.h>

using namespace arangodb::replication2::replicated_state::document;

auto DocumentCoreParameters::toSharedSlice() const -> velocypack::SharedSlice {
  VPackBuilder builder;
  velocypack::serialize(builder, *this);
  return builder.sharedSlice();
}

DocumentFactory::DocumentFactory(
    std::shared_ptr<IDocumentStateHandlersFactory> handlersFactory,
    transaction::IManager& transactionManager)
    : _handlersFactory(std::move(handlersFactory)),
      _transactionManager(transactionManager){};

auto DocumentFactory::constructFollower(std::unique_ptr<DocumentCore> core)
    -> std::shared_ptr<DocumentFollowerState> {
  return std::make_shared<DocumentFollowerState>(std::move(core),
                                                 _handlersFactory);
}

auto DocumentFactory::constructLeader(std::unique_ptr<DocumentCore> core)
    -> std::shared_ptr<DocumentLeaderState> {
  return std::make_shared<DocumentLeaderState>(
      std::move(core), _handlersFactory, _transactionManager);
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
      std::move(gid), std::move(coreParameters), _handlersFactory,
      std::move(logContext));
}

auto DocumentFactory::constructCleanupHandler()
    -> std::shared_ptr<DocumentCleanupHandler> {
  return std::make_shared<DocumentCleanupHandler>();
}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"

template struct arangodb::replication2::replicated_state::ReplicatedState<
    DocumentState>;

void DocumentCleanupHandler::drop(std::unique_ptr<DocumentCore> core) {
  core->drop();
  core.reset();
}
