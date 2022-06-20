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

#include "DocumentFollowerState.h"
#include "DocumentLeaderState.h"
#include "DocumentStateMachine.h"
#include "Logger/LogContextKeys.h"

#include <Futures/Future.h>

using namespace arangodb::replication2::replicated_state::document;

DocumentLeaderState::DocumentLeaderState(std::unique_ptr<DocumentCore> core)
    : loggerContext(
          core->loggerContext.with<logContextKeyStateComponent>("LeaderState")),
      collectionId(core->getCollectionId()),
      _guardedData(std::move(core)) {}

auto DocumentLeaderState::resign() && noexcept
    -> std::unique_ptr<DocumentCore> {
  return _guardedData.doUnderLock([](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
    }
    return std::move(data.core);
  });
}

auto DocumentLeaderState::recoverEntries(std::unique_ptr<EntryIterator> ptr)
    -> futures::Future<Result> {
  return {TRI_ERROR_NO_ERROR};
}

void DocumentLeaderState::replicateOperations(
    velocypack::SharedSlice payload, TRI_voc_document_operation_e operation,
    TransactionId transactionId) {
  auto opName = std::invoke([&operation]() -> std::string {
    switch (operation) {
      case TRI_VOC_DOCUMENT_OPERATION_INSERT:
        return "insert";
      case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
        return "updated";
      case TRI_VOC_DOCUMENT_OPERATION_REPLACE:
        return "replace";
      case TRI_VOC_DOCUMENT_OPERATION_REMOVE:
        return "remove";
      default:
        TRI_ASSERT(false);
    }
  });

  auto entry = DocumentLogEntry{std::string(collectionId), std::move(opName),
                                std::move(payload), transactionId};
  getStream()->insert(entry);
}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"
