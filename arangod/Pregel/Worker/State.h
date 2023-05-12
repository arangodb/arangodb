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
#include "Actor/ActorPID.h"
#include "Pregel/Algorithm.h"
#include "Pregel/CollectionSpecifications.h"
#include "Pregel/GraphStore/Quiver.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/OutgoingCache.h"
#include "Pregel/PregelOptions.h"
#include "Pregel/Worker/Messages.h"
#include "Pregel/WorkerContext.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/vocbase.h"
#include "Pregel/Status/Status.h"

namespace arangodb::pregel::worker {

template<typename V, typename E, typename M>
struct WorkerState {
  WorkerState(std::unique_ptr<WorkerContext> workerContext,
              actor::ActorPID conductor,
              worker::message::CreateWorker specifications,
              std::chrono::seconds messageTimeout,
              std::unique_ptr<MessageFormat<M>> newMessageFormat,
              std::unique_ptr<MessageCombiner<M>> newMessageCombiner,
              std::unique_ptr<Algorithm<V, E, M>> algorithm,
              TRI_vocbase_t& vocbase, actor::ActorPID spawnActor,
              actor::ActorPID resultActor, actor::ActorPID statusActor)
      : config{std::make_shared<WorkerConfig>(&vocbase)},
        workerContext{std::move(workerContext)},
        messageTimeout{std::move(messageTimeout)},
        messageFormat{std::move(newMessageFormat)},
        messageCombiner{std::move(newMessageCombiner)},
        conductor{std::move(conductor)},
        algorithm{std::move(algorithm)},
        vocbaseGuard{vocbase},
        spawnActor(spawnActor),
        resultActor(resultActor),
        statusActor(statusActor) {
    config->updateConfig(specifications);

    if (messageCombiner) {
      readCache = std::make_unique<CombiningInCache<M>>(
          config->localPregelShardIDs(), messageFormat.get(),
          messageCombiner.get());
      writeCache = std::make_unique<CombiningInCache<M>>(
          config->localPregelShardIDs(), messageFormat.get(),
          messageCombiner.get());
      inCache = std::make_unique<CombiningInCache<M>>(
          containers::FlatHashSet<PregelShard>{}, messageFormat.get(),
          messageCombiner.get());
      outCache = std::make_unique<CombiningOutActorCache<M>>(
          config, messageFormat.get(), messageCombiner.get());
    } else {
      readCache = std::make_unique<ArrayInCache<M>>(
          config->localPregelShardIDs(), messageFormat.get());
      writeCache = std::make_unique<ArrayInCache<M>>(
          config->localPregelShardIDs(), messageFormat.get());
      inCache = std::make_unique<ArrayInCache<M>>(
          containers::FlatHashSet<PregelShard>{}, messageFormat.get());
      outCache =
          std::make_unique<ArrayOutActorCache<M>>(config, messageFormat.get());
    }
  }

  std::shared_ptr<WorkerConfig> config;

  // only needed in computing state
  std::unique_ptr<WorkerContext> workerContext;
  std::chrono::seconds messageTimeout;
  std::vector<worker::message::PregelMessage> messagesForNextGss;
  // distinguishes config.globalSuperStep being initialized to 0 and
  // config.globalSuperStep being explicitely set to 0 when the first superstep
  // starts: needed to process incoming messages in its dedicated gss
  bool computationStarted = false;
  std::optional<std::chrono::steady_clock::time_point>
      isWaitingForAllMessagesSince;
  std::unique_ptr<MessageFormat<M>> messageFormat;
  std::unique_ptr<MessageCombiner<M>> messageCombiner;
  std::unique_ptr<InCache<M>> readCache = nullptr;
  std::unique_ptr<InCache<M>> writeCache = nullptr;
  std::unique_ptr<InCache<M>> inCache = nullptr;
  std::unique_ptr<OutCache<M>> outCache = nullptr;
  uint32_t messageBatchSize = 500;

  const actor::ActorPID conductor;
  std::unique_ptr<Algorithm<V, E, M>> algorithm;
  const DatabaseGuard vocbaseGuard;
  const actor::ActorPID spawnActor;
  const actor::ActorPID resultActor;
  const actor::ActorPID statusActor;
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
