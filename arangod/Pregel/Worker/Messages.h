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

#include "Cluster/ClusterTypes.h"
#include "Inspection/Format.h"
#include "Pregel/ExecutionNumber.h"
#include "Pregel/Graph/Graph.h"
#include "Pregel/Statistics.h"
#include "Pregel/Status/Status.h"

namespace arangodb::pregel {

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
      f.field(Utils::globalSuperstepKey, x.gss), f.field("shard", x.shard),
      f.field("messages", x.messages));
}

}  // namespace arangodb::pregel

template<>
struct fmt::formatter<arangodb::pregel::StatusUpdated>
    : arangodb::inspection::inspection_formatter {};
template<>
struct fmt::formatter<arangodb::pregel::GlobalSuperStepFinished>
    : arangodb::inspection::inspection_formatter {};
