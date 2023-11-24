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

#include "Actor/BaseRuntime.h"
#include "Actor/DistributedActorPID.h"
#include "Actor/IExternalDispatcher.h"

namespace arangodb::actor {

struct DistributedRuntime
    : BaseRuntime<DistributedRuntime, DistributedActorPID> {
  using Base = BaseRuntime<DistributedRuntime, DistributedActorPID>;

  DistributedRuntime() = delete;
  DistributedRuntime(DistributedRuntime const&) = delete;
  DistributedRuntime(DistributedRuntime&&) = delete;
  DistributedRuntime(ServerID myServerID, std::string runtimeID,
                     std::shared_ptr<IScheduler> scheduler,
                     std::shared_ptr<IExternalDispatcher> externalDispatcher)
      : Base(std::move(runtimeID), std::move(scheduler)),
        myServerID(std::move(myServerID)),
        externalDispatcher(std::move(externalDispatcher)) {}

  auto monitorActor(ActorPID monitoringActor, ActorPID monitoredActor) -> void {
    // ATM we can only monitor actors on the same server
    ACTOR_ASSERT(monitoringActor.server == myServerID);
    ACTOR_ASSERT(monitoringActor.server == monitoredActor.server);
    Base::monitorActor(monitoringActor, monitoredActor);
  }

  ServerID const myServerID;

  template<typename Inspector>
  friend inline auto inspect(Inspector& f, DistributedRuntime& x) {
    return f.object(x).fields(f.field("myServerID", x.myServerID),
                              f.embedFields(static_cast<Base&>(x)));
  }

 private:
  template<typename Derived, typename PID>
  friend struct BaseRuntime;

  std::shared_ptr<IExternalDispatcher> externalDispatcher;

  ActorPID makePid(ActorID id) {
    // TODO - we do not want to pass the database name as part of the spawn
    // call. If we really need it as part of  the actor PID, we need to find a
    // better way.
    return ActorPID{.server = myServerID, .database = "database", .id = id};
  }

  template<typename ActorMessage>
  auto doDispatch(ActorPID sender, ActorPID receiver, ActorMessage&& message,
                  IgnoreDispatchFailure ignoreFailure) -> void {
    if (receiver.server == sender.server) {
      dispatchLocally(sender, receiver, std::move(message), ignoreFailure);
    } else {
      dispatchExternally(sender, receiver, std::move(message));
    }
  }

  template<typename ActorMessage>
  auto dispatchExternally(ActorPID sender, ActorPID receiver,
                          ActorMessage&& message) -> void {
    auto payload = inspection::serializeWithErrorT(message);
    ACTOR_ASSERT(payload.ok());
    externalDispatcher->dispatch(sender, receiver, payload.get());
  }
};

};  // namespace arangodb::actor
