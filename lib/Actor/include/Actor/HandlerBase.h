////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Markus Pfeiffer
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <chrono>
#include "Inspection/VPackWithErrorT.h"

#include "Message.h"

namespace arangodb::actor {

template<typename Runtime, typename State>
struct HandlerBase {
  using ActorPID = typename Runtime::ActorPID;
  HandlerBase(ActorPID self, ActorPID sender, std::unique_ptr<State> state,
              std::shared_ptr<Runtime> runtime)
      : self(self),
        sender(sender),
        state{std::move(state)},
        _runtime(runtime){};

  template<typename ActorMessage>
  auto dispatch(ActorPID receiver, ActorMessage&& message) -> void {
    _runtime->dispatch(self, receiver, std::forward<ActorMessage>(message));
  }

  template<typename ActorMessage>
  auto dispatchDelayed(std::chrono::seconds delay, ActorPID receiver,
                       ActorMessage&& message) -> void {
    _runtime->dispatchDelayed(delay, self, receiver,
                              std::forward<ActorMessage>(message));
  }

  template<typename ActorConfig>
  auto spawn(std::unique_ptr<typename ActorConfig::State> initialState)
      -> ActorPID {
    return _runtime->template spawn<ActorConfig>(std::move(initialState));
  }

  template<typename ActorConfig>
  auto spawn(std::unique_ptr<typename ActorConfig::State> initialState,
             typename ActorConfig::Message initialMessage) -> ActorPID {
    return _runtime->template spawn<ActorConfig>(std::move(initialState),
                                                 std::move(initialMessage));
  }

  auto finish(ExitReason reason) -> void {
    _runtime->finishActor(self, reason);
  }

  auto monitor(typename Runtime::ActorPID pid) -> void {
    _runtime->monitorActor(self, pid);
  }

 protected:
  ActorPID const self;
  ActorPID const sender;
  std::unique_ptr<State> state;

  auto runtime() -> Runtime& { return *_runtime; }

 private:
  std::shared_ptr<Runtime> _runtime;
};

}  // namespace arangodb::actor
