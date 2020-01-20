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

#ifndef ARANGOD_AQL_ENUMERATE_EXECUTOR_H
#define ARANGOD_AQL_ENUMERATE_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/types.h"

#include <memory>
#include <unordered_set>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

struct AqlCall;
class AqlItemBlockInputRange;
class ExecutorInfos;
class OutputAqlItemRow;
class NoStats;
template <BlockPassthrough>
class SingleRowFetcher;

class EnumerateListExecutorInfos : public ExecutorInfos {
 public:
  // cppcheck-suppress passedByValue
  EnumerateListExecutorInfos(RegisterId inputRegister, RegisterId outputRegister,
                             RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                             std::unordered_set<RegisterId> registersToClear,
                             std::unordered_set<RegisterId> registersToKeep);

  EnumerateListExecutorInfos() = delete;
  EnumerateListExecutorInfos(EnumerateListExecutorInfos&&) = default;
  EnumerateListExecutorInfos(EnumerateListExecutorInfos const&) = delete;
  ~EnumerateListExecutorInfos() = default;

  RegisterId getInputRegister() const noexcept;
  RegisterId getOutputRegister() const noexcept;

 private:
  // These two are exactly the values in the parent members
  // ExecutorInfo::_inRegs and ExecutorInfo::_outRegs, respectively
  // getInputRegisters() and getOutputRegisters().
  RegisterId _inputRegister;
  RegisterId _outputRegister;
};

/**
 * @brief Implementation of Filter Node
 */
class EnumerateListExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = EnumerateListExecutorInfos;
  using Stats = NoStats;

  EnumerateListExecutor(Fetcher& fetcher, EnumerateListExecutorInfos&);
  ~EnumerateListExecutor() = default;

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputRange& input, OutputAqlItemRow& output);

 private:
  AqlValue getAqlValue(AqlValue const& inVarReg, size_t const& pos, bool& mustDestroy);
  void initialize();

 private:
  EnumerateListExecutorInfos& _infos;
  Fetcher& _fetcher;
  InputAqlItemRow _currentRow;
  ExecutionState _rowState;
  size_t _inputArrayPosition;
  size_t _inputArrayLength;
  size_t _produced;
};

}  // namespace aql
}  // namespace arangodb

#endif
