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

#include "Actor/ActorBase.h"

namespace arangodb::pregel::actor {

struct Dispatcher {
  Dispatcher(ServerID myServerID, ActorMap& actors)
      : myServerID(myServerID), actors(actors) {}

  auto operator()(std::unique_ptr<Message> msg) -> void {
    if (msg->receiver.server == myServerID) {
      if (not actors.contains(msg->receiver.id)) {
        // TODO
      }
      auto& actor = actors[msg->receiver.id];
      actor->process(msg->sender, std::move(msg->payload));
    } else {
      // TODO  sending_mechanism.send(std::move(msg));
      std::abort();
    }
  }

  ServerID myServerID;
  ActorMap& actors;
};

}  // namespace arangodb::pregel::actor
