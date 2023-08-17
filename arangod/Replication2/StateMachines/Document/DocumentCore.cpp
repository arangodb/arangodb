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

#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"
#include "Basics/application-exit.h"

using namespace arangodb::replication2::replicated_state::document;

DocumentCore::DocumentCore(
    TRI_vocbase_t& vocbase, GlobalLogIdentifier gid,
    DocumentCoreParameters coreParameters,
    std::shared_ptr<IDocumentStateHandlersFactory> const& handlersFactory,
    LoggerContext loggerContext)
    : gid(std::move(gid)),
      loggerContext(std::move(loggerContext)),
      _vocbase(vocbase),
      _params(std::move(coreParameters)),
      _shardHandler(handlersFactory->createShardHandler(vocbase, this->gid)) {}

void DocumentCore::drop() {
  if (auto res = _shardHandler->dropAllShards(); res.fail()) {
    LOG_CTX("f3b3d", ERR, loggerContext)
        << "Failed to drop all shards: " << res;
  }
}

auto DocumentCore::getVocbase() -> TRI_vocbase_t& { return _vocbase; }

auto DocumentCore::getShardHandler()
    -> std::shared_ptr<IDocumentStateShardHandler> {
  return _shardHandler;
}
