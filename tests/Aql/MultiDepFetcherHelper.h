////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef TESTS_AQL_MULTIDEPFETCHERHELPER_H
#define TESTS_AQL_MULTIDEPFETCHERHELPER_H

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutionStats.h"
#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SharedAqlItemBlockPtr.h"

#include <boost/variant.hpp>

#include <tuple>

namespace arangodb {
namespace tests {
namespace aql {

// std::pair<ExecutionState, size_t> preFetchNumberOfRows(size_t atMost)
struct PrefetchNumberOfRows {
  using Result = std::pair<arangodb::aql::ExecutionState, size_t>;
  size_t atMost;
};

// std::pair<ExecutionState, InputAqlItemRow> fetchRowForDependency(size_t dependency, size_t atMost)
struct FetchRowForDependency {
  using Result = std::pair<arangodb::aql::ExecutionState, arangodb::aql::InputAqlItemRow>;
  size_t dependency;
  size_t atMost;
};

// std::pair<ExecutionState, size_t> skipRowsForDependency(size_t dependency, size_t atMost);
struct SkipRowsForDependency {
  using Result = std::pair<arangodb::aql::ExecutionState, size_t>;
  size_t dependency;
  size_t atMost;
};

// std::pair<ExecutionState, ShadowAqlItemRow> fetchShadowRow(size_t atMost);
struct FetchShadowRow {
  using Result = std::pair<arangodb::aql::ExecutionState, arangodb::aql::ShadowAqlItemRow>;
  size_t atMost;
};

template <class FetcherCallT>
using ConcreteFetcherIOPair = std::pair<FetcherCallT, typename FetcherCallT::Result>;

using FetcherIOPair =
    boost::variant<ConcreteFetcherIOPair<PrefetchNumberOfRows>, ConcreteFetcherIOPair<FetchRowForDependency>,
                   ConcreteFetcherIOPair<SkipRowsForDependency>, ConcreteFetcherIOPair<FetchShadowRow>>;

void runFetcher(arangodb::aql::MultiDependencySingleRowFetcher& testee,
                std::vector<FetcherIOPair> const& inputOutputPairs);

}  // namespace aql
}  // namespace tests
}  // namespace arangodb

#endif  // TESTS_AQL_MULTIDEPFETCHERHELPER_H
