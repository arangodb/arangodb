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

#include "Replication2/StateMachines/Document/DocumentFollowerState.h"

#include "Replication2/StateMachines/Document/DocumentCore.h"
#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"
#include "Replication2/StateMachines/Document/DocumentStateNetworkHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"

#include <Basics/Exceptions.h>
#include <Futures/Future.h>

using namespace arangodb::replication2::replicated_state::document;

DocumentFollowerState::DocumentFollowerState(
    std::unique_ptr<DocumentCore> core,
    std::shared_ptr<IDocumentStateHandlersFactory> handlersFactory)
    : _networkHandler(handlersFactory->createNetworkHandler(core->getGid())),
      _transactionHandler(
          handlersFactory->createTransactionHandler(core->getGid())),
      _guardedData(std::move(core)) {}

DocumentFollowerState::~DocumentFollowerState() = default;

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
                                            LogIndex waitForIndex) noexcept
    -> futures::Future<Result> {
  return _guardedData
      .doUnderLock(
          [self = shared_from_this(), &destination, waitForIndex](
              auto& data) -> futures::Future<ResultT<velocypack::SharedSlice>> {
            if (data.didResign()) {
              return ResultT<velocypack::SharedSlice>::error(
                  TRI_ERROR_CLUSTER_NOT_FOLLOWER);
            }

            // A follower may request a snapshot before leadership has been
            // established. A retry will occur in that case.
            auto leaderInterface =
                self->_networkHandler->getLeaderInterface(destination);
            return leaderInterface->getSnapshot(waitForIndex);
          })
      .thenValue([](auto&& result) -> futures::Future<Result> {
        return result.result();
      });
}

auto DocumentFollowerState::applyEntries(
    std::unique_ptr<EntryIterator> ptr) noexcept -> futures::Future<Result> {
  return _guardedData.doUnderLock([self = shared_from_this(),
                                   ptr = std::move(ptr)](
                                      auto& data) -> futures::Future<Result> {
    if (data.didResign()) {
      return {TRI_ERROR_CLUSTER_NOT_FOLLOWER};
    }

    if (self->_transactionHandler == nullptr) {
      return Result{
          TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
          fmt::format("Transaction handler is missing from "
                      "DocumentFollowerState during applyEntries "
                      "{}! This happens if the vocbase cannot be found during "
                      "DocumentState construction.",
                      to_string(data.core->getGid()))};
    }

    while (auto entry = ptr->next()) {
      auto doc = entry->second;
      auto res = self->_transactionHandler->applyEntry(doc);
      if (res.fail()) {
        return res;
      }
    }
    return {TRI_ERROR_NO_ERROR};
  });
}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"
