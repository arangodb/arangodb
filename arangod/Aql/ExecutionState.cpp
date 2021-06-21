////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ExecutionState.h"

#include <ostream>

namespace arangodb {
namespace aql {

std::ostream& operator<<(std::ostream& ostream, ExecutionState state) {
  switch (state) {
    case ExecutionState::DONE:
      ostream << "DONE";
      break;
    case ExecutionState::HASMORE:
      ostream << "HASMORE";
      break;
    case ExecutionState::WAITING:
      ostream << "WAITING";
      break;
  }
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream, ExecutorState state) {
  switch (state) {
    case ExecutorState::DONE:
      ostream << "DONE";
      break;
    case ExecutorState::HASMORE:
      ostream << "HASMORE";
      break;
    default:
      ostream << " WAT WAT WAT";
      break;
  }
  return ostream;
}

}  // namespace aql
}  // namespace arangodb
