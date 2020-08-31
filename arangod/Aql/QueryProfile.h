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

#ifndef ARANGOD_AQL_QUERY_PROFILE_H
#define ARANGOD_AQL_QUERY_PROFILE_H 1

#include "Aql/QueryExecutionState.h"
#include "Basics/Common.h"

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

  explicit QueryProfile(Query* query);

  ~QueryProfile();

 public:
  
  void registerInQueryList();
  
  /// @brief unregister the query from the list of queries, if entered
  void unregisterFromQueryList() noexcept;

  double setStateDone(QueryExecutionState::ValueType);

  /// @brief sets the absolute end time for an execution state
  void setStateEnd(QueryExecutionState::ValueType state, double time);

  /// @brief get a timer time
  inline double timer(QueryExecutionState::ValueType t) const {
    return _timers[QueryExecutionState::toNumber(t)];
  }

  /// @brief convert the profile to VelocyPack
  void toVelocyPack(arangodb::velocypack::Builder&) const;
  
 private:

 private:
  Query* _query;
  std::array<double, static_cast<size_t>(QueryExecutionState::ValueType::INVALID_STATE)> _timers;
  double _lastStamp;
  bool _tracked;
};

// we want the number of execution states to be quite low
// as we reserve a statically sized array for it
static_assert(static_cast<int>(QueryExecutionState::ValueType::INITIALIZATION) == 0,
              "unexpected min QueryExecutionState enum value");
static_assert(static_cast<int>(QueryExecutionState::ValueType::INVALID_STATE) < 11,
              "unexpected max QueryExecutionState enum value");

}  // namespace aql
}  // namespace arangodb

#endif
