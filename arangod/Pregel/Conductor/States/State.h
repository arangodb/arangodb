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
#include <optional>
#include <string>
#include <string_view>
#include "Pregel/WorkerConductorMessages.h"
#include "velocypack/Builder.h"

namespace arangodb::pregel {

struct Message;

namespace conductor {

#define LOG_PREGEL_CONDUCTOR(logId, level) \
  LOG_TOPIC(logId, level, Logger::PREGEL)  \
      << "[job " << conductor._executionNumber << "] "

struct State {
  virtual auto run() -> std::optional<std::unique_ptr<State>> = 0;
  virtual auto canBeCanceled() -> bool = 0;
  virtual auto getResults(bool withId) -> ResultT<PregelResults> {
    VPackBuilder emptyArray;
    { VPackArrayBuilder ab(&emptyArray); }
    return PregelResults{emptyArray};
  };
  virtual auto name() const -> std::string = 0;
  virtual auto isRunning() const -> bool = 0;
  virtual auto getExpiration() const
      -> std::optional<std::chrono::system_clock::time_point> = 0;
  virtual ~State(){};
};

}  // namespace conductor
}  // namespace arangodb::pregel
