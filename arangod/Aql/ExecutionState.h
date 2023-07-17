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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <fmt/format.h>

#include <iosfwd>

namespace arangodb::aql {

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
  // done with this subquery, no more results to be expected
  DONE,
  // (potentially) more results available. this may "lie" and
  // report that there are more results when in fact there are
  // none (background: to accurately determine that there are
  // more results we may need to execute expensive operations
  // on the preceding blocks, which we want to avoid)
  HASMORE
};

//@ brief Like the Executor state but for the Global State of the query
// The values on this enum are on purpose identical to the ExecutorState,
// as they can actually only get into the same States. How ever
// they are used in different situations, and we want compilers help
// to ensure we have not accidentally mixed them up.
enum class MainQueryState {
  // done with the main query. As soon as we reached DONE
  // state here, we cannot switch back to HASMORE. On upstream
  // All pieces of the query have been completed.
  DONE,
  // (potentially) more results available. this may "lie" and
  // report that there are more results when in fact there are
  // none (background: to accurately determine that there are
  // more results we may need to execute expensive operations
  // on the preceding blocks, which we want to avoid)
  HASMORE
};

auto toStringView(ExecutionState state) -> std::string_view;

auto toStringView(ExecutorState state) -> std::string_view;

std::ostream& operator<<(std::ostream& ostream, ExecutionState state);

std::ostream& operator<<(std::ostream& ostream, ExecutorState state);

}  // namespace arangodb::aql

template<>
struct fmt::formatter<::arangodb::aql::ExecutionState>
    : formatter<std::string_view> {
  // parse is inherited from formatter<string_view>.
  template<class FormatContext>
  auto format(::arangodb::aql::ExecutionState state, FormatContext& fc) const {
    auto view = toStringView(state);

    return formatter<std::string_view>::format(view, fc);
  }
};

template<>
struct fmt::formatter<::arangodb::aql::ExecutorState>
    : formatter<std::string_view> {
  // parse is inherited from formatter<string_view>.
  template<class FormatContext>
  auto format(::arangodb::aql::ExecutorState state, FormatContext& fc) const {
    auto view = toStringView(state);

    return formatter<std::string_view>::format(view, fc);
  }
};