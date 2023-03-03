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

#include <map>

#include "Inspection/Format.h"
#include "Inspection/Types.h"
#include "Pregel/ExecutionNumber.h"
#include "Pregel/Utils.h"

namespace arangodb::pregel {

struct ConductorStart {};
template<typename Inspector>
auto inspect(Inspector& f, ConductorStart& x) {
  return f.object(x).fields();
}
struct WorkerCreated {};
template<typename Inspector>
auto inspect(Inspector& f, WorkerCreated& x) {
  return f.object(x).fields();
}
struct ConductorMessages : std::variant<ConductorStart, WorkerCreated> {
  using std::variant<ConductorStart, WorkerCreated>::variant;
};
template<typename Inspector>
auto inspect(Inspector& f, ConductorMessages& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<ConductorStart>("Start"),
      arangodb::inspection::type<WorkerCreated>("WorkerCreated"));
}

// TODO split LoadGraph off CreateWorker
struct CreateWorker {
  ExecutionNumber executionNumber;
  std::string algorithm;
  VPackBuilder userParameters;
  std::string coordinatorId;
  bool useMemoryMaps;
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
      f.field("useMemoryMaps", x.useMemoryMaps),
      f.field("edgeCollectionRestrictions", x.edgeCollectionRestrictions),
      f.field("vertexShards", x.vertexShards),
      f.field("edgeShards", x.edgeShards),
      f.field("collectionPlanIds", x.collectionPlanIds),
      f.field("allShards", x.allShards));
}

struct PrepareGlobalSuperStep {
  ExecutionNumber executionNumber;
  uint64_t gss;
  uint64_t vertexCount;
  uint64_t edgeCount;
};
template<typename Inspector>
auto inspect(Inspector& f, PrepareGlobalSuperStep& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field(Utils::globalSuperstepKey, x.gss),
      f.field("vertexCount", x.vertexCount), f.field("edgeCount", x.edgeCount));
}

struct RunGlobalSuperStep {
  ExecutionNumber executionNumber;
  uint64_t gss;
  uint64_t vertexCount;
  uint64_t edgeCount;
  VPackBuilder aggregators;
};

template<typename Inspector>
auto inspect(Inspector& f, RunGlobalSuperStep& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field(Utils::globalSuperstepKey, x.gss),
      f.field("vertexCount", x.vertexCount), f.field("edgeCount", x.edgeCount),
      f.field("aggregators", x.aggregators));
}

// TODO split into Store and Cleanup
struct FinalizeExecution {
  ExecutionNumber executionNumber;
  bool store;
};
template<typename Inspector>
auto inspect(Inspector& f, FinalizeExecution& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("store", x.store));
}

struct CollectPregelResults {
  ExecutionNumber executionNumber;
  bool withId;
};
template<typename Inspector>
auto inspect(Inspector& f, CollectPregelResults& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("withId", x.withId).fallback(false));
}

}  // namespace arangodb::pregel

template<>
struct fmt::formatter<arangodb::pregel::PrepareGlobalSuperStep>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::pregel::RunGlobalSuperStep>
    : arangodb::inspection::inspection_formatter {};
