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
#include <utility>
#include "Actor/ActorPID.h"
#include "Actor/HandlerBase.h"
#include "Pregel/AggregatorHandler.h"
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
  SpawnState(TRI_vocbase_t& vocbase, actor::ActorPID resultActor)
      : vocbaseGuard{vocbase}, resultActor(std::move(resultActor)) {}
  const DatabaseGuard vocbaseGuard;
  const actor::ActorPID resultActor;
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
  auto spawnWorker(std::unique_ptr<Algorithm<V, E, M>> algorithm,
                   message::SpawnWorker msg) -> void {
    this->template spawn<worker::WorkerActor<V, E, M>>(
        std::make_unique<worker::WorkerState<V, E, M>>(
            algorithm->workerContextUnique(
                std::make_unique<AggregatorHandler>(algorithm.get()),
                std::make_unique<AggregatorHandler>(algorithm.get()),
                msg.message.userParameters.slice()),
            msg.conductor, msg.message, std::chrono::seconds(60),
            algorithm->messageFormatUnique(),
            algorithm->messageCombinerUnique(), std::move(algorithm),
            this->state->vocbaseGuard.database(), this->self,
            this->state->resultActor, msg.statusActor, msg.metricsActor),
        worker::message::WorkerStart{});
  }

  auto spawnWorker(message::SpawnWorker msg) -> void {
    VPackSlice userParams = msg.message.userParameters.slice();
    std::string algorithm = msg.message.algorithm;

    if (algorithm == "sssp") {
      spawnWorker<algos::SSSPType::Vertex, algos::SSSPType::Edge,
                  algos::SSSPType::Message>(
          std::make_unique<algos::SSSPAlgorithm>(userParams), std::move(msg));
    } else if (algorithm == "pagerank") {
      spawnWorker<algos::PageRankType::Vertex, algos::PageRankType::Edge,
                  algos::PageRankType::Message>(
          std::make_unique<algos::PageRank>(userParams), std::move(msg));
    } else if (algorithm == "recoveringpagerank") {
      spawnWorker<algos::RecoveringPageRankType::Vertex,
                  algos::RecoveringPageRankType::Edge,
                  algos::RecoveringPageRankType::Message>(
          std::make_unique<algos::RecoveringPageRank>(userParams),
          std::move(msg));
    } else if (algorithm == "shortestpath") {
      spawnWorker<algos::ShortestPathType::Vertex,
                  algos::ShortestPathType::Edge,
                  algos::ShortestPathType::Message>(
          std::make_unique<algos::ShortestPathAlgorithm>(userParams),
          std::move(msg));
    } else if (algorithm == "linerank") {
      spawnWorker<algos::LineRankType::Vertex, algos::LineRankType::Edge,
                  algos::LineRankType::Message>(
          std::make_unique<algos::LineRank>(userParams), std::move(msg));
    } else if (algorithm == "effectivecloseness") {
      spawnWorker<algos::EffectiveClosenessType::Vertex,
                  algos::EffectiveClosenessType::Edge,
                  algos::EffectiveClosenessType::Message>(
          std::make_unique<algos::EffectiveCloseness>(userParams),
          std::move(msg));
    } else if (algorithm == "connectedcomponents") {
      spawnWorker<algos::ConnectedComponentsType::Vertex,
                  algos::ConnectedComponentsType::Edge,
                  algos::ConnectedComponentsType::Message>(
          std::make_unique<algos::ConnectedComponents>(userParams),
          std::move(msg));
    } else if (algorithm == "scc") {
      spawnWorker<algos::SCCType::Vertex, algos::SCCType::Edge,
                  algos::SCCType::Message>(
          std::make_unique<algos::SCC>(userParams), std::move(msg));
    } else if (algorithm == "hits") {
      spawnWorker<algos::HITSType::Vertex, algos::HITSType::Edge,
                  algos::HITSType::Message>(
          std::make_unique<algos::HITS>(userParams), std::move(msg));
    } else if (algorithm == "hitskleinberg") {
      spawnWorker<algos::HITSKleinbergType::Vertex,
                  algos::HITSKleinbergType::Edge,
                  algos::HITSKleinbergType::Message>(
          std::make_unique<algos::HITSKleinberg>(userParams), std::move(msg));
    } else if (algorithm == "labelpropagation") {
      spawnWorker<algos::LabelPropagationType::Vertex,
                  algos::LabelPropagationType::Edge,
                  algos::LabelPropagationType::Message>(
          std::make_unique<algos::LabelPropagation>(userParams),
          std::move(msg));
    } else if (algorithm == "slpa") {
      spawnWorker<algos::SLPAType::Vertex, algos::SLPAType::Edge,
                  algos::SLPAType::Message>(
          std::make_unique<algos::SLPA>(userParams), std::move(msg));
    } else if (algorithm == "dmid") {
      spawnWorker<algos::DMIDType::Vertex, algos::DMIDType::Edge,
                  algos::DMIDType::Message>(
          std::make_unique<algos::DMID>(userParams), std::move(msg));
    } else if (algorithm == "wcc") {
      spawnWorker<algos::WCCType::Vertex, algos::WCCType::Edge,
                  algos::WCCType::Message>(
          std::make_unique<algos::WCC>(userParams), std::move(msg));
    } else if (algorithm == "colorpropagation") {
      spawnWorker<algos::ColorPropagationType::Vertex,
                  algos::ColorPropagationType::Edge,
                  algos::ColorPropagationType::Message>(
          std::make_unique<algos::ColorPropagation>(userParams),
          std::move(msg));
    }
#if defined(ARANGODB_ENABLE_MAINTAINER_MODE)
    else if (algorithm == "readwrite") {
      spawnWorker<algos::ReadWriteType::Vertex, algos::ReadWriteType::Edge,
                  algos::ReadWriteType::Message>(
          std::make_unique<algos::ReadWrite>(userParams), std::move(msg));
    }
#endif
    else {
      this->template dispatch<conductor::message::ConductorMessages>(
          msg.conductor, ResultT<conductor::message::WorkerCreated>::error(
                             TRI_ERROR_BAD_PARAMETER, "Unsupported algorithm"));
    }
  }

  auto operator()(message::SpawnWorker msg) -> std::unique_ptr<SpawnState> {
    LOG_TOPIC("2452c", INFO, Logger::PREGEL)
        << "Spawn Actor: Spawn worker actor";
    if (msg.destinationServer == this->self.server) {
      spawnWorker(std::move(msg));
    } else {
      // ActorID 0 is a reserved ID that is used here to tell the RestHandler
      // that it needs to spawn a new actor instead of searching for an existing
      // actor
      this->dispatch(actor::ActorPID{.server = msg.destinationServer,
                                     .database = this->self.database,
                                     .id = {0}},
                     message::SpawnMessages{msg});
    }
    return std::move(this->state);
  }

  auto operator()(message::SpawnCleanup msg) -> std::unique_ptr<SpawnState> {
    this->finish();
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
