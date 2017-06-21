////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_QUERY_PROFILE_H
#define ARANGOD_AQL_QUERY_PROFILE_H 1

#include "Basics/Common.h"
#include "Aql/QueryExecutionState.h"

#include <array>

namespace arangodb {

namespace velocypack {
class Builder;
}

namespace aql {
class Query;

struct QueryProfile {
  QueryProfile(QueryProfile const&) = delete;
  QueryProfile& operator=(QueryProfile const&) = delete;

  explicit QueryProfile(Query*);

  ~QueryProfile();

  double setDone(QueryExecutionState::ValueType);

  /// @brief sets the absolute end time for an execution state
  void setEnd(QueryExecutionState::ValueType state, double time);

  /// @brief convert the profile to VelocyPack
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack();

  Query* query;
  std::array<double, static_cast<size_t>(QueryExecutionState::ValueType::INVALID_STATE)> timers;
  double stamp;
  bool tracked;
};

// we want the number of execution states to be quite low
// as we reserve a statically sized array for it
static_assert(static_cast<int>(QueryExecutionState::ValueType::INITIALIZATION) == 0, "unexpected min QueryExecutionState enum value");
static_assert(static_cast<int>(QueryExecutionState::ValueType::INVALID_STATE) < 10, "unexpected max QueryExecutionState enum value");

}
}

#endif
