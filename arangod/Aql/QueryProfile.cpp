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

#include "QueryProfile.h"

#include "Aql/Query.h"
#include "Aql/QueryList.h"
#include "Aql/Timing.h"
#include "Basics/EnumIterator.h"
#include "Basics/debugging.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

/// @brief create a profile
QueryProfile::QueryProfile(Query* query)
    : _query(query), 
      _lastStamp(query->startTime()), 
      _tracked(false) {
  for (auto& it : _timers) {
    it = 0.0;  // reset timers
  }
}

/// @brief destroy a profile
QueryProfile::~QueryProfile() {
  unregisterFromQueryList();
}

void QueryProfile::registerInQueryList() {
  TRI_ASSERT(!_tracked);
  auto queryList = _query->vocbase().queryList();
  if (queryList) {
    _tracked = queryList->insert(_query);
  }
}

void QueryProfile::unregisterFromQueryList() noexcept {
  // only remove from list when the query was inserted into it...
  if (_tracked) {
    auto queryList = _query->vocbase().queryList();

    try {
      queryList->remove(_query);
    } catch (...) {
    }

    _tracked = false;
  }
}

/// @brief sets a state to done
double QueryProfile::setStateDone(QueryExecutionState::ValueType state) {
  double const now = arangodb::aql::currentSteadyClockValue();

  if (state != QueryExecutionState::ValueType::INVALID_STATE &&
      state != QueryExecutionState::ValueType::KILLED) {
    // record duration of state
    _timers[static_cast<int>(state)] = now - _lastStamp;
  }

  // set timestamp
  _lastStamp = now;
  return now;
}

/// @brief sets the absolute end time for an execution state
void QueryProfile::setStateEnd(QueryExecutionState::ValueType state, double time) {
  _timers[static_cast<int>(state)] = time - _lastStamp;
}

/// @brief convert the profile to VelocyPack
void QueryProfile::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder, "profile", true);
  for (auto state : ENUM_ITERATOR(QueryExecutionState::ValueType, INITIALIZATION, FINALIZATION)) {
    double const value = _timers[static_cast<size_t>(state)];

    if (value >= 0.0) {
      builder.add(QueryExecutionState::toString(state), VPackValue(value));
    }
  }
}
