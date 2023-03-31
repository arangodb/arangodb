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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <chrono>
#include <memory>
#include "Actor/ActorPID.h"
#include "Actor/HandlerBase.h"
#include "Basics/ResultT.h"
#include "Pregel/ResultMessages.h"
#include "Scheduler/SchedulerFeature.h"
#include "fmt/core.h"

namespace arangodb::pregel {

struct ResultState {
  ResultState() = default;
  ResultState(TTL ttl) : ttl{ttl} {};
  ResultT<PregelResults> results = {PregelResults{}};
  bool complete{false};
  std::vector<actor::ActorPID> otherResultActors;
  TTL ttl;
  std::chrono::system_clock::time_point expiration;
};

template<typename Inspector>
auto inspect(Inspector& f, ResultState& x) {
  return f.object(x).fields(f.field("results", x.results));
}

template<typename Runtime>
struct ResultHandler : actor::HandlerBase<Runtime, ResultState> {
  auto setExpiration() {
    this->state->expiration =
        std::chrono::system_clock::now() + this->state->ttl.duration;

    this->template dispatch<pregel::message::ResultMessages>(
        this->self, pregel::message::CleanupResultWhenExpired{});
  }

  auto operator()(message::ResultStart start) -> std::unique_ptr<ResultState> {
    LOG_TOPIC("ea414", INFO, Logger::PREGEL)
        << fmt::format("Result Actor {} started", this->self);
    return std::move(this->state);
  }

  auto operator()(message::OtherResultActorStarted start)
      -> std::unique_ptr<ResultState> {
    this->state->otherResultActors.push_back(this->sender);
    return std::move(this->state);
  }

  auto operator()(message::SaveResults start) -> std::unique_ptr<ResultState> {
    this->state->results = {start.results};
    this->state->complete = true;
    setExpiration();
    return std::move(this->state);
  }

  auto operator()(message::AddResults msg) -> std::unique_ptr<ResultState> {
    if (this->state->complete) {
      return std::move(this->state);
    }

    if (this->state->results.fail()) {
      return std::move(this->state);
    }

    if (msg.results.fail()) {
      this->state->results = msg.results;
      this->state->complete = true;
      setExpiration();
      return std::move(this->state);
    }

    VPackBuilder newResultsBuilder;
    {
      VPackArrayBuilder ab(&newResultsBuilder);
      // Add existing results to new builder
      if (!msg.results.get().results.isEmpty()) {
        newResultsBuilder.add(
            VPackArrayIterator(msg.results.get().results.slice()));
      }
      // add new results from message to builder
      newResultsBuilder.add(
          VPackArrayIterator(msg.results.get().results.slice()));
    }
    this->state->results = {
        PregelResults{.results = std::move(newResultsBuilder)}};

    if (msg.receivedAllResults) {
      this->state->complete = true;
      setExpiration();
    }

    return std::move(this->state);
  }

  auto operator()(message::CleanupResultWhenExpired msg)
      -> std::unique_ptr<ResultState> {
    if (this->state->expiration <= std::chrono::system_clock::now()) {
      this->finish();
    } else {
      // send this message every 20 seconds
      std::chrono::seconds offset = std::chrono::seconds(20);
      this->template dispatchDelayed<pregel::message::ResultMessages>(
          offset, this->self, pregel::message::CleanupResultWhenExpired{});
    }
    return std::move(this->state);
  }

  auto operator()(message::CleanupResults msg) -> std::unique_ptr<ResultState> {
    this->finish();
    for (auto const& actor : this->state->otherResultActors) {
      this->template dispatch<pregel::message::ResultMessages>(
          actor, pregel::message::CleanupResults{});
    }
    return std::move(this->state);
  }

  auto operator()(actor::message::UnknownMessage unknown)
      -> std::unique_ptr<ResultState> {
    LOG_TOPIC("eb602", INFO, Logger::PREGEL) << fmt::format(
        "Result Actor: Error - sent unknown message to {}", unknown.receiver);
    return std::move(this->state);
  }

  auto operator()(actor::message::ActorNotFound notFound)
      -> std::unique_ptr<ResultState> {
    LOG_TOPIC("e3156", INFO, Logger::PREGEL) << fmt::format(
        "Result Actor: Error - receiving actor {} not found", notFound.actor);
    return std::move(this->state);
  }

  auto operator()(actor::message::NetworkError notFound)
      -> std::unique_ptr<ResultState> {
    LOG_TOPIC("e87b3", INFO, Logger::PREGEL) << fmt::format(
        "Result Actor: Error - network error {}", notFound.message);
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<ResultState> {
    LOG_TOPIC("e9d72", INFO, Logger::PREGEL)
        << "Result Actor: Got unhandled message";
    return std::move(this->state);
  }
};

struct ResultActor {
  using State = ResultState;
  using Message = message::ResultMessages;
  template<typename Runtime>
  using Handler = ResultHandler<Runtime>;
  static constexpr auto typeName() -> std::string_view {
    return "Result Actor";
  }
};

}  // namespace arangodb::pregel
