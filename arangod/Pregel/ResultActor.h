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

struct PregelResult {
  ResultT<PregelResults> results = {PregelResults{}};
  std::atomic<bool> complete{false};
  auto add(ResultT<PregelResults> moreResults, bool lastResult) -> void {
    if (complete.load()) {
      return;
    }
    if (results.fail()) {
      return;
    }
    if (moreResults.fail()) {
      results = moreResults;
      complete.store(true);
      return;
    }

    VPackBuilder newResultsBuilder;
    {
      VPackArrayBuilder ab(&newResultsBuilder);
      // Add existing results to new builder
      if (!results.get().results.isEmpty()) {
        newResultsBuilder.add(
            VPackArrayIterator(results.get().results.slice()));
      }
      // add new results from message to builder
      newResultsBuilder.add(
          VPackArrayIterator(moreResults.get().results.slice()));
    }
    results = {PregelResults{.results = std::move(newResultsBuilder)}};

    if (lastResult) {
      complete.store(true);
    }
  }
  auto set(ResultT<PregelResults> moreResults) -> void {
    results = moreResults;
    complete.store(true);
  }
  auto get() -> std::optional<ResultT<PregelResults>> {
    if (not complete.load()) {
      return std::nullopt;
    }
    return results;
  }
};
template<typename Inspector>
auto inspect(Inspector& f, PregelResult& x) {
  return f.object(x).fields(f.field("results", x.results));
}
struct ResultState {
  ResultState() = default;
  ResultState(TTL ttl) : ttl{ttl} {};
  std::shared_ptr<PregelResult> data = std::make_shared<PregelResult>();
  std::vector<actor::ActorPID> otherResultActors;
  TTL ttl;
  std::chrono::system_clock::time_point expiration;
};
template<typename Inspector>
auto inspect(Inspector& f, ResultState& x) {
  return f.object(x).fields(f.field("data", x.data));
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

  auto operator()(message::SaveResults msg) -> std::unique_ptr<ResultState> {
    this->state->data->set(msg.results);
    setExpiration();
    return std::move(this->state);
  }

  auto operator()(message::AddResults msg) -> std::unique_ptr<ResultState> {
    this->state->data->add(msg.results, msg.receivedAllResults);
    if (this->state->data->complete.load()) {
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
