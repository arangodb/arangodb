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

#include <variant>

#include "Pregel/Actor/Message.h"
#include "Pregel/Messaging/ConductorMessages.h"
#include "Pregel/Messaging/WorkerMessages.h"

namespace arangodb::pregel {

struct Ok {};
template<typename Inspector>
auto inspect(Inspector& f, Ok& x) {
  return f.object(x).fields();
}

using MessagePayload =
    std::variant<Ok, CreateWorker, ResultT<WorkerCreated>, LoadGraph,
                 ResultT<GraphLoaded>, PrepareGlobalSuperStep,
                 ResultT<GlobalSuperStepPrepared>, RunGlobalSuperStep,
                 ResultT<GlobalSuperStepFinished>, Store, ResultT<Stored>,
                 Cleanup, ResultT<CleanupFinished>, CollectPregelResults,
                 ResultT<PregelResults>, StatusUpdated, PregelMessage,
                 actor::NetworkMessage>;

struct MessagePayloadSerializer : MessagePayload {};
template<class Inspector>
auto inspect(Inspector& f, MessagePayloadSerializer& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<Ok>("ok"),
      arangodb::inspection::type<CreateWorker>("createWorker"),
      arangodb::inspection::type<ResultT<WorkerCreated>>("workerCreated"),
      arangodb::inspection::type<LoadGraph>("loadGraph"),
      arangodb::inspection::type<ResultT<GraphLoaded>>("graphLoaded"),
      arangodb::inspection::type<PrepareGlobalSuperStep>(
          "prepareGlobalSuperStep"),
      arangodb::inspection::type<ResultT<GlobalSuperStepPrepared>>(
          "globalSuperStepPrepared"),
      arangodb::inspection::type<RunGlobalSuperStep>("runGlobalSuperStep"),
      arangodb::inspection::type<ResultT<GlobalSuperStepFinished>>(
          "globalSuperStepFinished"),
      arangodb::inspection::type<Store>("store"),
      arangodb::inspection::type<ResultT<Stored>>("stored"),
      arangodb::inspection::type<Cleanup>("cleanup"),
      arangodb::inspection::type<ResultT<CleanupFinished>>("cleanupFinished"),
      arangodb::inspection::type<CollectPregelResults>("collectPregelResults"),
      arangodb::inspection::type<ResultT<PregelResults>>("pregelResults"),
      arangodb::inspection::type<PregelMessage>("pregelMessage"),
      arangodb::inspection::type<StatusUpdated>("statusUpdated"),
      arangodb::inspection::type<actor::NetworkMessage>("actorNetworkMessage"));
}
template<typename Inspector>
auto inspect(Inspector& f, MessagePayload& x) {
  if constexpr (Inspector::isLoading) {
    auto v = MessagePayloadSerializer{};
    auto res = f.apply(v);
    x = MessagePayload{v};
    return res;
  } else {
    auto v = MessagePayloadSerializer{x};
    return f.apply(v);
  }
}

struct ModernMessage {
  ExecutionNumber executionNumber;
  MessagePayload payload;
};
template<typename Inspector>
auto inspect(Inspector& f, ModernMessage& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("payload", x.payload));
}

}  // namespace arangodb::pregel
