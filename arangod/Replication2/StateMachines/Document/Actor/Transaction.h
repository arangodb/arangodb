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

#include <type_traits>
#include <variant>

#include "Actor/Actor.h"
#include "Actor/ExitReason.h"
#include "Actor/LocalActorPID.h"
#include "Actor/LocalRuntime.h"
#include "Basics/application-exit.h"
#include "Replication2/LoggerContext.h"
#include "Replication2/StateMachines/Document/DocumentFollowerState.h"
#include "Replication2/StateMachines/Document/DocumentLogEntry.h"
#include "Replication2/StateMachines/Document/ReplicatedOperation.h"

namespace arangodb::replication2::replicated_state::document::actor {

using namespace arangodb::actor;

struct TransactionState {
  explicit TransactionState(DocumentFollowerState& state) : state(state) {}

  friend inline auto inspect(auto& f, TransactionState& x) {
    return f.object(x).fields(f.field("type", "TransactionState"));
  }

  template<class OpType>
  void maybeFinishActor(LocalActorPID pid) {
    // we also have to finish the actor in case of an intermediate
    // commit, because for the later operations in this transaction
    // we will start a separate actor
    if constexpr (FinishesUserTransaction<OpType> ||
                  std::is_same_v<OpType,
                                 ReplicatedOperation::IntermediateCommit>) {
      state._runtime->finishActor(pid, ExitReason::kFinished);
    }
  }

  void applyEntry(LocalActorPID pid, UserTransactionOperation const& op,
                  LogIndex index) {
    std::visit(
        [&](auto&& op) -> void {
          using OpType = std::remove_cvref_t<decltype(op)>;

          if (skip) {
            maybeFinishActor<OpType>(pid);
            return;
          }

          try {
            auto originalRes =
                state._handlers.transactionHandler->applyEntry(op);
            auto res =
                state._handlers.errorHandler->handleOpResult(op, originalRes);
            if (res.fail()) {
              LOG_CTX("0aa2e", FATAL, state.loggerContext)
                  << "failed to apply entry " << op << " on follower: " << res;
              FATAL_ERROR_EXIT();
            }
            if constexpr (ModifiesUserTransaction<OpType>) {
              if (originalRes.fail()) {
                state._guardedData.doUnderLock([&](auto& data) {
                  data.activeTransactions.markAsInactive(op.tid);
                });
                state._handlers.transactionHandler->removeTransaction(op.tid);
                skip = true;
                return;
              }
            }

            if constexpr (FinishesUserTransaction<OpType>) {
              state._guardedData.doUnderLock([&](auto& data) {
                data.activeTransactions.markAsInactive(op.tid);
              });
            }

            maybeFinishActor<OpType>(pid);
          } catch (std::exception& e) {
            LOG_CTX("013aa", FATAL, state.loggerContext)
                << "caught exception while applying entry " << op << ": "
                << e.what();
            FATAL_ERROR_EXIT();
          } catch (...) {
            LOG_CTX("515fc", FATAL, state.loggerContext)
                << "caught unknown exception while applying entry " << op;
            FATAL_ERROR_EXIT();
          }
        },
        op);
  }

  // private:
  DocumentFollowerState& state;

  // will be set to true if one of the modification operations fail (e.g.,
  // because the shard does not exist, or we have a unique constraint violation,
  // ...). In this case, we can conclude that we are replaying the log, and this
  // transaction has already been applied, so we can immediately remove it (and
  // thereby abort it), and skip all subsequent operations.
  bool skip = false;
};

namespace message {

struct ProcessEntry {
  UserTransactionOperation op;
  LogIndex index;

  friend inline auto inspect(auto& f, ProcessEntry& x) {
    return f.object(x).fields(f.field("op", x.op), f.field("index", x.index));
  }
};

struct TransactionMessages : std::variant<ProcessEntry> {
  using std::variant<ProcessEntry>::variant;

  friend inline auto inspect(auto& f, TransactionMessages& x) {
    return f.variant(x).unqualified().alternatives(
        arangodb::inspection::type<ProcessEntry>("processEntry"));
  }
};

}  // namespace message

template<typename Runtime>
struct TransactionHandler : HandlerBase<Runtime, TransactionState> {
  using ActorPID = typename Runtime::ActorPID;

  auto operator()(message::ProcessEntry&& msg)
      -> std::unique_ptr<TransactionState> {
    this->state->applyEntry(this->self, msg.op, msg.index);
    return std::move(this->state);
  }

  auto operator()(auto&& msg) -> std::unique_ptr<TransactionState> {
    LOG_CTX("3bc7f", FATAL, this->state->state.loggerContext)
        << "Transaction actor received unexpected message "
        << typeid(msg).name() << " " << inspection::json(msg);
    FATAL_ERROR_EXIT();
    return std::move(this->state);
  }
};

struct TransactionActor {
  using State = TransactionState;
  using Message = message::TransactionMessages;
  template<typename Runtime>
  using Handler = TransactionHandler<Runtime>;
  static constexpr auto typeName() -> std::string_view {
    return "TransactionActor";
  };
};

}  // namespace arangodb::replication2::replicated_state::document::actor
