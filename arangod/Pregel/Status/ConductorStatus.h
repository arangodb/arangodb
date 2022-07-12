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
#include <string>

#include <Inspection/VPack.h>
#include <Inspection/Transformers.h>

#include <Cluster/ClusterTypes.h>

#include <Pregel/Common.h>
#include <Pregel/Status/WorkerStatus.h>

namespace arangodb::pregel {

struct ConductorStatus {
  TimeStamp timeStamp = std::chrono::system_clock::now();
  std::unordered_map<ServerID, WorkerStatus> workers;

  static auto forWorkers(std::vector<ServerID> const& ids) -> ConductorStatus {
    auto status = ConductorStatus{};
    for (auto const& id : ids) {
      status.workers.emplace(id, WorkerStatus{});
    }
    return status;
  }
  auto updateWorkerStatus(ServerID const& id, WorkerStatus const& status)
      -> void {
    workers.at(id) = status;
    timeStamp = status.timeStamp;
  }
};

template<typename Inspector>
auto inspect(Inspector& f, ConductorStatus& x) {
  return f.object(x).fields(
      f.field(timeStampString, x.timeStamp)
          .transformWith(inspection::TimeStampTransformer{}),
      f.field("workerStatus", x.workers));
}

}  // namespace arangodb::pregel
