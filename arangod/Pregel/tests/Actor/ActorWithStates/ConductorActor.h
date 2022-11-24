#pragma once

#include <memory>
#include "Actor/Actor.h"
#include "Scheduler.h"
#include "InitialActor.h"

namespace arangodb::pregel::actor {

struct Conductor {
  std::unique_ptr<ActorBase> stateActor;
};

struct ConductorHandler {
  ConductorHandler(std::unique_ptr<Conductor> state,
                   std::shared_ptr<ActorBase> sender)
      : state{std::move(state)}, sender{sender} {}
  std::unique_ptr<Conductor> state;
  std::shared_ptr<ActorBase> sender;

  auto operator()(InitConductor& msg) -> std::unique_ptr<Conductor> {
    state->stateActor = std::make_unique<InitialActor>(
        testScheduler, std::make_unique<InitialState>(sender));
    return std::move(state);
  }
};

using ConductorActor = Actor<Scheduler, ConductorHandler, Conductor>;
}  // namespace arangodb::pregel::actor
