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

#include <memory>
#include "Actor/ActorPID.h"
#include "Actor/HandlerBase.h"
#include "Pregel/Worker/Actor.h"
#include "Pregel/SpawnMessages.h"
#include "Pregel/Worker/Messages.h"
#include "Pregel/Worker/State.h"
#include "VocBase/vocbase.h"
#include "fmt/core.h"

#include "Pregel/Algos/ColorPropagation/ColorPropagation.h"
#include "Pregel/Algos/ConnectedComponents/ConnectedComponents.h"
#include "Pregel/Algos/DMID/DMID.h"
#include "Pregel/Algos/EffectiveCloseness/EffectiveCloseness.h"
#include "Pregel/Algos/HITS/HITS.h"
#include "Pregel/Algos/HITSKleinberg/HITSKleinberg.h"
#include "Pregel/Algos/LabelPropagation/LabelPropagation.h"
#include "Pregel/Algos/LineRank/LineRank.h"
#include "Pregel/Algos/PageRank/PageRank.h"
#include "Pregel/Algos/RecoveringPageRank/RecoveringPageRank.h"
#include "Pregel/Algos/SCC/SCC.h"
#include "Pregel/Algos/ShortestPath/ShortestPath.h"
#include "Pregel/Algos/SLPA/SLPA.h"
#include "Pregel/Algos/SSSP/SSSP.h"
#include "Pregel/Algos/WCC/WCC.h"
#include "Pregel/Utils.h"
#if defined(ARANGODB_ENABLE_MAINTAINER_MODE)
#include "Pregel/Algos/ReadWrite/ReadWrite.h"
#endif

namespace arangodb::pregel {

struct SpawnState {
  SpawnState(TRI_vocbase_t& vocbase) : vocbaseGuard{vocbase} {}
  const DatabaseGuard vocbaseGuard;
};
template<typename Inspector>
auto inspect(Inspector& f, SpawnState& x) {
  return f.object(x).fields();
}

template<typename Runtime>
struct SpawnHandler : actor::HandlerBase<Runtime, SpawnState> {
  auto operator()(message::SpawnStart start) -> std::unique_ptr<SpawnState> {
    LOG_TOPIC("4a414", INFO, Logger::PREGEL)
        << fmt::format("Spawn Actor {} started", this->self);
    return std::move(this->state);
  }

  template<typename V, typename E, typename M>
  auto spawnWorker(Algorithm<V, E, M>* algorithm, message::SpawnWorker msg)
      -> void {
    this->template spawn<worker::WorkerActor<V, E, M>>(
        std::make_unique<worker::WorkerState<V, E, M>>(
            msg.conductor, msg.message.executionSpecifications,
            msg.message.collectionSpecifications, algorithm,
            this->state->vocbaseGuard.database()),
        worker::message::WorkerStart{});
  }

  auto spawnWorker(message::SpawnWorker msg) -> void {
    VPackSlice userParams =
        msg.message.executionSpecifications.userParameters.slice();
    std::string algorithm = msg.message.executionSpecifications.algorithm;

    auto& server = this->state->vocbaseGuard.database().server();
    if (algorithm == "sssp") {
      spawnWorker(new algos::SSSPAlgorithm(server, userParams), std::move(msg));
    } else if (algorithm == "pagerank") {
      spawnWorker(new algos::PageRank(server, userParams), std::move(msg));
    } else if (algorithm == "recoveringpagerank") {
      spawnWorker(new algos::RecoveringPageRank(server, userParams),
                  std::move(msg));
    } else if (algorithm == "shortestpath") {
      spawnWorker(new algos::ShortestPathAlgorithm(server, userParams),
                  std::move(msg));
    } else if (algorithm == "linerank") {
      spawnWorker(new algos::LineRank(server, userParams), std::move(msg));
    } else if (algorithm == "effectivecloseness") {
      spawnWorker(new algos::EffectiveCloseness(server, userParams),
                  std::move(msg));
    } else if (algorithm == "connectedcomponents") {
      spawnWorker(new algos::ConnectedComponents(server, userParams),
                  std::move(msg));
    } else if (algorithm == "scc") {
      spawnWorker(new algos::SCC(server, userParams), std::move(msg));
    } else if (algorithm == "hits") {
      spawnWorker(new algos::HITS(server, userParams), std::move(msg));
    } else if (algorithm == "hitskleinberg") {
      spawnWorker(new algos::HITSKleinberg(server, userParams), std::move(msg));
    } else if (algorithm == "labelpropagation") {
      spawnWorker(new algos::LabelPropagation(server, userParams),
                  std::move(msg));
    } else if (algorithm == "slpa") {
      spawnWorker(new algos::SLPA(server, userParams), std::move(msg));
    } else if (algorithm == "dmid") {
      spawnWorker(new algos::DMID(server, userParams), std::move(msg));
    } else if (algorithm == "wcc") {
      spawnWorker(new algos::WCC(server, userParams), std::move(msg));
    } else if (algorithm == "colorpropagation") {
      spawnWorker(new algos::ColorPropagation(server, userParams),
                  std::move(msg));
    }
#if defined(ARANGODB_ENABLE_MAINTAINER_MODE)
    else if (algorithm == "readwrite") {
      spawnWorker(new algos::ReadWrite(server, userParams), std::move(msg));
    } else {
#endif
      this->template dispatch<conductor::message::ConductorMessages>(
          msg.conductor, ResultT<conductor::message::WorkerCreated>::error(
                             TRI_ERROR_BAD_PARAMETER, "Unsupported algorithm"));
    }
  }

  auto operator()(message::SpawnWorker msg) -> std::unique_ptr<SpawnState> {
    LOG_TOPIC("2452c", INFO, Logger::PREGEL)
        << "Spawn Actor: Spawn worker actor";
    spawnWorker(std::move(msg));
    return std::move(this->state);
  }

  auto operator()(actor::message::UnknownMessage unknown)
      -> std::unique_ptr<SpawnState> {
    LOG_TOPIC("7b602", INFO, Logger::PREGEL) << fmt::format(
        "Spawn Actor: Error - sent unknown message to {}", unknown.receiver);
    return std::move(this->state);
  }

  auto operator()(actor::message::ActorNotFound notFound)
      -> std::unique_ptr<SpawnState> {
    LOG_TOPIC("03156", INFO, Logger::PREGEL) << fmt::format(
        "Spawn Actor: Error - receiving actor {} not found", notFound.actor);
    return std::move(this->state);
  }

  auto operator()(actor::message::NetworkError notFound)
      -> std::unique_ptr<SpawnState> {
    LOG_TOPIC("a87b3", INFO, Logger::PREGEL) << fmt::format(
        "Spawn Actor: Error - network error {}", notFound.message);
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<SpawnState> {
    LOG_TOPIC("89d72", INFO, Logger::PREGEL)
        << "Spawn Actor: Got unhandled message";
    return std::move(this->state);
  }
};

struct SpawnActor {
  using State = SpawnState;
  using Message = message::SpawnMessages;
  template<typename Runtime>
  using Handler = SpawnHandler<Runtime>;
  static constexpr auto typeName() -> std::string_view { return "Spawn Actor"; }
};

}  // namespace arangodb::pregel
