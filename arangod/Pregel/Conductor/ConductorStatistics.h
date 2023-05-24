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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Inspection/Format.h"
#include "Inspection/Types.h"
// #include "Pregel/StatusActor.h"
#include "Pregel/Status/ExecutionStatus.h"
#include "Pregel/Status/ConductorStatus.h"

namespace arangodb::pregel {

struct NetworkStruct {
  size_t receivedCount;
  size_t sendCount;
};

struct FinishedTimingsStruct {
  double totalRuntime;
  double startupTime;
  double computationTime;
  double storageTime;
  std::vector<double> gssTimes;
};

struct ConductorStatistics {
  std::string id;
  std::string database;
  std::optional<std::string> algorithm;
  std::string created;
  std::string expires;
  long long ttl;
  std::string state;
  size_t gss;
  bool graphLoaded;
  std::string user;
  FinishedTimingsStruct timingsStruct;
  VPackBuilder aggregators;  // This can be optimized after deprecation of DMID
  NetworkStruct network;
  std::optional<size_t> vertexCount;
  std::optional<size_t> edgeCount;
  size_t parallelism;
  AccumulatedConductorStatus detail;
};

template<typename Inspector>
auto inspect(Inspector& f, FinishedTimingsStruct& x) {
  return f.object(x).fields(f.field("totalRuntime", x.totalRuntime),
                            f.field("startupTime", x.startupTime),
                            f.field("computationTime", x.computationTime),
                            f.field("storageTime", x.storageTime),
                            f.field("gssTimes", x.gssTimes));
}

template<typename Inspector>
auto inspect(Inspector& f, NetworkStruct& x) {
  return f.object(x).fields(f.field("receivedCount", x.receivedCount),
                            f.field("sendCount", x.sendCount));
}

template<typename Inspector>
auto inspect(Inspector& f, ConductorStatistics& x) {
  return f.object(x).fields(
      f.field("id", x.id), f.field("database", x.database),
      f.field("algorithm", x.algorithm), f.field("created", x.created),
      f.field("expires", x.expires), f.field("ttl", x.ttl),
      f.field("state", x.state), f.field("gss", x.gss),
      f.field("graphLoaded", x.graphLoaded), f.field("user", x.user),
      f.embedFields(x.timingsStruct), f.field("aggregators", x.aggregators),
      f.embedFields(x.network), f.field("vertexCount", x.vertexCount),
      f.field("edgeCount", x.edgeCount), f.field("parallelism", x.parallelism),
      // TODO _masterContext serializeValues
      f.field("detail", x.detail)

  );
}
};  // namespace arangodb::pregel
