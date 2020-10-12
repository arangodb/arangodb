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

#ifndef ARANGOD_AQL_EXECUTION_TIMING_H
#define ARANGOD_AQL_EXECUTION_TIMING_H 1

namespace arangodb {
namespace aql {

/// @brief returns the current value of the steady clock.
/// note that values produced by this function are not necessarily
/// identical to unix timestamps, and are thus not meaningful by themselves. 
/// they are only meaningful to measure time differences, i.e. when 
/// subtracting two of this function's return values from another.
/// the values returned by this function are monotonically increasing,
/// but not necessarily strictly monotonically increasing.
double currentSteadyClockValue();

/// @brief returns the elapsed time (in seconds) since the previous
/// value, which must have been produced by a call to currentSteadyClockValue().
double elapsedSince(double previous);

}  // namespace aql
}  // namespace arangodb

#endif
