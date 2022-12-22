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

#include "ActorBase.h"
#include "velocypack/SharedSlice.h"

namespace arangodb::pregel::actor {

struct ExternalDispatcher {
  std::function<void(ActorPID, ActorPID, velocypack::SharedSlice)> send;
};

struct Dispatcher {
  Dispatcher(ServerID myServerID, ActorMap& actors,
             ExternalDispatcher externalDispatcher)
      : myServerID(myServerID),
        actors(actors),
        externalDispatcher(externalDispatcher) {}

  auto operator()(ActorPID sender, ActorPID receiver,
                  std::unique_ptr<MessagePayloadBase> payload) -> void {
    if (not actors.contains(receiver.id)) {
      std::abort();
      // TODO
    }
    auto& actor = actors[receiver.id];
    actor->process(sender, std::move(payload));
  }
  auto operator()(ActorPID sender, ActorPID receiver,
                  velocypack::SharedSlice msg) -> void {
    if (receiver.server == myServerID) {
      fmt::print("Called dispatcher recursively");
      std::abort();
    } else {
      externalDispatcher.send(sender, receiver, msg);
    }
  }
  ServerID myServerID;
  ActorMap& actors;
  ExternalDispatcher externalDispatcher;
};

}  // namespace arangodb::pregel::actor
