#pragma once

#include "Actor/Actor.h"
#include "ConductorActor.h"
#include "Message.h"
#include <fmt/core.h>

namespace arangodb::pregel::actor {
struct InitialState {
  ~InitialState() = default;
  InitialState(std::shared_ptr<ActorBase> conductor) : conductor{conductor} {};
  std::shared_ptr<ActorBase> conductor;
  auto name() const -> std::string { return "initial"; };
};

struct InitialHandler {
  InitialHandler() = default;
  InitialHandler(std::unique_ptr<InitialState> state,
                 std::shared_ptr<ActorBase> sender)
      : state{std::move(state)}, sender{sender} {}
  std::unique_ptr<InitialState> state;
  std::shared_ptr<ActorBase> sender;

  auto operator()(InitStart& msg) -> std::unique_ptr<InitialState> {
    fmt::print("got start message");
    return std::move(state);
  }
  auto operator()(InitDone& msg) -> std::unique_ptr<InitialState> {
    fmt::print("got done message");
    state->conductor->process(std::make_unique<Message>(nullptr, InitDone{}));
    return std::move(state);
  }
  auto operator()(auto&& msg) -> std::unique_ptr<InitialState> {
    fmt::print("got any message");
    return std::move(state);
  }
};

using InitialActor = Actor<Scheduler, InitialHandler, InitialState>;

}  // namespace arangodb::pregel::actor
