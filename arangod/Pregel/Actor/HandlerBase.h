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

  // TODO concept that SpecificMessage is in variant of ActorConfig
  template<typename ActorMessage, typename SpecificMessage>
  auto dispatch(ActorPID receiver, SpecificMessage message) -> void {
    if (receiver.server == self.server) {
      runtime->dispatch(
          self, receiver,
          std::make_unique<MessagePayload<MessageOrError<ActorMessage>>>(
              std::move(message)));
    } else {
      auto messageOrError = MessageOrError<ActorMessage>(message);
      auto payload = inspection::serializeWithErrorT(messageOrError);
      if (payload.ok()) {
        runtime->dispatch(self, receiver, payload.get());
      } else {
        fmt::print("HandlerBase error serializing message");
        std::abort();
      }
    }
  }

  // TODO concept that InitialMessage is in variant of ActorConfig
  template<typename ActorConfig, typename InitialMessage>
  auto spawn(typename ActorConfig::State initialState,
             InitialMessage initialMessage) -> ActorID {
    return runtime->template spawn<ActorConfig>(initialState, initialMessage);
  }

  auto operator()(UnknownMessage msg) -> std::unique_ptr<State> {
    fmt::print(stderr, "Handle unknown message");
    return std::move(this->state);
    // go into error state
  }
  auto operator()(auto&& bla) -> std::unique_ptr<State> {
    fmt::print(stderr, "Catch all");
    return std::move(this->state);
  }

  void foo(UnknownMessage msg) {}
  virtual ~HandlerBase() = default;

  ActorPID self;
  ActorPID sender;
  std::unique_ptr<State> state;

 private:
  std::shared_ptr<Runtime> runtime;
};

}  // namespace arangodb::pregel::actor
