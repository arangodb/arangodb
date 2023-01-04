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

#include <string>
#include <cstdint>
#include <memory>
#include <vector>

#include "Actor.h"
#include "Dispatcher.h"

namespace arangodb::pregel::actor {

template<CallableOnFunction Scheduler>
struct Runtime : std::enable_shared_from_this<Runtime<Scheduler>> {
  Runtime() = delete;
  Runtime(Runtime const&) = delete;
  Runtime(Runtime&&) = delete;
  Runtime(ServerID myServerID, std::string runtimeID,
          std::shared_ptr<Scheduler> scheduler,
          ExternalDispatcher externalDispatcher)
      : myServerID(myServerID),
        runtimeID(runtimeID),
        scheduler(scheduler),
        dispatcher(std::make_shared<Dispatcher>(myServerID, actors,
                                                externalDispatcher)) {}

  template<typename ActorConfig>
  auto spawn(typename ActorConfig::State initialState,
             typename ActorConfig::Message initialMessage) -> ActorID {
    auto newId = ActorID{
        uniqueActorIDCounter++};  // TODO: check whether this is what we want

    auto address = ActorPID{.server = myServerID, .id = newId};

    auto newActor = std::make_unique<Actor<Runtime, ActorConfig>>(
        address, this->shared_from_this(),
        std::make_unique<typename ActorConfig::State>(initialState));
    actors.emplace(newId, std::move(newActor));

    // Send initial message to newly created actor
    auto initialPayload =
        std::make_unique<MessagePayload<typename ActorConfig::Message>>(
            initialMessage);
    (*dispatcher)(address, address, std::move(initialPayload));

    return newId;
  }

  auto shutdown() -> void {
    // TODO
  }

  auto getActorIDs() -> std::vector<ActorID> {
    auto res = std::vector<ActorID>{};

    for (auto& [id, _] : actors) {
      res.push_back(id);
    }

    return res;
  }

  template<typename ActorConfig>
  auto getActorStateByID(ActorID id)
      -> std::optional<typename ActorConfig::State> {
    if (actors.contains(id)) {
      auto* actor =
          dynamic_cast<Actor<Runtime, ActorConfig>*>(actors[id].get());
      if (actor != nullptr && actor->state != nullptr) {
        return *actor->state;
      }
    }
    return std::nullopt;
  }

  auto getSerializedActorByID(ActorID id)
      -> std::optional<velocypack::SharedSlice> {
    if (actors.contains(id)) {
      auto& actor = actors[id];
      if (actor != nullptr) {
        return actor->serialize();
      }
    }
    return std::nullopt;
  }

  auto process(ActorPID sender, ActorPID receiver, velocypack::SharedSlice msg)
      -> void {
    if (receiver.server != myServerID) {
      fmt::print(stderr, "received message for receiver {}, this is not me: {}",
                 receiver, myServerID);
      std::abort();
    }

    auto a = actors.find(receiver.id);
    if (a == std::end(actors)) {
      fmt::print(
          stderr,
          "received message for receiver {}, but the actor could not be found.",
          receiver);
      std::abort();
    }

    auto& [_, actor] = *a;
    actor->process(sender, msg);
  }

  auto preDispatch(ActorPID receiver) -> ActorMap::mapped_type& {
    if (receiver.server != myServerID) {
      std::abort();
      // TODO
    }
    auto a = actors.find(receiver.id);
    if (a == std::end(actors)) {
      std::abort();
    }
    auto& [_, actor] = *a;
    return actor;
  }

  auto dispatch(ActorPID sender, ActorPID receiver,
                std::unique_ptr<MessagePayloadBase> payload) -> void {
    auto& actor = preDispatch(receiver);
    actor->process(sender, std::move(payload));
  }
  auto dispatch(ActorPID sender, ActorPID receiver, velocypack::SharedSlice msg)
      -> void {
    if (receiver.server == myServerID) {
      auto& actor = preDispatch(receiver);
      actor->process(sender, msg);
    } else {
      // TODO
      std::abort();
      // externalDispatcher.send(sender, receiver, msg);
    }
  }

  ServerID myServerID;
  std::string const runtimeID;

  std::shared_ptr<Scheduler> scheduler;
  std::shared_ptr<Dispatcher> dispatcher;

  std::atomic<size_t> uniqueActorIDCounter{0};

  ActorMap actors;
};
template<CallableOnFunction Scheduler, typename Inspector>
auto inspect(Inspector& f, Runtime<Scheduler>& x) {
  return f.object(x).fields(
      f.field("myServerID", x.myServerID), f.field("runtimeID", x.runtimeID),
      f.field("uniqueActorIDCounter", x.uniqueActorIDCounter.load()),
      f.field("actors", x.actors));
}

};  // namespace arangodb::pregel::actor

template<arangodb::pregel::actor::CallableOnFunction Scheduler>
struct fmt::formatter<arangodb::pregel::actor::Runtime<Scheduler>>
    : arangodb::inspection::inspection_formatter {};
