////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Actor/Actor.h"
#include "Actor/ActorList.h"
#include "Actor/ActorID.h"
#include "Actor/Assert.h"
#include "Actor/ExitReason.h"
#include "Actor/IScheduler.h"

namespace arangodb::actor {

template<class Derived, class TActorPID>
struct BaseRuntime
    : std::enable_shared_from_this<BaseRuntime<Derived, TActorPID>> {
  using ActorPID = TActorPID;

  BaseRuntime() = delete;
  BaseRuntime(BaseRuntime const&) = delete;
  BaseRuntime(BaseRuntime&&) = delete;
  BaseRuntime(std::string runtimeID, std::shared_ptr<IScheduler> scheduler)
      : runtimeID(runtimeID), _scheduler(scheduler) {}

  template<typename ActorConfig>
  auto spawn(std::unique_ptr<typename ActorConfig::State> initialState)
      -> ActorPID {
    auto newId = ActorID{uniqueActorIDCounter++};

    auto address = self().makePid(newId);

    auto newActor = std::make_shared<Actor<Derived, ActorConfig>>(
        address, std::static_pointer_cast<Derived>(this->shared_from_this()),
        std::move(initialState));
    actors.add(newId, std::move(newActor));

    return address;
  }

  template<typename ActorConfig>
  auto spawn(std::unique_ptr<typename ActorConfig::State> initialState,
             typename ActorConfig::Message initialMessage) -> ActorPID {
    auto address = spawn<ActorConfig>(std::move(initialState));

    // Send initial message to newly created actor
    dispatchLocally(address, address, std::move(initialMessage),
                    IgnoreDispatchFailure::yes);

    return address;
  }

  auto getActorIDs() -> std::vector<ActorID> { return actors.allIDs(); }

  auto contains(ActorID id) const -> bool { return actors.contains(id); }

  template<typename ActorConfig>
  auto getActorStateByID(ActorID id) const
      -> std::optional<typename ActorConfig::State> {
    auto actorBase = actors.find(id);
    if (actorBase.has_value()) {
      auto* actor =
          dynamic_cast<Actor<Derived, ActorConfig>*>(actorBase->get());
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
  auto dispatch(ActorPID sender, ActorPID receiver, ActorMessage&& message)
      -> void {
    self().doDispatch(sender, receiver, std::move(message),
                      IgnoreDispatchFailure::no);
  }

  template<typename ActorMessage>
  auto dispatchDelayed(std::chrono::seconds delay, ActorPID sender,
                       ActorPID receiver, ActorMessage&& message) -> void {
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

  auto finishActor(ActorPID pid, ExitReason reason) -> void {
    auto actor = actors.find(pid.id);
    if (actor.has_value()) {
      actor.value()->finish(reason);
    }
  }

  auto monitorActor(ActorPID monitoringActor, ActorPID monitoredActor) -> void {
    if (not actors.monitor(monitoringActor.id, monitoredActor.id)) {
      // in case the monitored actor no longer exists (or may never have
      // existed) we send the down msg right away
      dispatch(
          // we use 0 as the sender id
          self().makePid({0}), monitoringActor,
          message::ActorDown<ActorPID>{.actor = monitoredActor,
                                       .reason = ExitReason::kUnknown});
    }
  }

  void stopActor(ActorPID pid, ExitReason reason) {
    auto res = actors.remove(pid.id);
    if (res) {
      auto& entry = *res;
      entry.actor.reset();
      for (auto& monitor : entry.monitors) {
        dispatch(pid,
                 // TODO - same problem with database as in dispatch
                 self().makePid(monitor),
                 message::ActorDown<ActorPID>{.actor = pid, .reason = reason});
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
      actor->finish(ExitReason::kShutdown);
    }
  }

  auto shutdown() -> void {
    softShutdown();
    actors.waitForAll();
  }

  IScheduler& scheduler() { return *_scheduler; }

  std::string const runtimeID;

  template<typename Inspector>
  friend inline auto inspect(Inspector& f, BaseRuntime& x) {
    return f.object(x).fields(
        f.field("runtimeID", x.runtimeID),
        f.field("uniqueActorIDCounter", x.uniqueActorIDCounter.load()),
        f.field("actors", x.actors));
  }

  ActorList<ActorPID> actors;

 protected:
  enum class IgnoreDispatchFailure {

    no,
    yes
  };

  template<typename ActorMessage>
  auto dispatchLocally(ActorPID sender, ActorPID receiver,
                       ActorMessage&& message,
                       IgnoreDispatchFailure ignoreFailure) -> void {
    auto actor = actors.find(receiver.id);
    MessagePayload<ActorMessage> payload(std::move(message));
    if (actor.has_value()) {
      actor->get()->process(sender, payload);
    } else if (ignoreFailure == IgnoreDispatchFailure::no) {
      // The sender might no longer exist, so don't bother if we cannot send the
      // ActorNotFound message
      self().doDispatch(
          receiver, sender,
          message::ActorError<ActorPID>{
              message::ActorNotFound<ActorPID>{.actor = receiver}},
          IgnoreDispatchFailure::yes);
    }
  }

 private:
  Derived& self() { return static_cast<Derived&>(*this); }

  std::shared_ptr<IScheduler> _scheduler;

  // actor id 0 is reserved for special messages
  std::atomic<size_t> uniqueActorIDCounter{1};
};

};  // namespace arangodb::actor
