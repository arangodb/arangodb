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
      : self(self), sender(sender), state{std::move(state)}, runtime(runtime){};

  template<typename ActorMessage>
  auto dispatch(ActorPID receiver, ActorMessage message) -> void {
    runtime->dispatch(self, receiver, message);
  }

  template<typename ActorMessage>
  auto dispatchDelayed(std::chrono::seconds delay, ActorPID receiver,
                       ActorMessage const& message) -> void {
    runtime->dispatchDelayed(delay, self, receiver, message);
  }

  template<typename ActorConfig>
  auto spawn(std::unique_ptr<typename ActorConfig::State> initialState,
             typename ActorConfig::Message initialMessage) -> ActorPID {
    return runtime->template spawn<ActorConfig>(std::move(initialState),
                                                initialMessage);
  }

  auto finish(ExitReason reason) -> void { runtime->finishActor(self, reason); }

 protected:
  ActorPID const self;
  ActorPID const sender;
  std::unique_ptr<State> state;

 private:
  std::shared_ptr<Runtime> runtime;
};

}  // namespace arangodb::actor
