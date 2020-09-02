////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "QueryExecutionState.h"

#include <string>

#include "Basics/debugging.h"

using namespace arangodb::aql;

/// @brief names of query phases / states
static std::string const StateNames[] = {
    "initializing",         // INITIALIZATION
    "parsing",              // PARSING
    "optimizing ast",       // AST_OPTIMIZATION
    "loading collections",  // LOADING_COLLECTIONS
    "instantiating plan",   // PLAN_INSTANTIATION
    "optimizing plan",      // PLAN_OPTIMIZATION
    "executing",            // EXECUTION
    "finalizing",           // FINALIZATION
    "finished",             // FINISHED
    "killed",               // KILLED

    "invalid"  // INVALID
};

// make sure the state strings and the actual states match
static_assert(sizeof(StateNames) / sizeof(std::string) ==
                  static_cast<size_t>(QueryExecutionState::ValueType::INVALID_STATE) + 1,
              "invalid number of ExecutionState values");

QueryExecutionState::ValueType QueryExecutionState::fromNumber(size_t value) {
  TRI_ASSERT(value < static_cast<size_t>(QueryExecutionState::ValueType::INVALID_STATE));

  return static_cast<QueryExecutionState::ValueType>(value);
}

size_t QueryExecutionState::toNumber(QueryExecutionState::ValueType value) {
  return static_cast<size_t>(value);
}

/// @brief get a description of the query's current state
std::string const& QueryExecutionState::toString(QueryExecutionState::ValueType state) {
  return StateNames[static_cast<int>(state)];
}

/// @brief get a description of the query's current state, prefixed with "
/// (while "
std::string QueryExecutionState::toStringWithPrefix(QueryExecutionState::ValueType state) {
  return std::string(" (while " + StateNames[static_cast<int>(state)] + ")");
}

std::ostream& operator<<(std::ostream& stream,
                         arangodb::aql::QueryExecutionState::ValueType state) {
  stream << StateNames[static_cast<int>(state)];
  return stream;
}
