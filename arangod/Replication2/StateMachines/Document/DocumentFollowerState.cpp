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
#include "Transaction/Manager.h"

#include <Basics/Exceptions.h>
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
  while (auto entry = ptr->next()) {
    auto doc = entry->second;

    auto transactionHandler = _guardedData.doUnderLock(
        [](auto& data) -> std::shared_ptr<IDocumentStateTransactionHandler> {
          if (data.didResign()) {
            return nullptr;
          }
          return data.core->getTransactionHandler();
        });
    if (transactionHandler == nullptr) {
      return {TRI_ERROR_CLUSTER_NOT_FOLLOWER};
    }

    try {
      auto fut = futures::Future<Result>{Result{}};
      switch (doc.operation) {
        case OperationType::kInsert:
        case OperationType::kUpdate:
          transactionHandler->ensureTransaction(doc);
          if (auto res = transactionHandler->initTransaction(doc.tid);
              res.fail()) {
            THROW_ARANGO_EXCEPTION(res);
          }
          if (auto res = transactionHandler->startTransaction(doc.tid);
              res.fail()) {
            THROW_ARANGO_EXCEPTION(res);
          }
          fut = transactionHandler->applyTransaction(doc.tid);
          break;
        case OperationType::kCommit:
          fut = transactionHandler->finishTransaction(doc.tid);
          break;
        default:
          THROW_ARANGO_EXCEPTION(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION);
      }

      fut.wait();
      fut.result().throwIfFailed();
    } catch (std::exception& e) {
      VPackBuilder builder;
      velocypack::serialize(builder, doc);
      TRI_ASSERT(false) << e.what() << " " << builder.toJson();
    }
  }

  return {TRI_ERROR_NO_ERROR};
}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"
