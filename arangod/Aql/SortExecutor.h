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


#ifndef ARANGOD_AQL_SORT_EXECUTOR_H
#define ARANGOD_AQL_SORT_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include <memory>

namespace arangodb {
namespace aql {

class AqlItemRow;
class ExecutorInfos;
class AllRowsFetcher;

/**
 * @brief Implementation of Sort Node
 */
class SortExecutor {
 public:
  SortExecutor(AllRowsFetcher& fetcher, ExecutorInfos&);
  ~SortExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  ExecutionState produceRow(AqlItemRow& output);

 private:
  AllRowsFetcher& _fetcher;

  ExecutorInfos& _infos;
};
}  // namespace aql
}  // namespace arangodb

#endif
