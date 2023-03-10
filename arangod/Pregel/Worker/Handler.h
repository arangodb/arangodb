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
#include "Actor/HandlerBase.h"
#include "Pregel/Worker/Messages.h"
#include "Pregel/Worker/State.h"
#include "Pregel/Conductor/Messages.h"
#include "Pregel/ResultMessages.h"

namespace arangodb::pregel::worker {

template<typename V, typename E, typename M, typename Runtime>
struct WorkerHandler : actor::HandlerBase<Runtime, WorkerState<V, E, M>> {
  auto operator()(message::WorkerStart start)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("cd696", INFO, Logger::PREGEL) << fmt::format(
        "Worker Actor {} started with state {}", this->self, *this->state);
    // TODO GORDO-1556
    // _feature.metrics()->pregelWorkersNumber->fetch_add(1);
    this->template dispatch<conductor::message::ConductorMessages>(
        this->state->conductor, ResultT<conductor::message::WorkerCreated>{});
    return std::move(this->state);
  }

  auto operator()(message::LoadGraph start)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("cd69c", INFO, Logger::PREGEL)
        << fmt::format("Worker Actor {} is loading", this->self);
    std::function<void()> statusUpdateCallback =
        [/*self = shared_from_this(),*/ this] {
          // TODO
          this->template dispatch<conductor::message::ConductorMessages>(
              this->state->conductor,
              conductor::message::StatusUpdate{
                  .executionNumber = this->state->config->executionNumber(),
                  .status = this->state->observeStatus()});
        };

    // TODO GORDO-1510
    // _feature.metrics()->pregelWorkersLoadingNumber->fetch_add(1);

    auto graphLoaded = [/*self = shared_from_this(),*/ this]()
        -> ResultT<conductor::message::GraphLoaded> {
      try {
        // TODO GORDO-1546
        // add graph store to state
        // this->state->graphStore->loadShards(&_config, statusUpdateCallback,
        // []() {});
        return {conductor::message::GraphLoaded{
            .executionNumber = this->state->config->executionNumber(),
            .vertexCount = 0,  // TODO GORDO-1546
                               // this->state->graphStore->localVertexCount(),
            .edgeCount = 0,    // TODO GORDO-1546
                               // this->state->_graphStore->localEdgeCount()
        }};
        LOG_TOPIC("5206c", WARN, Logger::PREGEL)
            << fmt::format("Worker {} has finished loading.", this->self);
      } catch (std::exception const& ex) {
        return Result{
            TRI_ERROR_INTERNAL,
            fmt::format("caught exception in loadShards: {}", ex.what())};
      } catch (...) {
        return Result{TRI_ERROR_INTERNAL,
                      "caught unknown exception exception in loadShards"};
      }
    };
    this->template dispatch<conductor::message::ConductorMessages>(
        this->state->conductor, graphLoaded());
    // TODO GORDO-1510
    // _feature.metrics()->pregelWorkersLoadingNumber->fetch_sub(1);
    return std::move(this->state);
  }

  auto operator()(message::RunGlobalSuperStep message)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("0f658", INFO, Logger::PREGEL)
        << fmt::format("Worker Actor {} is computing", this->self);
    // prepare
    // process vertices in threads
    // finish
    return std::move(this->state);
  }

  auto operator()(message::ProduceResults msg)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    std::string tmp;

    VPackBuilder results;
    results.openArray(/*unindexed*/ true);
    // TODO: re-enable as soon as we do have access to the config and graphstore
    //  Ticket: [GORDO-1546] see details here.
    /*for (auto& vertex : this->state->graphStore->quiver()) {
      TRI_ASSERT(vertex.shard().value <
                 this->state->config->globalShardIDs().size());
      ShardID const& shardId =
          this->state->config->globalShardID(vertex.shard());

      results.openObject(true);

      if (msg.withID) {
        std::string const& cname =
            this->state->config->shardIDToCollectionName(shardId);
        if (!cname.empty()) {
          tmp.clear();
          tmp.append(cname);
          tmp.push_back('/');
          tmp.append(vertex.key().data(), vertex.key().size());
          results.add(StaticStrings::IdString, VPackValue(tmp));
        }
      }

      results.add(StaticStrings::KeyString,
                  VPackValuePair(vertex.key().data(), vertex.key().size(),
                                 VPackValueType::String));

      V const& data = vertex.data();
      if (auto res =
              this->state->graphStore->graphFormat()->buildVertexDocument(
                  results, &data);
          !res) {
        std::string const failureString = "Failed to build vertex document";
        LOG_TOPIC("eee4d", ERR, Logger::PREGEL) << failureString;
        this->template dispatch<message::WorkerMessages>(
            this->state->resultActor,
            pregel::message::SaveResults{
                .results = Result(TRI_ERROR_INTERNAL, failureString)});
        return;
      }
      results.close();
    }*/
    results.close();

    this->template dispatch<pregel::message::ResultMessages>(
        this->state->resultActor,
        pregel::message::SaveResults{.results = {PregelResults{results}}});
    this->template dispatch<pregel::conductor::message::ConductorMessages>(
        this->state->conductor, pregel::conductor::message::ResultCreated{
                                    .results = {PregelResults{results}}});

    return std::move(this->state);
  }

  auto operator()(actor::message::UnknownMessage unknown)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("7ee4d", INFO, Logger::PREGEL) << fmt::format(
        "Worker Actor: Error - sent unknown message to {}", unknown.receiver);
    return std::move(this->state);
  }

  auto operator()(actor::message::ActorNotFound notFound)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("2d647", INFO, Logger::PREGEL) << fmt::format(
        "Worker Actor: Error - receiving actor {} not found", notFound.actor);
    return std::move(this->state);
  }

  auto operator()(actor::message::NetworkError notFound)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("1c3d9", INFO, Logger::PREGEL) << fmt::format(
        "Worker Actor: Error - network error {}", notFound.message);
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("8b81a", INFO, Logger::PREGEL)
        << fmt::format("Worker Actor: Got unhandled message: {}", rest);
    return std::move(this->state);
  }
};

}  // namespace arangodb::pregel::worker
