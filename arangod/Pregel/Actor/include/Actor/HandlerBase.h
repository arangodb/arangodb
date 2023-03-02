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

#include "Inspection/VPackWithErrorT.h"

#include "Message.h"
#include "ActorPID.h"

namespace arangodb::pregel::actor {

template<typename Runtime, typename State>
struct HandlerBase {
  HandlerBase(ActorPID self, ActorPID sender, std::unique_ptr<State> state,
              std::shared_ptr<Runtime> runtime)
      : self(self), sender(sender), state{std::move(state)}, runtime(runtime){};

  template<typename ActorMessage>
  auto dispatch(ActorPID receiver, ActorMessage message) -> void {
    runtime->dispatch(self, receiver, message);
  }

  template<typename ActorConfig>
  auto spawn(typename ActorConfig::State initialState,
             typename ActorConfig::Message initialMessage) -> ActorID {
    return runtime->template spawn<ActorConfig>(self.database, initialState,
                                                initialMessage);
  }

  auto finish() -> void { runtime->finish(self); }

 protected:
  ActorPID self;
  ActorPID sender;
  std::unique_ptr<State> state;

 private:
  std::shared_ptr<Runtime> runtime;
};

}  // namespace arangodb::pregel::actor
