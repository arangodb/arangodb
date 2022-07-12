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
#include "Transaction/Manager.h"
#include "Transaction/Methods.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/ManagerFeature.h"
#include "Utils/OperationResult.h"

#include <Futures/Future.h>

using namespace arangodb::replication2::replicated_state::document;

DocumentFollowerState::DocumentFollowerState(std::unique_ptr<DocumentCore> core)
    : _guardedData(std::move(core)){};

auto DocumentFollowerState::resign() && noexcept
    -> std::unique_ptr<DocumentCore> {
  return _guardedData.doUnderLock([](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_FOLLOWER);
    }
    return std::move(data.core);
  });
}

auto DocumentFollowerState::acquireSnapshot(ParticipantId const& destination,
                                            LogIndex) noexcept
    -> futures::Future<Result> {
  return {TRI_ERROR_NO_ERROR};
}

auto DocumentFollowerState::applyEntries(
    std::unique_ptr<EntryIterator> ptr) noexcept -> futures::Future<Result> {
  auto shardId = _guardedData.doUnderLock([](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_FOLLOWER);
    }
    return data.core->getShardId();
  });

  std::vector<std::string> const readCollections{};
  std::vector<std::string> const writeCollections = {std::string(shardId)};
  std::vector<std::string> const exclusiveCollections{};

  while (auto entry = ptr->next()) {
    auto doc = entry->second;
    VPackBuilder b;
    velocypack::serialize(b, doc);
    LOG_DEVEL << "got doc " << b.toJson();
    auto tid = doc.trx;
    auto docTrx = _guardedData.doUnderLock([tid](auto& data) {
      if (data.didResign()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_FOLLOWER);
      }
      auto docTransaction = data.core->ensureTransaction(tid);
      return docTransaction;
    });
    docTrx->getState()->setWriteAccessType();
    auto ctx = std::make_shared<transaction::ManagedContext>(
        tid, docTrx->getState(), false, false, true);
    auto& options = docTrx->getState()->options();
    options.allowImplicitCollectionsForWrite = true;
    auto activeTrx = std::make_unique<transaction::Methods>(
        ctx, readCollections, writeCollections, exclusiveCollections, options);

    Result res = activeTrx->begin();
    TRI_ASSERT(res.ok());

    auto opOptions = arangodb::OperationOptions();
    if (doc.operation == OperationType::kInsert) {
      auto f = activeTrx->insertAsync(doc.shardId, doc.data.slice(), opOptions)
                   .thenValue([activeTrx = std::move(activeTrx)](
                                  arangodb::OperationResult&& opRes) {
                     return activeTrx->finishAsync(opRes.result)
                         .thenValue([opRes(std::move(opRes))](Result&& result) {
                           TRI_ASSERT(opRes.ok());
                           TRI_ASSERT(result.ok());
                         });
                   });
      f.wait();
      f.result().throwIfFailed();
    }
  }
  return {TRI_ERROR_NO_ERROR};
}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"
