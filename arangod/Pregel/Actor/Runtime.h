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

#include <velocypack/Builder.h>
#include <Inspection/VPackWithErrorT.h>
#include <string>
#include <cstdint>
#include <memory>
#include <vector>

#include "Actor.h"

namespace arangodb::pregel::actor {

template<typename Scheduler, typename SendingMechanism>
struct Runtime {
  Runtime() = delete;
  Runtime(ServerID myServerID, std::string runtimeID,
          std::shared_ptr<Scheduler> scheduler,
          std::shared_ptr<SendingMechanism> sending_mechanism)
      : myServerID(myServerID),
        runtimeID(runtimeID),
        scheduler(scheduler),
        sending_mechanism(sending_mechanism) {}

  auto dispatch(std::unique_ptr<Message> msg) -> void {
    if (msg->receiver.server == myServerID) {
      auto& actor = actors.at(msg->receiver.id);
      if (msg->payload == nullptr) {
        std::cout << "dispatch found nullptr payload" << std::endl;
      }
      actor->process(msg->sender, std::move(msg->payload));
    } else {
      // TODO  sending_mechanism.send(std::move(msg));
      assert(false);
    }
  }

  template<Actorable ActorConfig>
  auto spawn(typename ActorConfig::State initialState,
             typename ActorConfig::Message initialMessage) -> ActorID {
    auto newId = ActorID{
        uniqueActorIdCounter++};  // TODO: check whether this is what we want
    auto newActor = std::make_unique<Actor<Scheduler, ActorConfig>>(
        scheduler, std::make_unique<typename ActorConfig::State>(initialState));
    actors.emplace(newId, std::move(newActor));

    // Send initial message to newly created actor
    auto address = ActorPID{.id = newId, .server = myServerID};
    auto initialPayload =
        std::make_unique<MessagePayload<typename ActorConfig::Message>>(
            initialMessage);
    dispatch(
        std::make_unique<Message>(address, address, std::move(initialPayload)));

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

  template<Actorable ActorConfig>
  auto getActorStateByID(ActorID id)
      -> std::optional<typename ActorConfig::State> {
    if (actors.contains(id)) {
      auto* actor =
          dynamic_cast<Actor<Scheduler, ActorConfig>*>(actors[id].get());
      if (actor != nullptr && actor->state != nullptr) {
        return *actor->state;
      }
    }
    return std::nullopt;
  }

  ServerID myServerID;
  std::string const runtimeID;

  std::shared_ptr<Scheduler> scheduler;
  std::shared_ptr<SendingMechanism> sending_mechanism;

  using ActorMap = std::unordered_map<ActorID, std::unique_ptr<ActorBase>>;
  std::atomic<size_t> uniqueActorIdCounter{0};

  ActorMap actors;
};

};  // namespace arangodb::pregel::actor
