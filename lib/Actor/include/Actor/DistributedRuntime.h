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

#include <chrono>
#include <optional>
#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include "Inspection/VPackWithErrorT.h"

#include "Actor/Actor.h"
#include "Actor/ActorList.h"
#include "Actor/ActorID.h"
#include "Actor/Assert.h"
#include "Actor/DistributedActorPID.h"
#include "Actor/IExternalDispatcher.h"
#include "Actor/IScheduler.h"

namespace arangodb::actor {

struct DistributedRuntime : std::enable_shared_from_this<DistributedRuntime> {
  using ActorPID = DistributedActorPID;

  DistributedRuntime() = delete;
  DistributedRuntime(DistributedRuntime const&) = delete;
  DistributedRuntime(DistributedRuntime&&) = delete;
  DistributedRuntime(ServerID myServerID, std::string runtimeID,
                     std::shared_ptr<IScheduler> scheduler,
                     std::shared_ptr<IExternalDispatcher> externalDispatcher)
      : myServerID(myServerID),
        runtimeID(runtimeID),
        _scheduler(scheduler),
        externalDispatcher(externalDispatcher) {}

  template<typename ActorConfig>
  auto spawn(std::unique_ptr<typename ActorConfig::State> initialState)
      -> ActorPID {
    auto newId = ActorID{uniqueActorIDCounter++};

    // TODO - we do not want to pass the database name as part of the spawn
    // call. If we really need it as part of  the actor PID, we need to find a
    // better way.
    auto address =
        ActorPID{.server = myServerID, .database = "database", .id = newId};

    auto newActor = std::make_shared<Actor<DistributedRuntime, ActorConfig>>(
        address, this->shared_from_this(), std::move(initialState));
    actors.add(newId, std::move(newActor));

    return address;
  }

  template<typename ActorConfig>
  auto spawn(std::unique_ptr<typename ActorConfig::State> initialState,
             typename ActorConfig::Message initialMessage) -> ActorPID {
    auto address = spawn<ActorConfig>(std::move(initialState));

    // Send initial message to newly created actor
    dispatchLocally(address, address, initialMessage, true);

    return address;
  }

  auto getActorIDs() -> std::vector<ActorID> { return actors.allIDs(); }

  auto contains(ActorID id) const -> bool { return actors.contains(id); }

  template<typename ActorConfig>
  auto getActorStateByID(ActorID id) const
      -> std::optional<typename ActorConfig::State> {
    auto actorBase = actors.find(id);
    if (actorBase.has_value()) {
      auto* actor = dynamic_cast<Actor<DistributedRuntime, ActorConfig>*>(
          actorBase->get());
      if (actor != nullptr) {
        return actor->getState();
      }
    }
    return std::nullopt;
  }

  template<typename ActorConfig>
  auto getActorStateByID(ActorPID pid) const
      -> std::optional<typename ActorConfig::State> {
    return getActorStateByID<ActorConfig>(pid.id);
  }

  auto getSerializedActorByID(ActorID id) const
      -> std::optional<velocypack::SharedSlice> {
    auto actor = actors.find(id);
    if (actor.has_value()) {
      if (actor->get() != nullptr) {
        return actor.value()->serialize();
      }
    }
    return std::nullopt;
  }

  auto receive(ActorPID sender, ActorPID receiver, velocypack::SharedSlice msg)
      -> void {
    auto actor = actors.find(receiver.id);
    if (actor.has_value()) {
      actor.value()->process(sender, msg);
    } else {
      auto error = message::ActorError<ActorPID>{
          message::ActorNotFound<ActorPID>{.actor = receiver}};
      auto payload = inspection::serializeWithErrorT(error);
      ACTOR_ASSERT(payload.ok());
      dispatch(receiver, sender, payload.get());
    }
  }

  template<typename ActorMessage>
  auto dispatch(ActorPID sender, ActorPID receiver, ActorMessage const& message)
      -> void {
    doDispatch(sender, receiver, message, false);
  }

  template<typename ActorMessage>
  auto dispatchDelayed(std::chrono::seconds delay, ActorPID sender,
                       ActorPID receiver, ActorMessage const& message) -> void {
    _scheduler->delay(delay, [self = this->weak_from_this(), sender, receiver,
                              message](bool canceled) {
      auto me = self.lock();
      if (me != nullptr) {
        me->dispatch(sender, receiver, message);
      }
    });
  }

  auto areAllActorsIdle() -> bool {
    return actors.checkAll(
        [](std::shared_ptr<ActorBase<ActorPID>> const& actor) {
          return actor->isIdle();
        });
  }

  auto finishActor(ActorPID pid) -> void {
    auto actor = actors.find(pid.id);
    if (actor.has_value()) {
      actor.value()->finish();
    }
  }

  auto monitorActor(ActorPID monitoringActor, ActorPID monitoredActor) -> void {
    // ATM we can only monitor actors on the same server
    ACTOR_ASSERT(monitoringActor.server == myServerID);
    ACTOR_ASSERT(monitoringActor.server == monitoredActor.server);
    if (not actors.monitor(monitoringActor.id, monitoredActor.id)) {
      // in case the monitored actor no longer exists (or may never have
      // existed) we send the down msg right away
      dispatch(
          // we use 0 as the sender id
          DistributedActorPID{.server = myServerID, .database = "", .id = {0}},
          monitoringActor,
          message::ActorDown<ActorPID>{.actor = monitoredActor});
    }
  }

  void stopActor(ActorPID pid) {
    auto res = actors.remove(pid.id);
    if (res) {
      auto& entry = *res;
      entry.actor.reset();
      for (auto& monitor : entry.monitors) {
        dispatch(pid,
                 // TODO - same problem with database as in dispatch
                 DistributedActorPID{
                     .server = myServerID, .database = "", .id = monitor},
                 message::ActorDown<ActorPID>{.actor = pid});
      }
    }
  }

  auto softShutdown() -> void {
    std::vector<std::shared_ptr<ActorBase<ActorPID>>> actorsCopy;
    // we copy out all actors, because we have to call finish outside the lock!
    actors.apply([&](std::shared_ptr<ActorBase<ActorPID>> const& actor) {
      actorsCopy.emplace_back(actor);
    });
    for (auto& actor : actorsCopy) {
      actor->finish();
    }
  }

  IScheduler& scheduler() { return *_scheduler; }

  ServerID const myServerID;
  std::string const runtimeID;

  template<typename Inspector>
  friend inline auto inspect(Inspector& f, DistributedRuntime& x) {
    return f.object(x).fields(
        f.field("myServerID", x.myServerID), f.field("runtimeID", x.runtimeID),
        f.field("uniqueActorIDCounter", x.uniqueActorIDCounter.load()),
        f.field("actors", x.actors));
  }

  ActorList<ActorPID> actors;

 private:
  std::shared_ptr<IScheduler> _scheduler;
  std::shared_ptr<IExternalDispatcher> externalDispatcher;

  // actor id 0 is reserved for special messages
  std::atomic<size_t> uniqueActorIDCounter{1};

  template<typename ActorMessage>
  auto doDispatch(ActorPID sender, ActorPID receiver,
                  ActorMessage const& message, bool ignoreFailure) -> void {
    if (receiver.server == sender.server) {
      dispatchLocally(sender, receiver, message, ignoreFailure);
    } else {
      dispatchExternally(sender, receiver, message);
    }
  }

  template<typename ActorMessage>
  auto dispatchLocally(ActorPID sender, ActorPID receiver,
                       ActorMessage const& message, bool ignoreFailure)
      -> void {
    auto actor = actors.find(receiver.id);
    auto payload = MessagePayload<ActorMessage>(std::move(message));
    if (actor.has_value()) {
      actor->get()->process(sender, payload);
    } else if (not ignoreFailure) {
      // The sender might no longer exist, so don't bother if we cannot send the
      // ActorNotFound message
      doDispatch(receiver, sender,
                 message::ActorError<ActorPID>{
                     message::ActorNotFound<ActorPID>{.actor = receiver}},
                 true);
    }
  }

  template<typename ActorMessage>
  auto dispatchExternally(ActorPID sender, ActorPID receiver,
                          ActorMessage const& message) -> void {
    auto payload = inspection::serializeWithErrorT(message);
    ACTOR_ASSERT(payload.ok());
    externalDispatcher->dispatch(sender, receiver, payload.get());
  }
};

};  // namespace arangodb::actor
