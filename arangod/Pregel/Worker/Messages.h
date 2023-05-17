////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "Actor/ActorPID.h"
#include "Inspection/Format.h"
#include "Inspection/Types.h"
#include "Pregel/CollectionSpecifications.h"
#include "Pregel/DatabaseTypes.h"
#include "Pregel/ExecutionNumber.h"
#include "Pregel/GraphStore/Graph.h"
#include "Pregel/PregelOptions.h"
#include "Pregel/Statistics.h"
#include "Pregel/Status/Status.h"

namespace arangodb::pregel {

namespace worker::message {

struct CreateWorker {
  ExecutionNumber executionNumber;
  std::string algorithm;
  VPackBuilder userParameters;
  std::string coordinatorId;
  size_t parallelism;
  std::unordered_map<CollectionID, std::vector<ShardID>>
      edgeCollectionRestrictions;
  std::map<CollectionID, std::vector<ShardID>> vertexShards;
  std::map<CollectionID, std::vector<ShardID>> edgeShards;
  std::unordered_map<CollectionID, std::string> collectionPlanIds;
  std::vector<ShardID> allShards;
};
template<typename Inspector>
auto inspect(Inspector& f, CreateWorker& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("algorithm", x.algorithm),
      f.field("userParameters", x.userParameters),
      f.field("coordinatorId", x.coordinatorId),
      f.field("parallelism", x.parallelism),
      f.field("edgeCollectionRestrictions", x.edgeCollectionRestrictions),
      f.field("vertexShards", x.vertexShards),
      f.field("edgeShards", x.edgeShards),
      f.field("collectionPlanIds", x.collectionPlanIds),
      f.field("allShards", x.allShards));
}

struct WorkerStart {};
template<typename Inspector>
auto inspect(Inspector& f, WorkerStart& x) {
  return f.object(x).fields();
}

struct LoadGraph {
  std::unordered_map<ShardID, actor::ActorPID> responsibleActorPerShard;
};
template<typename Inspector>
auto inspect(Inspector& f, LoadGraph& x) {
  return f.object(x).fields(
      f.field("responsibleActorPerShard", x.responsibleActorPerShard));
}

struct RunGlobalSuperStep {
  uint64_t gss;
  uint64_t vertexCount;
  uint64_t edgeCount;
  uint64_t sendCount;
  VPackBuilder aggregators;
};
template<typename Inspector>
auto inspect(Inspector& f, RunGlobalSuperStep& x) {
  return f.object(x).fields(
      f.field("globalSuperStep", x.gss), f.field("vertexCount", x.vertexCount),
      f.field("edgeCount", x.edgeCount), f.field("sendCount", x.sendCount),
      f.field("aggregators", x.aggregators));
}

struct PregelMessage {
  ExecutionNumber executionNumber;
  uint64_t gss;
  PregelShard shard;
  VPackBuilder messages;
};
template<typename Inspector>
auto inspect(Inspector& f, PregelMessage& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("globalSuperStep", x.gss), f.field("shard", x.shard),
      f.field("messages", x.messages));
}

struct Store {};
template<typename Inspector>
auto inspect(Inspector& f, Store& x) {
  return f.object(x).fields();
}

struct ProduceResults {
  bool withID = true;
};
template<typename Inspector>
auto inspect(Inspector& f, ProduceResults& x) {
  return f.object(x).fields(f.field("withID", x.withID));
}

struct Cleanup {};
template<typename Inspector>
auto inspect(Inspector& f, Cleanup& x) {
  return f.object(x).fields();
}

struct WorkerMessages
    : std::variant<WorkerStart, CreateWorker, LoadGraph, RunGlobalSuperStep,
                   PregelMessage, Store, ProduceResults, Cleanup> {
  using std::variant<WorkerStart, CreateWorker, LoadGraph, RunGlobalSuperStep,
                     PregelMessage, Store, ProduceResults, Cleanup>::variant;
};
template<typename Inspector>
auto inspect(Inspector& f, WorkerMessages& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<WorkerStart>("Start"),
      arangodb::inspection::type<CreateWorker>("CreateWorker"),
      arangodb::inspection::type<LoadGraph>("LoadGraph"),
      arangodb::inspection::type<RunGlobalSuperStep>("RunGlobalSuperStep"),
      arangodb::inspection::type<PregelMessage>("PregelMessage"),
      arangodb::inspection::type<Store>("Store"),
      arangodb::inspection::type<ProduceResults>("ProduceResults"),
      arangodb::inspection::type<Cleanup>("Cleanup"));
}

}  // namespace worker::message

struct GraphLoaded {
  ExecutionNumber executionNumber;
  std::string sender;
  uint64_t vertexCount;
  uint64_t edgeCount;
};

template<typename Inspector>
auto inspect(Inspector& f, GraphLoaded& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("sender", x.sender), f.field("vertexCount", x.vertexCount),
      f.field("edgeCount", x.edgeCount));
}

struct GlobalSuperStepPrepared {
  ExecutionNumber executionNumber;
  std::string sender;
  uint64_t activeCount;
  uint64_t vertexCount;
  uint64_t edgeCount;
  VPackBuilder aggregators;
};
template<typename Inspector>
auto inspect(Inspector& f, GlobalSuperStepPrepared& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("sender", x.sender), f.field("activeCount", x.activeCount),
      f.field("vertexCount", x.vertexCount), f.field("edgeCount", x.edgeCount),
      f.field("aggregators", x.aggregators));
}

struct GlobalSuperStepFinished {
  ExecutionNumber executionNumber;
  std::string sender;
  uint64_t gss;
  MessageStats messageStats;
};

template<typename Inspector>
auto inspect(Inspector& f, GlobalSuperStepFinished& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("sender", x.sender), f.field("gss", x.gss),
      f.field("messageStats", x.messageStats));
}

struct Finished {
  ExecutionNumber executionNumber;
  std::string sender;
};
template<typename Inspector>
auto inspect(Inspector& f, Finished& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("sender", x.sender));
}

struct StatusUpdated {
  ExecutionNumber executionNumber;
  std::string sender;
  Status status;
};
template<typename Inspector>
auto inspect(Inspector& f, StatusUpdated& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("sender", x.sender), f.field("status", x.status));
}

struct PregelResults {
  VPackBuilder results;
};
template<typename Inspector>
auto inspect(Inspector& f, PregelResults& x) {
  return f.object(x).fields(f.field("results", x.results));
}

}  // namespace arangodb::pregel

template<>
struct fmt::formatter<arangodb::pregel::StatusUpdated>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::pregel::GlobalSuperStepFinished>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::pregel::worker::message::WorkerStart>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::pregel::worker::message::CreateWorker>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::pregel::worker::message::LoadGraph>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::pregel::worker::message::RunGlobalSuperStep>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::pregel::worker::message::PregelMessage>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::pregel::worker::message::WorkerMessages>
    : arangodb::inspection::inspection_formatter {};
