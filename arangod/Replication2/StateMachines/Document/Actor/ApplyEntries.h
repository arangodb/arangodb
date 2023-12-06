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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <variant>
#include <unordered_map>
#include <unordered_set>
#include <optional>

#include "Actor/Actor.h"
#include "Actor/ExitReason.h"
#include "Actor/LocalActorPID.h"
#include "Basics/application-exit.h"
#include "Replication2/LoggerContext.h"
#include "Replication2/StateMachines/Document/DocumentFollowerState.h"
#include "Replication2/StateMachines/Document/DocumentLogEntry.h"

namespace arangodb::replication2::replicated_state::document::actor {

using namespace arangodb::actor;

struct ApplyEntriesState {
  explicit ApplyEntriesState(DocumentFollowerState& state)
      : loggerContext(state.loggerContext),  // TODO - include pid in context
        handlers(state._handlers),
        _state(state) {}

  void setBatch(std::unique_ptr<DocumentFollowerState::EntryIterator> entries,
                futures::Promise<Result> promise);
  void continueBatch();

  void releaseIndex(std::optional<LogIndex> index);

  void resign();

  friend inline auto inspect(auto& f, ApplyEntriesState& x) {
    return f.object(x).fields(f.field("type", "ApplyEntriesState"));
  }

 private:
  enum class ProcessResult {
    kContinue,
    kWaitForPendingTrx,  // indicates that the current entry cannot be processed
                         // until all pending transactions have finished
    kMoveToNextEntryAndWaitForPendingTrx  // indicates that the current entry
                                          // has been processed, but we need to
                                          // wait for all pending transactions
                                          // to finish before we we process the
                                          // next entry
  };

  struct Batch {
    Batch(std::unique_ptr<DocumentFollowerState::EntryIterator>&& entries,
          futures::Promise<Result>&& promise)
        : entries(std::move(entries)),
          promise(std::move(promise)),
          _currentEntry(this->entries->next()) {}

    std::unique_ptr<DocumentFollowerState::EntryIterator> entries;
    futures::Promise<Result> promise;

    std::optional<std::pair<LogIndex, DocumentLogEntry>> _currentEntry;
    std::optional<LogIndex> lastIndex;
  };
  void resolveBatch(Result result);

  auto processEntry(DataDefinition auto& op, LogIndex)
      -> ResultT<ProcessResult>;
  auto processEntry(ReplicatedOperation::AbortAllOngoingTrx& op, LogIndex index)
      -> ResultT<ProcessResult>;
  auto processEntry(UserTransaction auto& op, LogIndex index)
      -> ResultT<ProcessResult>;

  auto beforeApplyEntry(ModifiesUserTransaction auto const& op, LogIndex index)
      -> bool;
  auto beforeApplyEntry(ReplicatedOperation::IntermediateCommit const& op,
                        LogIndex) -> bool;
  auto beforeApplyEntry(FinishesUserTransaction auto const& op, LogIndex index)
      -> bool;

  auto applyDataDefinitionEntry(DocumentFollowerState::GuardedData& data,
                                ReplicatedOperation::DropShard const& op)
      -> Result;
  auto applyDataDefinitionEntry(DocumentFollowerState::GuardedData& data,
                                ReplicatedOperation::ModifyShard const& op)
      -> Result;
  template<class T>
  requires IsAnyOf<T, ReplicatedOperation::CreateShard,
                   ReplicatedOperation::CreateIndex,
                   ReplicatedOperation::DropIndex>
  auto applyDataDefinitionEntry(DocumentFollowerState::GuardedData& data,
                                T const& op) -> Result;

  template<class T>
  auto applyEntryAndReleaseIndex(DocumentFollowerState::GuardedData& data,
                                 T const& op) -> Result;

  LocalActorPID myPid;
  LoggerContext const loggerContext;
  Handlers const handlers;
  DocumentFollowerState& _state;
  std::unique_ptr<Batch> _batch;

  // list of currently active, i.e., still ongoing transactions.
  std::unordered_map<TransactionId, LocalActorPID> _activeTransactions;
  // list of pending transactions - these are transactions which have been
  // sent a commit message, but have not yet finished
  std::unordered_set<LocalActorPID> _pendingTransactions;

  template<typename Runtime>
  friend struct ApplyEntriesHandler;
};

namespace message {

struct ApplyEntries {
  std::unique_ptr<DocumentFollowerState::EntryIterator> entries;
  futures::Promise<Result> promise;

  friend inline auto inspect(auto& f, ApplyEntries& x) {
    std::string type = "ApplyEntries";
    return f.object(x).fields(f.field("type", type));
  }
};

struct Resign {
  friend inline auto inspect(auto& f, Resign& x) {
    std::string type = "Resign";
    return f.object(x).fields(f.field("type", type));
  }
};

struct ApplyEntriesMessages : std::variant<ApplyEntries, Resign> {
  using std::variant<ApplyEntries, Resign>::variant;

  friend inline auto inspect(auto& f, ApplyEntriesMessages& x) {
    return f.variant(x).unqualified().alternatives(
        arangodb::inspection::type<ApplyEntries>("applyEntries"),
        arangodb::inspection::type<Resign>("resign"));
  }
};

}  // namespace message

template<typename Runtime>
struct ApplyEntriesHandler : HandlerBase<Runtime, ApplyEntriesState> {
  using ActorPID = typename Runtime::ActorPID;

  auto operator()(message::ApplyEntries&& msg) noexcept
      -> std::unique_ptr<ApplyEntriesState> {
    // TODO - remove the try and make continueBatch noexcept
    try {
      this->state->myPid = this->self;
      this->state->setBatch(std::move(msg.entries), std::move(msg.promise));
      this->state->continueBatch();
    } catch (std::exception& e) {
      LOG_DEVEL_CTX(this->state->_state.loggerContext)
          << "caught exception while applying entries: " << e.what();
      throw;
    } catch (...) {
      LOG_DEVEL_CTX(this->state->_state.loggerContext)
          << "caught unknown exception while applying entries";
      throw;
    }
    return std::move(this->state);
  }

  auto operator()(message::Resign&& msg) noexcept
      -> std::unique_ptr<ApplyEntriesState> {
    this->state->resign();
    this->finish(ExitReason::kFinished);
    return std::move(this->state);
  }

  auto operator()(
      arangodb::actor::message::ActorDown<typename Runtime::ActorPID>&&
          msg) noexcept -> std::unique_ptr<ApplyEntriesState> {
    LOG_CTX("56a21", DEBUG, this->state->loggerContext)
        << "applyEntries actor received actor down message "
        << inspection::json(msg);
    if (msg.reason != ExitReason::kShutdown) {
      ADB_PROD_ASSERT(this->state->_pendingTransactions.contains(msg.actor))
          << inspection::json(this->state->_pendingTransactions) << " msg "
          << inspection::json(msg);
      ADB_PROD_ASSERT(msg.reason == ExitReason::kFinished)
          << inspection::json(msg);
    }
    this->state->_pendingTransactions.erase(msg.actor);
    if (this->state->_pendingTransactions.empty() && this->state->_batch) {
      // all pending trx finished, so we can now continuing processing the batch
      this->state->continueBatch();
    }
    return std::move(this->state);
  }

  auto operator()(auto&& msg) -> std::unique_ptr<ApplyEntriesState> {
    LOG_CTX("0bc2e", FATAL, this->state->_state.loggerContext)
        << "ApplyEntries actor received unexpected message "
        << typeid(msg).name() << " " << inspection::json(msg);
    FATAL_ERROR_EXIT();
    return std::move(this->state);
  }
};

struct ApplyEntriesActor {
  using State = ApplyEntriesState;
  using Message = message::ApplyEntriesMessages;
  template<typename Runtime>
  using Handler = ApplyEntriesHandler<Runtime>;
  static constexpr auto typeName() -> std::string_view {
    return "ApplyEntriesActor";
  };
};

}  // namespace arangodb::replication2::replicated_state::document::actor
