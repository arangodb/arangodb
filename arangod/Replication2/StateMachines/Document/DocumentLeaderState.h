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

#pragma once

#include "Replication2/StateMachines/Document/DocumentCore.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"

#include "Basics/UnshackledMutex.h"

#include <memory>

namespace arangodb::replication2::replicated_state::document {
struct DocumentLeaderState
    : replicated_state::IReplicatedLeaderState<DocumentState> {
  explicit DocumentLeaderState(
      std::unique_ptr<DocumentCore> core,
      std::shared_ptr<IDocumentStateHandlersFactory> handlersFactory);

  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<DocumentCore> override;

  auto recoverEntries(std::unique_ptr<EntryIterator> ptr)
      -> futures::Future<Result> override;

  auto replicateOperation(velocypack::SharedSlice payload,
                          OperationType operation, TransactionId transactionId,
                          ReplicationOptions opts) -> futures::Future<LogIndex>;

  LoggerContext const loggerContext;
  std::string_view const shardId;
  GlobalLogIdentifier const gid;

 private:
  struct GuardedData {
    explicit GuardedData(std::unique_ptr<DocumentCore> core)
        : core(std::move(core)){};
    [[nodiscard]] bool didResign() const noexcept { return core == nullptr; }

    std::unique_ptr<DocumentCore> core;
  };

  std::shared_ptr<IDocumentStateHandlersFactory> _handlersFactory;
  Guarded<GuardedData, basics::UnshackledMutex> _guardedData;
};
}  // namespace arangodb::replication2::replicated_state::document
