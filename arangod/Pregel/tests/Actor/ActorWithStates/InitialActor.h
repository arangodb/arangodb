#pragma once

#include "ConductorActor.h"
#include "Message.h"
#include <fmt/core.h>

namespace arangodb::pregel::actor {
struct InitialState {
  ~InitialState() = default;
  InitialState(ConductorActor& conductor) : conductor{conductor} {};
  ConductorActor& conductor;
  auto name() const -> std::string { return "initial"; };
};

struct InitialHandler {
  InitialHandler() = default;
  InitialHandler(std::unique_ptr<InitialState> state)
      : state{std::move(state)} {}
  std::unique_ptr<InitialState> state;

  auto operator()(InitStart& msg) -> std::unique_ptr<InitialState> {
    fmt::print("got start message");
    return std::move(state);
  }
  auto operator()(InitDone& msg) -> std::unique_ptr<InitialState> {
    fmt::print("got done message");
    state->conductor.process(std::make_unique<Message>(InitDone{}));
    return std::move(state);
  }
  auto operator()(auto&& msg) -> std::unique_ptr<InitialState> {
    fmt::print("got any message");
    return std::move(state);
  }
};

using InitialActor = Actor<Scheduler, InitialHandler, InitialState>;

}  // namespace arangodb::pregel::actor
