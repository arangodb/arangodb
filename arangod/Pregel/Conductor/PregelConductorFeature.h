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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#pragma once

//#include <atomic>
#include <chrono>
//#include <cstdint>
//#include <memory>
#include <string>
//#include <unordered_map>
//#include <vector>

//#include <velocypack/Builder.h>
//#include <velocypack/Slice.h>

//#include "Basics/Common.h"
//#include "Basics/Mutex.h"
//#include "Basics/ResultT.h"
#include "Pregel/ExecutionNumber.h"
//#include "Pregel/PregelMetrics.h"
//#include "ProgramOptions/ProgramOptions.h"
//#include "RestServer/arangod.h"
//#include "Scheduler/Scheduler.h"
//#include "Cluster/ServerState.h"
//#include "Utils/ExecContext.h"
#include "RestServer/arangod.h"

namespace arangodb::pregel {

class Conductor;

class PregelConductorFeature : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept {
    return "PregelConductor";
  }

  explicit PregelConductorFeature(Server& server);
//  ~PregelConductorFeature() override = default;

//  struct ConductorEntry {
//    std::string user;
//    std::chrono::steady_clock::time_point expires;
//    std::shared_ptr<Conductor> conductor;
//  };

//  std::unordered_map<ExecutionNumber, ConductorEntry> _conductors;
};

} // namespace arangodb::pregel