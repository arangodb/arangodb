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

#ifndef ARANGOD_AQL_FILTER_EXECUTOR_H
#define ARANGOD_AQL_FILTER_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Aql/types.h"

#include <memory>

namespace arangodb {
namespace aql {

class InputAqlItemRow;
class ExecutorInfos;
template <bool>
class SingleRowFetcher;

class FilterExecutorInfos : public ExecutorInfos {
 public:
  FilterExecutorInfos(RegisterId inputRegister, RegisterId nrInputRegisters,
                      RegisterId nrOutputRegisters,
                      std::unordered_set<RegisterId> registersToClear,
                      std::unordered_set<RegisterId> registersToKeep);

  FilterExecutorInfos() = delete;
  FilterExecutorInfos(FilterExecutorInfos&&) = default;
  FilterExecutorInfos(FilterExecutorInfos const&) = delete;
  ~FilterExecutorInfos() = default;

  RegisterId getInputRegister() const noexcept { return _inputRegister; }

 private:
  // This is exactly the value in the parent member ExecutorInfo::_inRegs,
  // respectively getInputRegisters().
  RegisterId _inputRegister;
};

/**
 * @brief Implementation of Filter Node
 */
class FilterExecutor {
 public:
  struct Properties {
    static const bool preservesOrder = true;
    static const bool allowsBlockPassthrough = false;
    static const bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = FilterExecutorInfos;
  using Stats = FilterStats;

  FilterExecutor() = delete;
  FilterExecutor(FilterExecutor&&) = default;
  FilterExecutor(FilterExecutor const&) = delete;
  FilterExecutor(Fetcher& fetcher, Infos&);
  ~FilterExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

  std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t atMost) const;

 private:
  Infos& _infos;
  Fetcher& _fetcher;
};

}  // namespace aql
}  // namespace arangodb

#endif
