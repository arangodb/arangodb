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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ExecutionState.h"

#include "Assertions/ProdAssert.h"

#include <ostream>

namespace arangodb::aql {

auto toStringView(ExecutionState state) -> std::string_view {
  switch (state) {
    case ExecutionState::DONE:
      return "DONE";
    case ExecutionState::HASMORE:
      return "HASMORE";
    case ExecutionState::WAITING:
      return "WAITING";
  }
  ADB_PROD_ASSERT(false)
      << "Unhandled state "
      << static_cast<std::underlying_type_t<decltype(state)>>(state);
  std::abort();
}
auto toStringView(ExecutorState state) -> std::string_view {
  switch (state) {
    case ExecutorState::DONE:
      return "DONE";
    case ExecutorState::HASMORE:
      return "HASMORE";
  }
  ADB_PROD_ASSERT(false)
      << "Unhandled state "
      << static_cast<std::underlying_type_t<decltype(state)>>(state);
  std::abort();
}

std::ostream& operator<<(std::ostream& ostream, ExecutionState state) {
  return ostream << toStringView(state);
}

std::ostream& operator<<(std::ostream& ostream, ExecutorState state) {
  return ostream << toStringView(state);
}

}  // namespace arangodb::aql
