////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_EMPTY_TEST_EXECUTOR_H
#define ARANGOD_AQL_EMPTY_TEST_EXECUTOR_H

#include "Aql/EmptyExecutorInfos.h"
#include "Aql/ExecutionState.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/Stats.h"
#include "Aql/types.h"

#include <memory>

namespace arangodb {
namespace aql {

class InputAqlItemRow;
template <BlockPassthrough>
class SingleRowFetcher;

class TestEmptyExecutorHelper {
 public:
  struct Properties {
    static const bool preservesOrder = true;
    static const BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static const bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = EmptyExecutorInfos;
  using Stats = FilterStats;

  TestEmptyExecutorHelper() = delete;
  TestEmptyExecutorHelper(TestEmptyExecutorHelper&&) = default;
  TestEmptyExecutorHelper(TestEmptyExecutorHelper const&) = delete;
  TestEmptyExecutorHelper(Fetcher&, Infos&);
  ~TestEmptyExecutorHelper() = default;

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);
};

}  // namespace aql
}  // namespace arangodb

#endif
