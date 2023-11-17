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
/// @author Julia Volmer
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <Inspection/VPackWithErrorT.h>
#include <memory>
#include <variant>

#include <velocypack/Builder.h>

#include "MPSCQueue.h"

namespace arangodb::actor {

struct MessagePayloadBase {
  virtual ~MessagePayloadBase() = default;
};

template<typename Payload>
struct MessagePayload : MessagePayloadBase {
  template<typename... Args>
  MessagePayload(Args&&... args) : payload(std::forward<Args>(args)...) {}

  Payload payload;
};
template<typename Payload, typename Inspector>
auto inspect(Inspector& f, MessagePayload<Payload>& x) {
  return f.object(x).fields(f.field("payload", x.payload));
}

namespace message {

template<typename PID>
struct UnknownMessage {
  PID sender;
  PID receiver;
};
template<typename Inspector, typename PID>
auto inspect(Inspector& f, UnknownMessage<PID>& x) {
  return f.object(x).fields(f.field("sender", x.sender),
                            f.field("receiver", x.receiver));
}

template<typename PID>
struct ActorNotFound {
  PID actor;
};
template<typename Inspector, typename PID>
auto inspect(Inspector& f, ActorNotFound<PID>& x) {
  return f.object(x).fields(f.field("actor", x.actor));
}

struct NetworkError {
  std::string message;
};
template<typename Inspector>
auto inspect(Inspector& f, NetworkError& x) {
  return f.object(x).fields(f.field("server", x.message));
}

template<typename PID>
struct ActorError
    : std::variant<UnknownMessage<PID>, ActorNotFound<PID>, NetworkError> {
  using std::variant<UnknownMessage<PID>, ActorNotFound<PID>,
                     NetworkError>::variant;
};
template<typename Inspector, typename PID>
auto inspect(Inspector& f, ActorError<PID>& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<UnknownMessage<PID>>("UnknownMessage"),
      arangodb::inspection::type<ActorNotFound<PID>>("ActorNotFound"),
      arangodb::inspection::type<NetworkError>("NetworkError"));
}

template<typename T, typename U>
struct concatenator;
template<typename... Args0, typename... Args1>
struct concatenator<std::variant<Args0...>, std::variant<Args1...>> {
  std::variant<Args0..., Args1...> item;
  concatenator(std::variant<Args0...> a) {
    std::visit([this](auto&& arg) { this->item = std::move(arg); },
               std::move(a));
  }
  concatenator(std::variant<Args1...> b) {
    std::visit([this](auto&& arg) { this->item = std::move(arg); },
               std::move(b));
  }
};

template<typename T, typename PID>
struct MessageOrError
    : concatenator<typename T::variant, typename ActorError<PID>::variant> {
  using concatenator<typename T::variant,
                     typename ActorError<PID>::variant>::concatenator;
};
}  // namespace message

}  // namespace arangodb::actor

template<typename Payload>
struct fmt::formatter<arangodb::actor::MessagePayload<Payload>>
    : arangodb::inspection::inspection_formatter {};
template<typename PID>
struct fmt::formatter<arangodb::actor::message::UnknownMessage<PID>>
    : arangodb::inspection::inspection_formatter {};
template<typename PID>
struct fmt::formatter<arangodb::actor::message::ActorNotFound<PID>>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::actor::message::NetworkError>
    : arangodb::inspection::inspection_formatter {};
