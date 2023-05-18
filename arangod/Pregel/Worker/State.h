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
#include <utility>
#include <memory>

#include "Actor/ActorPID.h"
#include "Pregel/Algorithm.h"
#include "Pregel/CollectionSpecifications.h"
#include "Pregel/GraphStore/Magazine.h"
#include "Pregel/GraphStore/Quiver.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/OutgoingCache.h"
#include "Pregel/PregelOptions.h"
#include "Pregel/Worker/Messages.h"
#include "Pregel/WorkerContext.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/vocbase.h"
#include "Pregel/Status/Status.h"
#include "Pregel/GraphStore/Magazine.h"
#include "Pregel/Worker/ExecutionStates/InitialState.h"

namespace arangodb::pregel::worker {

template<typename V, typename E, typename M>
struct WorkerState {
  WorkerState(std::unique_ptr<WorkerContext> workerContext,
              actor::ActorPID conductor,
              const worker::message::CreateWorker& specifications,
              std::chrono::seconds messageTimeout,
              std::unique_ptr<MessageFormat<M>> newMessageFormat,
              std::unique_ptr<MessageCombiner<M>> newMessageCombiner,
              std::unique_ptr<Algorithm<V, E, M>> algorithm,
              TRI_vocbase_t& vocbase, actor::ActorPID spawnActor,
              actor::ActorPID resultActor, actor::ActorPID statusActor,
              actor::ActorPID metricsActor)
      : config{std::make_shared<WorkerConfig>(&vocbase)},
        workerContext{std::move(workerContext)},
        messageTimeout{messageTimeout},
        messageFormat{std::move(newMessageFormat)},
        messageCombiner{std::move(newMessageCombiner)},
        conductor{std::move(conductor)},
        algorithm{std::move(algorithm)},
        vocbaseGuard{vocbase},
        spawnActor(std::move(spawnActor)),
        resultActor(std::move(resultActor)),
        statusActor(std::move(statusActor)),
        metricsActor(std::move(metricsActor)) {
    executionState = std::make_unique<Initial<V, E, M>>(*this);
    config->updateConfig(specifications);

    if (messageCombiner) {
      readCache = std::make_unique<CombiningInCache<M>>(
          config->localPregelShardIDs(), messageFormat.get(),
          messageCombiner.get());
      writeCache = std::make_unique<CombiningInCache<M>>(
          config->localPregelShardIDs(), messageFormat.get(),
          messageCombiner.get());
    } else {
      readCache = std::make_unique<ArrayInCache<M>>(
          config->localPregelShardIDs(), messageFormat.get());
      writeCache = std::make_unique<ArrayInCache<M>>(
          config->localPregelShardIDs(), messageFormat.get());
    }
  }

  std::shared_ptr<WorkerConfig> config;

  // only needed in computing state
  std::unique_ptr<WorkerContext> workerContext;
  std::chrono::seconds messageTimeout;
  std::optional<std::chrono::steady_clock::time_point>
      isWaitingForAllMessagesSince;
  std::unique_ptr<MessageFormat<M>> messageFormat;
  std::unique_ptr<MessageCombiner<M>> messageCombiner;
  std::unique_ptr<InCache<M>> readCache = nullptr;
  std::unique_ptr<InCache<M>> writeCache = nullptr;
  uint32_t messageBatchSize = 500;
  std::unordered_map<ShardID, actor::ActorPID> responsibleActorPerShard;

  std::unique_ptr<ExecutionState> executionState;
  const actor::ActorPID conductor;
  std::unique_ptr<Algorithm<V, E, M>> algorithm;
  const DatabaseGuard vocbaseGuard;
  const actor::ActorPID spawnActor;
  const actor::ActorPID resultActor;
  const actor::ActorPID statusActor;
  const actor::ActorPID metricsActor;
  Magazine<V, E> magazine;
  MessageStats messageStats;
};
template<typename V, typename E, typename M, typename Inspector>
auto inspect(Inspector& f, WorkerState<V, E, M>& x) {
  return f.object(x).fields(f.field("conductor", x.conductor),
                            f.field("algorithm", x.algorithm->name()));
}

}  // namespace arangodb::pregel::worker

template<typename V, typename E, typename M>
struct fmt::formatter<arangodb::pregel::worker::WorkerState<V, E, M>>
    : arangodb::inspection::inspection_formatter {};
