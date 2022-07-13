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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string>

#include <Inspection/VPack.h>
#include <Inspection/Transformers.h>

#include <Cluster/ClusterTypes.h>

#include <Pregel/Common.h>
#include <Pregel/Status/Status.h>

namespace arangodb::pregel {

struct AccumulatedConductorStatus {
  Status status;
  std::unordered_map<ServerID, Status> workers;
  bool operator==(AccumulatedConductorStatus const&) const = default;
};

template<typename Inspector>
auto inspect(Inspector& f, AccumulatedConductorStatus& x) {
  return f.object(x).fields(
      f.field("aggregatedStatus",
              x.status),  // TODO status entries should be on top level
      f.field("workerStatus", x.workers));
}

struct ConductorStatus {
  std::unordered_map<ServerID, Status> workers;
  static auto forWorkers(std::vector<ServerID> const& ids) -> ConductorStatus {
    auto status = ConductorStatus{};
    for (auto const& id : ids) {
      status.workers.emplace(id, Status{});
    }
    return status;
  }
  auto updateWorkerStatus(ServerID const& id, Status&& status) -> void {
    workers.at(id) = status;
  }
  auto accumulate() const -> AccumulatedConductorStatus {
    auto aggregate = std::accumulate(
        workers.begin(), workers.end(), Status{.timeStamp = TimeStamp::min()},
        [](Status const& accumulation,
           std::unordered_map<ServerID, Status>::value_type const& workers) {
          return accumulation + workers.second;
        });
    return AccumulatedConductorStatus{.status = aggregate, .workers = workers};
  }
};

}  // namespace arangodb::pregel
