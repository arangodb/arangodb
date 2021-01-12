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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_EXECUTION_STATE_H
#define ARANGOD_AQL_EXECUTION_STATE_H 1

#include <iosfwd>

namespace arangodb {
namespace aql {

enum class ExecutionState {
  // done with this block, definitely no more results
  DONE,
  // (potentially) more results available. this may "lie" and
  // report that there are more results when in fact there are
  // none (background: to accurately determine that there are
  // more results we may need to execute expensive operations
  // on the preceeding blocks, which we want to avoid)
  HASMORE,
  // unclear if more results available or not. caller is asked
  // to try again
  WAITING
};

enum class ExecutorState {
  // done with this block, definitely no more results
  DONE,
  // (potentially) more results available. this may "lie" and
  // report that there are more results when in fact there are
  // none (background: to accurately determine that there are
  // more results we may need to execute expensive operations
  // on the preceeding blocks, which we want to avoid)
  HASMORE
};

std::ostream& operator<<(std::ostream& ostream, ExecutionState state);

std::ostream& operator<<(std::ostream& ostream, ExecutorState state);

}  // namespace aql
}  // namespace arangodb
#endif
