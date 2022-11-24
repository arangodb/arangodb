#pragma once

#include <memory>
#include "Actor/Actor.h"
#include "Scheduler.h"
#include "InitialActor.h"

namespace arangodb::pregel::actor {

struct Conductor {
  std::unique_ptr<ActorBase> actor;
  Conductor() {
    actor = std::make_unique<InitialActor>(
        testScheduler, std::make_unique<InitialState>(*this));
  }
};

struct ConductorHandler {};

using ConductorActor = Actor<Scheduler, ConductorHandler, Conductor>;
}  // namespace arangodb::pregel::actor
