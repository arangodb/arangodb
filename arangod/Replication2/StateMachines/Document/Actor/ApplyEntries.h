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

#include "Actor/Actor.h"
#include "Actor/ExitReason.h"
#include "Basics/application-exit.h"
#include "Replication2/LoggerContext.h"
#include "Replication2/StateMachines/Document/DocumentFollowerState.h"
#include "Replication2/StateMachines/Document/DocumentLogEntry.h"

namespace arangodb::replication2::replicated_state::document::actor {

using namespace arangodb::actor;

struct ApplyEntriesState {
  explicit ApplyEntriesState(DocumentFollowerState& state) : state(state) {}

  DocumentFollowerState& state;

  std::unique_ptr<DocumentFollowerState::EntryIterator> entries;
  futures::Promise<Result> promise;

  void applyEntries();

  friend inline auto inspect(auto& f, ApplyEntriesState& x) {
    return f.object(x).fields(f.field("type", "ApplyEntriesState"));
  }
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

struct ApplyEntriesMessages : std::variant<ApplyEntries> {
  using std::variant<ApplyEntries>::variant;

  friend inline auto inspect(auto& f, ApplyEntriesMessages& x) {
    return f.variant(x).unqualified().alternatives(
        arangodb::inspection::type<ApplyEntries>("applyEntries"));
  }
};

}  // namespace message

template<typename Runtime>
struct ApplyEntriesHandler : HandlerBase<Runtime, ApplyEntriesState> {
  using ActorPID = typename Runtime::ActorPID;

  auto operator()(message::ApplyEntries&& msg)
      -> std::unique_ptr<ApplyEntriesState> {
    this->state->entries = std::move(msg.entries);
    this->state->promise = std::move(msg.promise);
    this->state->applyEntries();
    return std::move(this->state);
  }

  auto operator()(auto&& msg) -> std::unique_ptr<ApplyEntriesState> {
    LOG_CTX("0bc2e", FATAL, this->state->state.loggerContext)
        << "ApplyEntries actor received unexpected message "
        << inspection::json(msg);
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

// TODO - make this a cpp instead of a tpp
#include "ApplyEntries.tpp"