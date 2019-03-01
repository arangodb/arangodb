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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_UNSORTING_GATHER_EXECUTOR_H
#define ARANGOD_AQL_UNSORTING_GATHER_EXECUTOR_H

#include "Aql/ExecutionState.h"

namespace arangodb {
namespace aql {

class MultiDependencySingleRowFetcher;
class NoStats;
class ExecutorInfos;
class OutputAqlItemRow;

class UnsortingGatherExecutor {
 public:
  struct Properties {
    static const bool preservesOrder = false;
    static const bool allowsBlockPassthrough = false;
  };

  using Fetcher = MultiDependencySingleRowFetcher;
  using Infos = ExecutorInfos;
  using Stats = NoStats;

  UnsortingGatherExecutor(Fetcher& fetcher, Infos& unused);
  ~UnsortingGatherExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

 private:
  Fetcher& _fetcher;
  size_t _currentDependency;
  size_t _numberDependencies;
};

}  // namespace aql
}  // namespace arangodb

#endif